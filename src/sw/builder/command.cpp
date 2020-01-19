// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#define BOOST_THREAD_PROVIDES_EXECUTORS
#define BOOST_THREAD_PROVIDES_VARIADIC_THREAD
#define BOOST_THREAD_VERSION 5
#include "command.h"

#include "command_storage.h"
#include "file.h"
#include "file_storage.h"
#include "jumppad.h"
#include "os.h"
#include "program.h"
#include "sw_context.h"

#include <sw/support/filesystem.h>
#include <sw/support/hash.h>

#include <boost/algorithm/string.hpp>
#include <boost/dll.hpp>
#include <boost/thread/thread_pool.hpp>
#include <primitives/debug.h>
#include <primitives/executor.h>
#include <primitives/symbol.h>
#include <primitives/templates.h>
#include <primitives/sw/cl.h>
#include <primitives/sw/settings_program_name.h>

#include <regex>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "command");

//
static cl::opt<bool> save_failed_commands("save-failed-commands");
static cl::alias sfc("sfc", cl::desc("Alias for -save-failed-commands"), cl::aliasopt(save_failed_commands));

static cl::opt<bool> save_all_commands("save-all-commands");
static cl::alias sac("sac", cl::desc("Alias for -save-all-commands"), cl::aliasopt(save_all_commands));

static cl::opt<bool> save_executed_commands("save-executed-commands");
static cl::alias sec("sec", cl::desc("Alias for -save-executed-commands"), cl::aliasopt(save_executed_commands));

//
static cl::opt<bool> explain_outdated("explain-outdated", cl::desc("Explain outdated commands"));
static cl::opt<bool> explain_outdated_full("explain-outdated-full", cl::desc("Explain outdated commands with more info"));

static cl::opt<String> save_command_format("save-command-format", cl::desc("Explicitly set saved command format (bat or sh)"));

namespace sw
{

CommandNode::CommandNode()
{
}

CommandNode::CommandNode(const CommandNode &)
{
}

CommandNode &CommandNode::operator=(const CommandNode &)
{
    return *this;
}

CommandNode::~CommandNode()
{
}

namespace builder
{

Command::Command(const SwBuilderContext &swctx)
    : swctx(&swctx)
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
    String s = c.getName() + ", " + std::to_string(c.getHash()) + ", # of arguments " + std::to_string(c.arguments.size());
    if (explain_outdated_full)
    {
        s += "\n";
        s += "bdir: " + normalize_path(c.working_directory) + "\n";
        s += "env:\n";
        for (auto &[k, v] : c.environment)
            s += k + "\n" + v + "\n";
        for (auto &a : c.arguments)
            s += a->toString() + "\n";
        s.resize(s.size() - 1);
    }
    return s;
}

bool Command::check_if_file_newer(const path &p, const String &what, bool throw_on_missing) const
{
    auto s = File(p, getContext().getFileStorage()).isChanged(mtime, throw_on_missing);
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

    if (!command_storage)
    {
        if (isExplainNeeded())
            EXPLAIN_OUTDATED("command", true, "command storage is disabled", getCommandId(*this));
        return true;
    }

    auto k = getHash();
    auto r = command_storage->insert(k);
    if (r.second)
    {
        // we have insertion, no previous value available
        // so outdated
        if (isExplainNeeded())
            EXPLAIN_OUTDATED("command", true, "new command (command_storage = " + normalize_path(command_storage->root) + "): " + print(), getCommandId(*this));
        return true;
    }
    else
    {
        ((Command*)(this))->mtime = r.first->mtime;
        ((Command*)(this))->implicit_inputs = r.first->getImplicitInputs(command_storage->getInternalStorage());
        return isTimeChanged();
    }
}

bool Command::isTimeChanged() const
{
    try
    {
        return std::any_of(inputs.begin(), inputs.end(), [this](const auto &i) {
                   return check_if_file_newer(i, "input", true);
               }) ||
               std::any_of(outputs.begin(), outputs.end(), [this](const auto &i) {
                   return check_if_file_newer(i, "output", false);
               }) ||
               std::any_of(implicit_inputs.begin(), implicit_inputs.end(), [this](const auto &i) {
                   return check_if_file_newer(i, "implicit input", true);
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
    hash_combine(h, std::hash<path>()(getProgram()));

    // must sort arguments first
    // because some command may generate args in unspecified order
    std::set<String> args_sorted;

    Strings sa;
    for (auto &a : arguments)
        args_sorted.insert(a->toString());

    for (auto &a : args_sorted)
        hash_combine(h, std::hash<String>()(a));

    // redirections are also considered as arguments
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

    // command may depend on files not listed on the command line (dlls)?
    //for (auto &i : inputs)
        //hash_combine(h, std::hash<path>()(i));

    return h;
}

size_t Command::getHashAndSave() const
{
    return hash = getHash();
}

void Command::clean() const
{
    error_code ec;
    //for (auto &o : intermediate)
        //fs::remove(o, ec);
    for (auto &o : outputs)
        fs::remove(o, ec);
}

void Command::addInput(const path &p)
{
    if (p.empty())
        return;
    inputs.insert(p);
}

void Command::addInput(const Files &files)
{
    for (auto &f : files)
        addInput(f);
}

void Command::addImplicitInput(const path &p)
{
    if (p.empty())
        return;
    implicit_inputs.insert(p);
}

void Command::addImplicitInput(const Files &files)
{
    for (auto &f : files)
        addImplicitInput(f);
}

void Command::addOutput(const path &p)
{
    if (p.empty())
        return;
    outputs.insert(p);
    File(p, getContext().getFileStorage()).setGenerator(std::static_pointer_cast<Command>(shared_from_this()), true);
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
    for (auto &p : inputs)
    {
        File f(p, getContext().getFileStorage());
        if (f.isGenerated())
            dependencies.insert(f.getGenerator());
    }
}

path detail::ResolvableCommand::resolveProgram(const path &in) const
{
    return resolveExecutable(in);
}

void Command::prepare()
{
    if (prepared)
        return;

    // user entered commands may be in form 'git'
    // so, it is not empty, not generated and does not exist
    if (!getProgram().empty() && !File(getProgram(), getContext().getFileStorage()).isGeneratedAtAll() &&
        !path(getProgram()).is_absolute() && !fs::exists(getProgram()))
    {
        auto new_prog = resolveExecutable(getProgram());
        if (new_prog.empty())
            throw SW_RUNTIME_ERROR("passed program '" + getProgram() + "' is not resolved (missing): " + getCommandId(*this));
        setProgram(new_prog);
    }

    // extra check
    //if (!program)
        //throw SW_RUNTIME_ERROR("empty program: " + getCommandId(*this));

    // program is an input!
    inputs.insert(getProgram());

    // stable sort args by their positions

    getHashAndSave();

    // add more deps
    addInputOutputDeps();

    // late add real generator
    for (auto &p : outputs)
    {
        // there must be no error, because previous generator == this
        File(p, getContext().getFileStorage()).setGenerator(std::static_pointer_cast<Command>(shared_from_this()), false);
    }

    prepared = true;

    if (prev)
    {
        if (auto c = static_cast<Command *>(prev))
        {
            c->prepare();
            // take only deps from prev command
            dependencies.insert(c->dependencies.begin(), c->dependencies.end());
        }
    }
    if (next)
    {
        if (auto c = static_cast<Command*>(next))
            c->prepare();
    }
}

void Command::execute()
{
    if (command_storage)
        command_storage->add_user();

    auto sg = SCOPE_EXIT_NAMED
    {
        // free cs to release files (logs, cs)
        if (command_storage)
            command_storage->free_user();
    };

    if (!beforeCommand())
        return;

    execute1(); // main call

    // when we are here, we disable free_user() call,
    // because it will be called in afterCommand()
    sg.dismiss();
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
    // if you want to run command always and 'command_storage == nullptr'
    // also set 'always = true'!
    if (!always && !command_storage)
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

    // check our resources (before log)
    if (pool)
        pool->lock();
    SCOPE_EXIT
    {
        if (pool)
        pool->unlock();
    };

    printLog();
    return true;
}

void Command::afterCommand()
{
    //if (always)
        //return;

    // update things

    for (auto &i : inputs)
    {
        File f(i, getContext().getFileStorage());
        auto &fr = f.getFileData();
        mtime = std::max(mtime, fr.last_write_time);
    }

    for (auto &i : outputs)
    {
        File f(i, getContext().getFileStorage());
        auto &fr = f.getFileData();
        fr.refreshed = FileData::RefreshType::Unrefreshed;
        f.isChanged();
        if (!fs::exists(i))
        {
            auto e = "Output file was not created: " + normalize_path(i) + "\n" + getError();
            throw SW_RUNTIME_ERROR(makeErrorString(e));
        }
        mtime = std::max(mtime, fr.last_write_time);
    }

    if (!command_storage)
        return;

    // probably below is wrong, async writes are queue to one thread (FIFO)
    // so, deps are written first, only then command goes

    // FIXME: rare race is possible here
    // When command is written before all files above
    // and those files failed (deps write failed).
    // On the next run command times won't be compared with missing deps,
    // so outdated command wil not be re-runned

    auto k = getHash();
    auto &r = *command_storage->insert(k).first;
    r.hash = k;
    r.mtime = mtime;
    r.setImplicitInputs(implicit_inputs, command_storage->getInternalStorage());
    command_storage->async_command_log(r);
}

path Command::getResponseFilename() const
{
    return unique_path() += ".rsp";
}

String Command::getResponseFileContents(bool showIncludes) const
{
    String rsp;
    for (auto a = arguments.begin() + getFirstResponseFileArgument(); a != arguments.end(); a++)
    {
        if (!showIncludes && (*a)->toString() == "-showIncludes")
            continue;
        rsp += (*a)->quote(protect_args_with_quotes ? QuoteType::SimpleAndEscape : QuoteType::Escape);
        rsp += "\n";
    }
    if (!rsp.empty())
        rsp.resize(rsp.size() - 1);
    return rsp;
}

int Command::getFirstResponseFileArgument() const
{
    static const auto n_program_args = 1;
    return first_response_file_argument + n_program_args;
}

Command::Arguments &Command::getArguments()
{
    if (rsp_args.empty())
        return Base::getArguments();
    return rsp_args;
}

const Command::Arguments &Command::getArguments() const
{
    if (rsp_args.empty())
        return Base::getArguments();
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

        for (int i = 0; i < getFirstResponseFileArgument(); i++)
            rsp_args.push_back(arguments[i]->clone());
        rsp_args.push_back("@" + rsp_file.u8string());
    }

    SCOPE_EXIT
    {
        if (!rsp_file.empty())
            fs::remove(rsp_file);
    };

    auto make_error_string = [this]()
    {
        postProcess(false);
        printOutputs();

        return makeErrorString();
    };

    // create generated dirs
    for (auto &d : getGeneratedDirs())
        fs::create_directories(d);

    LOG_TRACE(logger, print());

    if (ec)
    {
        Base::execute(*ec);
        if (ec)
        {
            // TODO: save error string
            make_error_string();
            return;
        }
    }
    else
    {
        std::error_code ec;
        Base::execute(ec);
        if (ec)
        {
            auto err = make_error_string();
            throw SW_RUNTIME_ERROR(err);
        }
    }

    if (save_executed_commands || save_all_commands)
    {
        saveCommand();
    }

    postProcess(); // process deps
    printOutputs();
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
    if (s.empty())
        return;
    s = log_string + "\n" + s;
    boost::trim(s);
    if (write_output_to_file)
        write_file(fs::current_path() / SW_BINARY_DIR / "rsp" / std::to_string(getHash()) += ".txt", s);
    else
        LOG_INFO(logger, s);
}

String Command::makeErrorString()
{
    auto err = "command failed"s;
    auto errors = getErrors();
    if (errors.empty())
        return makeErrorString(err);

    err += ": ";
    for (auto &e : errors)
        err += e + ", ";
    err.resize(err.size() - 2);
    return makeErrorString(err);
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
    boost::trim(s);
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
    //s += "pid = " + std::to_string(pid) + "\n";
    s += "command is copied to " + p.u8string() + "\n";

    return s;
}

path Command::writeCommand(const path &p) const
{
    auto pbat = p;
    String t;

    bool bat = getHostOS().getShellType() == ShellType::Batch && !::sw::detail::isHostCygwin();
    if (!save_command_format.empty())
    {
        if (save_command_format == "bat")
            bat = true;
        else if (save_command_format == "sh")
            bat = false;
        else
            LOG_ERROR(logger, "Unknown save_command_format");
    }

    auto norm = [bat](const auto &s)
    {
        if (bat)
            return normalize_path_windows(s);
        return normalize_path(s);
    };

    // start
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

    /*if (bat)
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
    }*/

    // name
    t += "echo " + getName() + "\n\n";

    // env
    auto print_env = [&t, &bat](const auto &env)
    {
        for (auto &[k, v] : env)
        {
            if (bat)
                t += "set ";
            else
                t += "export ";
            t += k + "=";
            if (!bat)
                t += "\"";
            t += v;
            if (!bat)
                t += "\"";
            t += "\n";
        }
        return !env.empty();
    };
    if (auto p = getFirstCommand())
    {
        bool e = false;
        while (p)
        {
            e |= print_env(p->environment);
            p = p->next;
        }
        if (e)
            t += "\n";
    }
    else
    {
        if (print_env(environment))
            t += "\n";
    }

    // wdir
    if (!working_directory.empty())
        t += "cd " + norm(working_directory) + "\n\n";

    bool need_rsp = needsResponseFile();
    if (getHostOS().is(OSType::Windows))
        need_rsp = needsResponseFile(6'000);

    if (need_rsp)
    {
        auto rsp_name = path(p) += ".rsp";
        write_file(rsp_name, getResponseFileContents());

        for (int i = 0; i < getFirstResponseFileArgument(); i++)
            t += arguments[i]->quote() + " ";
        t += "\"@" + normalize_path(rsp_name) + "\" ";
    }
    else
    {
        static const String next_line_space = "    ";
        static const String next_line = "\n" + next_line_space;
        static const String bat_next_line = "^" + next_line;

        auto print_args = [&bat, &t](const auto &args)
        {
            for (auto &a : args)
            {
                if (a->toString() == "-showIncludes")
                    continue;
                auto a2 = a->quote(QuoteType::Escape);
                if (bat)
                {
                    // move to quote with escape?
                    // see https://www.robvanderwoude.com/escapechars.php
                    boost::replace_all(a2, "%", "%%"); // remove? because we always have double quotes
                }
                t += "\"" + a2 + "\" ";
                if (bat)
                    t += "^";
                else
                    t += "\\";
                t += next_line;
            }
            if (!args.empty())
            {
                t.resize(t.size() - 1 - next_line.size());
            }
        };

        if (auto p = getFirstCommand())
        {
            while (p)
            {
                print_args(p->arguments);
                p = p->next;
                if (p)
                {
                    if (bat)
                        t += "^";
                    else
                        t += "\\";
                    t += "\n | ";
                    if (!bat)
                        t += "\\\n";
                }
            }
        }
        else
            print_args(arguments);
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
    implicit_inputs.clear();

    postProcess1(ok);
}

bool Command::needsResponseFile() const
{
    // we do not use system(), so we really do not care when using exec*
    // we care on windows where CreateProcess has limit of 32K in total

    static constexpr auto win_create_process_sz = 32'000; // win have 32K limit, we take a bit fewer symbols
    static constexpr auto win_console_sz = 8'100; // win have 8192 limit on console, we take a bit fewer symbols

    static constexpr auto win_sz = win_create_process_sz;
    static constexpr auto apple_sz = 260'000;
    static constexpr auto nix_sz = 2'000'000; // something near 2M

    const auto selected_size =
#ifdef _WIN32
        win_sz
        // do not use for now
//#elif __APPLE__
        // apple_sz
#else
        nix_sz
#endif
        ;

    return needsResponseFile(selected_size);
}

bool Command::needsResponseFile(size_t selected_size) const
{
    // 3 = 1 + 2 = space + quotes
    size_t sz = getProgram().size() + 3;
    for (auto a = arguments.begin() + getFirstResponseFileArgument(); a != arguments.end(); a++)
        sz += (*a)->toString().size() + 3;

    if (use_response_files)
    {
        if (!*use_response_files)
        {
            if (sz > selected_size)
                LOG_WARN(logger, "Very long command line = " << sz << " and rsp files are disabled. Expect errors.");
        }
        return *use_response_files;
    }
    return sz > selected_size;
}

String Command::getName(bool short_name) const
{
    if (short_name)
    {
        if (name_short.empty())
        {
            if (!outputs.empty())
            {
                return normalize_path(*outputs.begin());
            }
            return std::to_string((uint64_t)this);
        }
        return name_short;
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
    return name;
}

void Command::printLog() const
{
    if (silent)
        return;
    static Executor eprinter(1);
    if (current_command)
    {
        eprinter.push([c = std::static_pointer_cast<const Command>(shared_from_this())]
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

void Command::setProgram(std::shared_ptr<Program> p)
{
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
    //for (auto &d : intermediate)
        //dirs.insert(get_parent(d));
    for (auto &d : outputs)
        dirs.insert(get_parent(d));
    for (auto &d : output_dirs)
    {
        if (!d.empty())
            dirs.insert(d);
    }
    return dirs;
}

bool Command::lessDuringExecution(const CommandNode &in) const
{
    // improve sorting! it's too stupid
    // simple "0 0 0 0 1 2 3 6 7 8 9 11" is not enough

    auto &rhs = (const Command &)in;

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

Command &Command::operator|(Command &c2)
{
    Base::operator|(c2);
    return *this;
}

Command &Command::operator|=(Command &c2)
{
    operator|(c2);
    return *this;
}

const SwBuilderContext &Command::getContext() const
{
    if (!swctx)
        throw SW_RUNTIME_ERROR("Empty sw context");
    return *swctx;
}

void Command::setContext(const SwBuilderContext &in)
{
    if (swctx)
        throw SW_RUNTIME_ERROR("Settings swctx twice");
    swctx = &in;
}

void CommandSequence::addCommand(const std::shared_ptr<Command> &c)
{
    commands.push_back(c);
}

void CommandSequence::execute1(std::error_code *ec)
{
    for (auto &c : commands)
    {
        c->always = true; // skip some checks
        c->execute();
    }
}

size_t CommandSequence::getHash1() const
{
    size_t h = 0;
    for (auto &c : commands)
        hash_combine(h, c->getHash());
    return h;
}

void CommandSequence::prepare()
{
    for (auto &c : commands)
        c->prepare();
}

ExecuteBuiltinCommand::ExecuteBuiltinCommand(const SwBuilderContext &swctx)
    : Command(swctx)
{
    setProgram(boost::dll::program_location().wstring());
}

ExecuteBuiltinCommand::ExecuteBuiltinCommand(const SwBuilderContext &swctx, const String &cmd_name, void *f, int version)
    : ExecuteBuiltinCommand(swctx)
{
    first_response_file_argument = 1;
    arguments.push_back(getInternalCallBuiltinFunctionName());
    arguments.push_back(normalize_path(primitives::getModuleNameForSymbol(f))); // add dependency on this? or on function (command) version
    arguments.push_back(cmd_name);
    arguments.push_back(std::to_string(version));
}

void ExecuteBuiltinCommand::push_back(const Files &files)
{
    arguments.push_back(std::to_string(files.size()));
    for (auto &o : FilesSorted(files.begin(), files.end()))
        arguments.push_back(normalize_path(o));
}

void ExecuteBuiltinCommand::push_back(const Strings &strings)
{
    arguments.push_back(std::to_string(strings.size()));
    for (auto &o : strings)
        arguments.push_back(normalize_path(o));
}

void ExecuteBuiltinCommand::execute1(std::error_code *ec)
{
    // add try catch?

    Strings sa;
    for (auto &a : arguments)
        sa.push_back(a->toString());

    auto start = getFirstResponseFileArgument();
    jumppad_call(
        sa[start + 0],
        sa[start + 1],
        std::stoi(sa[start + 2]),
        Strings{ sa.begin() + start + 3, sa.end() });
}

size_t ExecuteBuiltinCommand::getHash1() const
{
    size_t h = 0;
    // ignore program!

    auto start = getFirstResponseFileArgument();
    hash_combine(h, std::hash<String>()(arguments[start + 1]->toString())); // include function name
    hash_combine(h, std::hash<String>()(arguments[start + 2]->toString())); // include version

    // must sort arguments first
    // because some command may generate args in unspecified order
    std::set<String> args_sorted;

    Strings sa;
    for (auto &a : arguments)
        args_sorted.insert(a->toString());

    for (auto &a : args_sorted)
        hash_combine(h, std::hash<String>()(a));

    return h;
}

String getInternalCallBuiltinFunctionName()
{
    return "internal-call-builtin-function";
}

} // namespace builder

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
        c.setProgram(p_which);
        c.arguments.push_back(normalize_path(in));
        error_code ec;
        c.execute(ec);
        bool which = true;
        if (ec)
        {
            primitives::Command c;
            c.setProgram(p_where);
            c.arguments.push_back(normalize_path_windows(in));
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
                c2.setProgram("cygpath");
                c2.arguments.push_back("-w");
                c2.arguments.push_back(c.out.text);
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

std::map<path, String> &getMsvcIncludePrefixes()
{
    static std::map<path, String> prefixes;
    return prefixes;
}

String detectMsvcPrefix(builder::detail::ResolvableCommand c, const path &idir)
{
    auto &p = getMsvcIncludePrefixes();
    if (!p[c.getProgram()].empty())
        return p[c.getProgram()];

    String contents = "#include <iostream>\r\nint dummy;";
    auto fn = get_temp_filename("cliprefix") += ".cpp";
    auto obj = path(fn) += ".obj";
    write_file(fn, contents);
    c.push_back("/showIncludes");
    c.push_back("/c");
    c.push_back(fn);
    c.push_back("/Fo" + normalize_path_windows(obj));
    c.push_back("/I");
    c.push_back(idir);
    std::error_code ec;
    c.execute(ec);
    fs::remove(obj);
    fs::remove(fn);

    auto error = [&c](const String &reason)
    {
        return "Cannot match VS include prefix (" + reason + "):\n" + c.out.text + "\nstderr:\n" + c.err.text;
    };

    auto lines = split_lines(c.out.text);
    if (lines.size() < 2)
        throw SW_RUNTIME_ERROR(error("bad output"));

    static std::regex r(R"((.*\s)[a-zA-Z]:\\.*iostream)");
    std::smatch m;
    if (!std::regex_search(lines[1], m, r) &&
        !std::regex_search(lines[0], m, r) // clang-cl does not output filename
        )
        throw SW_RUNTIME_ERROR(error("regex_search failed"));
    return p[c.getProgram()] = m[1].str();
}

} // namespace sw
