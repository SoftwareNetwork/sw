// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#define BOOST_THREAD_PROVIDES_EXECUTORS
#define BOOST_THREAD_PROVIDES_VARIADIC_THREAD
#define BOOST_THREAD_VERSION 5
#include <sw/builder/command.h>

#include "command_storage.h"
#include "db.h"
#include "os.h"
#include "program.h"

#include <file_storage.h>
#include <hash.h>
#include <filesystem.h>
#include <primitives/context.h>
#include <primitives/debug.h>
#include <primitives/executor.h>
#include <primitives/templates.h>
#include <primitives/sw/cl.h>
#include <primitives/sw/settings_program_name.h>
#include <boost/algorithm/string.hpp>
#include <boost/thread/thread_pool.hpp>

#include <iostream>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "command");

static cl::opt<bool> save_failed_commands("save-failed-commands");
static cl::opt<bool> save_all_commands("save-all-commands");
static cl::opt<bool> save_executed_commands("save-executed-commands");
static cl::opt<bool> explain_outdated("explain-outdated", cl::desc("Explain outdated commands"));
static cl::opt<bool> explain_outdated_full("explain-outdated-full", cl::desc("Explain outdated commands with more info"));

namespace sw
{

SW_DEFINE_GLOBAL_STATIC_FUNCTION(CommandStorage, getCommandStorage)

static ConcurrentCommandStorage &getCommandStorage(bool local)
{
    return getCommandStorage().getStorage(local);
}

CommandStorage::CommandStorage()
{
    load();
}

CommandStorage::~CommandStorage()
{
    try
    {
        save();
    }
    catch (std::exception &e)
    {
        LOG_ERROR(logger, "Error during command db save: " << e.what());
    }
}

void CommandStorage::load()
{
    getDb().load(commands_local, true);
    getDb().load(commands_global, false);
}

void CommandStorage::save()
{
    getDb().save(commands_local, true);
    getDb().save(commands_global, false);
}

ConcurrentCommandStorage &CommandStorage::getStorage(bool local)
{
    return local ? commands_local : commands_global;
}

namespace builder
{

Command::Command()
{
}

Command::Command(::sw::FileStorage &fs)
    : fs(&fs)
{
}

Command::~Command()
{
}

static bool isExplainNeeded()
{
    return explain_outdated || explain_outdated_full;
}

static String getCommandId(const Command &c)
{
    String s = c.getName() + ", " + std::to_string(c.getHash()) + ", # of args " + std::to_string(c.args.size());
    if (explain_outdated_full)
    {
        s += "\n";
        s += "bdir: " + normalize_path(c.working_directory) + "\n";
        s += "env:\n";
        for (auto &[k, v] : c.environment)
            s += k + "\n" + v + "\n";
        s += normalize_path(c.program) + "\n";
        for (auto &a : c.args)
            s += a + "\n";
        s.resize(s.size() - 1);
    }
    return s;
}

bool Command::check_if_file_newer(const path &p, const String &what, bool throw_on_missing) const
{
    auto s = File(p, *fs).isChanged(mtime, throw_on_missing);
    if (s && isExplainNeeded())
        EXPLAIN_OUTDATED("command", true, what + " changed " + normalize_path(p) + ": " + *s, getCommandId(*this));
    return !!s;
}

bool Command::isOutdated() const
{
    if (always)
    {
        if (isExplainNeeded())
            EXPLAIN_OUTDATED("command", true, "always build", getCommandId(*this));
        return true;
    }

    if (command_storage == CS_DO_NOT_SAVE)
    {
        if (isExplainNeeded())
            EXPLAIN_OUTDATED("command", true, "command storage is disabled", getCommandId(*this));
        return true;
    }

    auto k = getHash();
    auto r = getCommandStorage(command_storage == CS_LOCAL).insert_ptr(k, 0);
    if (r.second)
    {
        // we have insertion, no previous value available
        // so outdated
        if (isExplainNeeded())
            EXPLAIN_OUTDATED("command", true, "new command: " + print(), getCommandId(*this));
        return true;
    }
    else
    {
        *((int64_t*)&mtime) = *r.first;
        return isTimeChanged();
    }
}

bool Command::isTimeChanged() const
{
    try
    {
        return check_if_file_newer(program, "program", true) ||
               std::any_of(inputs.begin(), inputs.end(), [this](const auto &i) {
                   return check_if_file_newer(i, "input", true);
               }) ||
               std::any_of(outputs.begin(), outputs.end(), [this](const auto &i) {
                   return check_if_file_newer(i, "output", false);
               });
    }
    catch (std::exception &e)
    {
        String s = "Command: " + getName() + "\n";
        s += e.what();
        throw SW_RUNTIME_ERROR(s);
    }
}

size_t Command::getHash() const
{
    if (hash != 0)
        return hash;
    return getHash1();
}

size_t Command::getHash1() const
{
    size_t h = 0;
    hash_combine(h, std::hash<path>()(program));

    // must sort args first, why?
    std::set<String> args_sorted(args.begin(), args.end());
    for (auto &a : args_sorted)
        hash_combine(h, std::hash<String>()(a));

    // redirections are also considered as args
    if (!in.file.empty())
        hash_combine(h, std::hash<path>()(in.file));
    if (!out.file.empty())
        hash_combine(h, std::hash<path>()(out.file));
    if (!err.file.empty())
        hash_combine(h, std::hash<path>()(err.file));

    hash_combine(h, std::hash<path>()(working_directory));

    // read other env vars? some of them may have influence
    for (auto &[k, v] : environment)
    {
        hash_combine(h, std::hash<String>()(k));
        hash_combine(h, std::hash<String>()(v));
    }

    return h;
}

size_t Command::getHashAndSave() const
{
    return hash = getHash();
}

void Command::updateCommandTime() const
{
    auto k = getHash();
    auto c = mtime.time_since_epoch().count();
    auto r = getCommandStorage(command_storage == CS_LOCAL).insert_ptr(k, c);
    if (!r.second)
        *r.first = c;
}

void Command::clean() const
{
    error_code ec;
    for (auto &o : intermediate)
        fs::remove(o, ec);
    for (auto &o : outputs)
        fs::remove(o, ec);
}

path Command::getProgram() const
{
    path p;
    /*if (base)
    {
        p = base->file;
        if (p.empty())
            throw SW_RUNTIME_ERROR("Empty program from base program");
    }
    else */if (!program.empty())
        p = program;
    else
        p = Base::getProgram();
    return p;
}

void Command::addInput(const path &p)
{
    if (p.empty())
        return;
    inputs.insert(p);
}

void Command::addIntermediate(const path &p)
{
    if (p.empty())
        return;
    intermediate.insert(p);
    //auto &r = File(p, *fs).getFileRecord();
    //r.setGenerator(shared_from_this());
}

void Command::addOutput(const path &p)
{
    if (p.empty())
        return;
    outputs.insert(p);
    auto &r = File(p, *fs).getFileRecord();
    //r.setGenerated(true);
    r.setGenerator(shared_from_this(), true);
}

void Command::addInput(const Files &files)
{
    for (auto &f : files)
        addInput(f);
}

void Command::addIntermediate(const Files &files)
{
    for (auto &f : files)
        addIntermediate(f);
}

void Command::addOutput(const Files &files)
{
    for (auto &f : files)
        addOutput(f);
}

path Command::redirectStdin(const path &p)
{
    in.file = p;
    addInput(p);
    return p;
}

path Command::redirectStdout(const path &p, bool append)
{
    out.file = p;
    out.append = append;
    addOutput(p);
    return p;
}

path Command::redirectStderr(const path &p, bool append)
{
    err.file = p;
    err.append = append;
    addOutput(p);
    return p;
}

void Command::addInputOutputDeps()
{
    if (File(program, *fs).isGenerated())
        dependencies.insert(File(program, *fs).getFileRecord().getGenerator());
    //inputs.insert(program);

    for (auto &p : inputs)
    {
        File f(p, *fs);
        if (f.isGenerated())
            dependencies.insert(f.getFileRecord().getGenerator());
        else
        {
            // do we really need this? yes!
            // if input's dep is generated, we add a dependency
            // no? cyclic deps become real
            //auto deps = f.gatherDependentGenerators();
            //dependencies.insert(deps.begin(), deps.end());
        }
    }
    // do we really need this?
    /*for (auto &p : outputs)
    {
        File f(p, *fs);
        f.addExplicitDependency(inputs);
        //f.addImplicitDependency(inputs);
    }*/
}

path detail::ResolvableCommand::resolveProgram(const path &in) const
{
    return resolveExecutable(in);
}

void Command::prepare()
{
    if (prepared)
        return;

    program = getProgram();

    // user entered commands may be in form 'git'
    // so, it is not empty, not generated and does not exist
    if (!program.empty() && !File(program, *fs).isGeneratedAtAll() && !program.is_absolute() && !fs::exists(program))
    {
        auto new_prog = resolveExecutable(program);
        if (new_prog.empty())
            throw SW_RUNTIME_ERROR("resolved program '" + program.u8string() + "' is empty: " + getCommandId(*this));

        program = new_prog;
    }

    // extra check
    if (program.empty())
        throw SW_RUNTIME_ERROR("empty program: " + getCommandId(*this));

    getHashAndSave();

    // add redirected generated files
    if (!out.file.empty())
        addOutput(out.file);
    if (!err.file.empty())
        addOutput(err.file);

    // add more deps
    addInputOutputDeps();

    // late add real generator
    for (auto &p : outputs)
    {
        auto &r = File(p, *fs).getFileRecord();
        r.setGenerator(shared_from_this(), false);
    }

    prepared = true;

    if (prev)
    {
        if (auto c = static_cast<Command*>(prev))
            c->prepare();
    }
    if (next)
    {
        if (auto c = static_cast<Command*>(next))
            c->prepare();
    }
}

void Command::execute()
{
    if (!beforeCommand())
        return;
    execute1(); // main thing
    afterCommand();
}

void Command::execute(std::error_code &ec)
{
    if (!beforeCommand())
        return;
    execute1(&ec); // main thing
    if (ec)
        return;
    afterCommand();
}

bool Command::beforeCommand()
{
    prepare();

    // check
    if (!always && command_storage == CS_UNDEFINED)
        throw SW_RUNTIME_ERROR(makeErrorString("command storage is not selected, call t.registerCommand(cmd)"));

    if (!isOutdated())
    {
        executed_ = true;
        (*current_command)++;
        return false;
    }

    if (isExecuted())
        throw std::logic_error("Trying to execute command twice: " + getName());

    executed_ = true;

    printLog();
    return true;
}

void Command::afterCommand()
{
    if (always)
        return;

    // update things

    auto update_time = [this](const auto &i)
    {
        File f(i, *fs);
        auto &fr = f.getFileRecord();
        fr.data->refreshed = FileData::RefreshType::Unrefreshed;
        fr.isChangedWithDeps();
        fs->async_file_log(&fr);
        //fr.writeToLog();
        //fr.updateLwt();
        if (!fs::exists(i))
        {
            auto e = "Output file was not created: " + normalize_path(i) + "\n" + getError();
            throw SW_RUNTIME_ERROR(makeErrorString(e));
        }
        //if (fr.data->last_write_time < start_time)
            //err
        mtime = std::max(mtime, fr.getMaxTime());
    };

    if (record_inputs_mtime)
    {
        mtime = std::max(mtime, File(program, *fs).getFileRecord().getMaxTime());
        for (auto &i : inputs)
            update_time(i);
    }
    //for (auto &i : intermediate)
        //update_time(i);
    for (auto &i : outputs)
        update_time(i);

    if (command_storage != CS_LOCAL && command_storage != CS_GLOBAL)
        return;

    updateCommandTime();

    // probably below is wrong, async writes are queue to one thread (FIFO)
    // so, deps are written first, only then command goes

    // FIXME: rare race is possible here
    // When command is written before all files above
    // and those files failed (deps write failed).
    // On the next run command times won't be compared with missing deps,
    // so outdated command wil not be re-runned

    fs->async_command_log(getHash(), mtime.time_since_epoch().count(), command_storage == CS_LOCAL);
}

path Command::getResponseFilename() const
{
    return unique_path() += ".rsp";
}

String Command::getResponseFileContents(bool showIncludes) const
{
    String rsp;
    for (auto a = args.begin() + first_response_file_argument; a != args.end(); a++)
    {
        if (!showIncludes && *a == "-showIncludes")
            continue;
        if (protect_args_with_quotes)
            rsp += "\"" + escape_cmd_arg(*a) + "\"";
        else
            rsp += escape_cmd_arg(*a);
        rsp += "\n";
    }
    if (!rsp.empty())
        rsp.resize(rsp.size() - 1);
    return rsp;
}

String Command::escape_cmd_arg(String s)
{
    boost::replace_all(s, "\\", "\\\\");
    boost::replace_all(s, "\"", "\\\"");
    return s;
}

Strings &Command::getArgs()
{
    if (rsp_args.empty())
        return Base::getArgs();
    return rsp_args;
}

void Command::execute1(std::error_code *ec)
{
    primitives::ScopedThreadName tn(": " + getName(), true);

    if (remove_outputs_before_execution)
    {
        // Some programs won't update their binaries even in case of updated sources/deps.
        // E.g., msvc bug: https://developercommunity.visualstudio.com/content/problem/97608/libexe-does-not-update-import-library.html
        error_code ec;
        for (auto &o : outputs)
            fs::remove(o, ec);
    }

    // check our resources
    if (pool)
        pool->lock();
    SCOPE_EXIT
    {
        if (pool)
            pool->unlock();
    };

    // Try to construct command line first.
    // Some systems have limitation on its length.

    path rsp_file;
    if (needsResponseFile())
    {
        auto t = temp_directory_path() / getResponseFilename();
        auto fn = t.filename();
        t = t.parent_path();
        rsp_file = t / getProgramName() / "rsp" / fn;
        write_file(rsp_file, getResponseFileContents(true));

        for (int i = 0; i < first_response_file_argument; i++)
            rsp_args.push_back(args[i]);
        rsp_args.push_back("@" + rsp_file.u8string());
    }

    SCOPE_EXIT
    {
        if (!rsp_file.empty())
            fs::remove(rsp_file);
    };

    auto make_error_string = [this](const String &e)
    {
        postProcess(false);
        printOutputs();

        return makeErrorString(e);
    };

    // create generated dirs
    for (auto &d : getGeneratedDirs())
        fs::create_directories(d);

    LOG_TRACE(logger, print());

    try
    {
        if (ec)
        {
            Base::execute(*ec);
            if (ec)
            {
                // TODO: save error string
                make_error_string(getError());
                return;
            }
        }
        else
            Base::execute();

        if (save_executed_commands || save_all_commands)
        {
            saveCommand();
        }

        postProcess(); // process deps
        printOutputs();
    }
    catch (std::exception &e)
    {
        auto err = make_error_string(e.what());
        throw SW_RUNTIME_ERROR(err);
    }
}

void Command::printOutputs()
{
    if (!show_output)
        return;
    boost::trim(out.text);
    boost::trim(err.text);
    String s;
    if (!out.text.empty())
        s += out.text + "\n";
    if (!err.text.empty())
        s += err.text + "\n";
    if (!s.empty())
    {
        s = log_string + "\n" + s;
        boost::trim(s);
        LOG_INFO(logger, s);
    }
}

String Command::makeErrorString(const String &e)
{
    String s = "When executing: " + getName();
    if (!out.text.empty())
    {
        boost::replace_all(out.text, "\r", "");
        s += "\n" + boost::trim_copy(out.text);
    }
    if (!err.text.empty())
    {
        boost::replace_all(err.text, "\r", "");
        s += "\n" + boost::trim_copy(err.text);
    }
    s += "\n";
    s += e;
    boost::trim(s);
    if (save_failed_commands || save_executed_commands || save_all_commands)
    {
        s += saveCommand();
    }
    return s;
}

String Command::saveCommand() const
{
    if (do_not_save_command)
        return String{};

    // use "fancy" rsp name = command hash
    auto p = fs::current_path() / SW_BINARY_DIR / "rsp" / (std::to_string(getHash()));
    p = writeCommand(p);

    String s;
    s += "\n";
    s += "pid = " + std::to_string(pid) + "\n";
    s += "command is copied to " + p.u8string() + "\n";

    return s;
}

path Command::writeCommand(const path &p) const
{
    auto pbat = p;
    String t;

    bool bat = getHostOS().getShellType() == ShellType::Batch && !::sw::detail::isHostCygwin();

    auto norm = [bat](const auto &s)
    {
        if (bat)
            return normalize_path_windows(s);
        return normalize_path(s);
    };

    if (bat)
        pbat += ".bat";
    else
        pbat += ".sh";

    if (bat)
    {
        t += "@echo off\n\n";
        t += "setlocal";
    }
    else
        t += "#!/bin/sh";
    t += "\n\n";

    if (bat)
        t += "::";
    else
        t += "#";
    t += " command: " + name + "\n\n";

    if (!name_short.empty())
    {
        if (bat)
            t += "::";
        else
            t += "#";
        t += " short name: " + name_short + "\n\n";
    }

    t += "echo " + getName() + "\n\n";

    for (auto &[k, v] : environment)
    {
        if (bat)
            t += "set ";
        t += k + "=";
        if (!bat)
            t += "\"";
        t += v;
        if (!bat)
            t += "\"";
        t += "\n\n";
    }

    if (!working_directory.empty())
        t += "cd " + norm(working_directory) + "\n\n";

    t += "\"" + norm(program) + "\" ";
    if (needsResponseFile())
    {
        auto rsp_name = path(p) += ".rsp";
        write_file(rsp_name, getResponseFileContents());

        for (int i = 0; i < first_response_file_argument; i++)
            t += args[i] + " ";
        t += "@" + normalize_path(rsp_name) + " ";
    }
    else
    {
        static const String bat_next_line = "^\n    ";
        if (!args.empty())
        {
            if (bat)
                t += bat_next_line;
        }
        for (auto &a : args)
        {
            if (a == "-showIncludes")
                continue;
            t += "\"" + escape_cmd_arg(a) + "\" ";
            if (bat)
                t += bat_next_line;
            else
                t += "\\\n\t";
        }
        if (!args.empty())
        {
            if (bat)
                t.resize(t.size() - bat_next_line.size());
            else
                t.resize(t.size() - 3);
        }
    }
    if (bat)
        t += "%";
    else
        t += "$";
    t += "*";

    if (!in.file.empty())
        t += " < " + norm(in.file);
    if (!out.file.empty())
        t += " > " + norm(out.file);
    if (!err.file.empty())
        t += " 2> " + norm(err.file);

    t += "\n";

    write_file(pbat, t);
    fs::permissions(pbat,
        fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
        fs::perm_options::add);

    return pbat;
}

void Command::postProcess(bool ok)
{
    // clear deps, otherwise they will stack up
    for (auto &f : outputs)
    {
        File f2(f, *fs);
        f2.clearImplicitDependencies();
        f2.addImplicitDependency(inputs);
    }

    postProcess1(ok);
}

bool Command::needsResponseFile() const
{
    static constexpr auto win_sz = 8'100; // win have 8192 limit, we take a bit fewer symbols
    static constexpr auto apple_sz = 260'000;
    static constexpr auto nix_sz = 8'100; // something near 2M

    // 3 = 1 + 2 = space + quotes
    size_t sz = program.u8string().size() + 3;
    for (auto a = args.begin() + first_response_file_argument; a != args.end(); a++)
        sz += a->size() + 3;

    if (use_response_files)
    {
        if (!*use_response_files)
        {
            if ((getHostOS().is(OSType::Windows) && sz > win_sz) ||
                (getHostOS().is(OSType::Macos) && sz > apple_sz))
                LOG_WARN(logger, "Very long command line = " << sz << " and rsp files are disabled. Expect errors.");
        }
        return *use_response_files;
    }
    return sz >
#ifdef _WIN32
        win_sz
// do not use for now
//#elif __APPLE__
        // apple_sz
#else
        nix_sz
#endif
        ;
}

String Command::getName(bool short_name) const
{
    if (short_name)
    {
        if (name_short.empty())
        {
            if (!outputs.empty())
            {
                return "\"" + normalize_path(*outputs.begin()) + "\"";
            }
            return std::to_string((uint64_t)this);
        }
        return "\"" + name_short + "\"";
    }
    if (name.empty())
    {
        if (!outputs.empty())
        {
            String s = "generate: ";
            for (auto &o : outputs)
                s += "\"" + normalize_path(o) + "\", ";
            s.resize(s.size() - 2);
            return s;
        }
        return std::to_string((uint64_t)this);
    }
    if (name[0] == '\"' && name.back() == '\"')
        return name;
    return "\"" + name + "\"";
}

void Command::printLog() const
{
    if (silent)
        return;
    static Executor eprinter(1);
    if (current_command)
    {
        eprinter.push([c = shared_from_this()]
        {
            c->log_string = "[" + std::to_string((*c->current_command)++) + "/" + std::to_string(c->total_commands->load()) + "] " + c->getName();

            // we cannot use this one because we must sync with printOutputs() call, both must use the same logger or (synced)stdout
            //std::cout << "\r" << log_string;

            // use this when logger won't call endl (custom sink is required)
            //LOG_INFO(logger, "\r" + log_string);

            LOG_INFO(logger, c->log_string);
        });
    }
}

void Command::setProgram(const path &p)
{
    program = p;
    //addInput(p);
}

void Command::setProgram(std::shared_ptr<Program> p)
{
    //base = p;
    if (p)
        setProgram(p->file);
}

Files Command::getGeneratedDirs() const
{
    // we do normalize path because it is possible to get bad paths
    // like c:\\dir1\\dir2/file.f
    // so parent_path() will give you bad parent
    // c:\\dir1 instead of c:\\dir1\\dir2

    auto get_parent = [](auto &p)
    {
//#ifdef _WIN32
        //return path(normalize_path(p)).parent_path();
//#else
        return p.parent_path();
//#endif
    };

    Files dirs;
    for (auto &d : intermediate)
        dirs.insert(get_parent(d));
    for (auto &d : outputs)
        dirs.insert(get_parent(d));
    for (auto &d : output_dirs)
    {
        if (!d.empty())
            dirs.insert(d);
    }
    return dirs;
}

static String getSystemRoot()
{
    static const String sr = []()
    {
        auto e = getenv("SystemRoot");
        if (!e)
            throw SW_RUNTIME_ERROR("getenv() failed");
        return e;
    }();
    return sr;
}

static String getSystemPath()
{
    // explicit! Path may be changed by IDE, other sources, so we keep it very small and very system
    static const String sp = []()
    {
        auto r = getSystemRoot();
        return r + "\\system32;" + r + ";" + r + "\\System32\\Wbem;" + r + "\\System32\\WindowsPowerShell\\v1.0\\";
    }();
    return sp;
}

void Command::addPathDirectory(const path &p)
{
#ifdef _WIN32
    static const auto env = "Path";
    static const auto delim = ";";
    auto norm = [](const auto &p) { return normalize_path_windows(p); };
#else
    static const auto env = "PATH";
    static const auto delim = ":";
    auto norm = [](const auto &p) { return p.u8string(); };
#endif

    if (environment[env].empty())
    {
#ifdef _WIN32
        environment[env] = getSystemPath();
#else
        auto e = getenv(env);
        if (!e)
            throw SW_RUNTIME_ERROR("getenv() failed");
        environment[env] = e;
#endif
    }
    environment[env] += delim + norm(p);
}

bool Command::lessDuringExecution(const Command &rhs) const
{
    // improve sorting! it's too stupid
    // simple "0 0 0 0 1 2 3 6 7 8 9 11" is not enough

    if (dependencies.size() != rhs.dependencies.size())
        return dependencies.size() < rhs.dependencies.size();
    if (strict_order && rhs.strict_order)
        return strict_order < rhs.strict_order;
    else if (strict_order)
        return true;
    else if (rhs.strict_order)
        return false;
    return dependent_commands.size() > dependent_commands.size();
}

void Command::onBeforeRun() noexcept
{
    tid = std::this_thread::get_id();
    t_begin = Clock::now();
}

void Command::onEnd() noexcept
{
    t_end = Clock::now();
}

/*void Command::load(BinaryContext &bctx)
{

}

void Command::save(BinaryContext &bctx)
{

}*/

Command &Command::operator|(Command &c2)
{
    Base::operator|(c2);
    return *this;
}

Command &Command::operator|=(Command &c2)
{
    Base::operator|=(c2);
    return *this;
}

}

// libuv cannot resolve /such/paths/on/cygwin, so we explicitly use which/where
path resolveExecutable(const path &in)
{
    if (in.empty())
        throw SW_RUNTIME_ERROR("empty input");

    if (in.is_absolute() && fs::exists(in))
        return in;

    if (auto p = primitives::resolve_executable(in); !p.empty())
        return p;

    // this is expensive resolve, so we cache

    static std::unordered_map<path, path> cache;
    static std::mutex m;

    {
        std::unique_lock lk(m);
        auto i = cache.find(in);
        if (i != cache.end())
            return i->second;
    }

    static const auto p_which = primitives::resolve_executable("which");
    static const auto p_where = primitives::resolve_executable("where");

    if (p_which.empty() && p_where.empty())
        throw SW_RUNTIME_ERROR("which and where were not found, cannot resolve executable: " + in.u8string());

    String result;

    // special cygwin resolving - no, common resolving?
    //if (getHostOS().Type == OSType::Cygwin)
    {
        // try to use which/where

        primitives::Command c;
        c.program = p_which;
        c.args.push_back(normalize_path(in));
        error_code ec;
        c.execute(ec);
        bool which = true;
        if (ec)
        {
            primitives::Command c;
            c.program = p_where;
            c.args.push_back(normalize_path_windows(in));
            c.execute(ec);
            which = false;
        }

        if (!ec)
        {
            boost::trim(c.out.text);

            // now run cygpath
            if (which && detail::isHostCygwin())
            {
                primitives::Command c2;
                c2.program = "cygpath";
                c2.args.push_back("-w");
                c2.args.push_back(c.out.text);
                c2.execute(ec);
                if (!ec)
                    result = boost::trim_copy(c2.out.text);
            }
            else
                result = c.out.text;
        }
    }

    std::unique_lock lk(m);
    cache[in] = result;

    return result;

    // remove this vvv
    // at the moment we also return empty string on error
    // "/usr/bin/which" is the single thing required to be absolute in code
    /*if (!p.empty())
        return p;
    return in;*/
}

path resolveExecutable(const FilesOrdered &paths)
{
    for (auto &p : paths)
    {
        auto e = resolveExecutable(p);
        if (!e.empty())
            return e;
    }
    return path();
}

}
