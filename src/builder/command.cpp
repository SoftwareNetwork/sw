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
#include <directories.h>
#include <filesystem.h>
#include <primitives/context.h>
#include <primitives/debug.h>
#include <primitives/executor.h>
#include <primitives/templates.h>
#include <primitives/sw/settings.h>
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

#ifdef _WIN32
#include <RestartManager.h>

#pragma comment(lib, "Rstrtmgr.lib")

// caller must close handles
static std::vector<HANDLE> getFileUsers(const path &fn)
{
    DWORD dwSession;
    WCHAR szSessionKey[CCH_RM_SESSION_KEY + 1] = { 0 };
    DWORD dwError = RmStartSession(&dwSession, 0, szSessionKey);
    if (dwError)
    {
        LOG_WARN(logger, "RmStartSession returned " << dwError);
        return {};
    }

    std::vector<HANDLE> handles;
    auto f = fn.wstring();
    PCWSTR pszFile = f.c_str();
    dwError = RmRegisterResources(dwSession, 1, &pszFile, 0, NULL, 0, NULL);
    if (dwError == ERROR_SUCCESS)
    {
        DWORD dwReason;
        UINT i;
        UINT nProcInfoNeeded;
        UINT nProcInfo = 10;
        std::vector<RM_PROCESS_INFO> rgpi(nProcInfo);
        dwError = RmGetList(dwSession, &nProcInfoNeeded, &nProcInfo, rgpi.data(), &dwReason);
        if (dwError == ERROR_MORE_DATA)
        {
            rgpi.resize(nProcInfoNeeded);
            dwError = RmGetList(dwSession, &nProcInfoNeeded, &nProcInfo, rgpi.data(), &dwReason);
        }
        if (dwError == ERROR_SUCCESS)
        {
            //LOG_WARN(logger, "RmGetList returned " << nProcInfo << " infos (" << nProcInfoNeeded << " needed)");
            for (i = 0; i < nProcInfo; i++)
            {
                //LOG_WARN(logger, i << ".ApplicationType = " << rgpi[i].ApplicationType);
                //LOG_WARN(logger, i << ".strAppName = " << rgpi[i].strAppName);
                //LOG_WARN(logger, i << ".Process.dwProcessId = " << rgpi[i].Process.dwProcessId);
                HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, rgpi[i].Process.dwProcessId);
                if (hProcess)
                    handles.push_back(hProcess);
            }
        }
        else
        {
            LOG_WARN(logger, "RmGetList returned " << dwError);
        }
    }
    else
    {
        LOG_WARN(logger, "RmRegisterResources(" << pszFile << ") returned " << dwError);
    }
    RmEndSession(dwSession);

    return handles;
}
#endif

namespace sw
{

SW_DEFINE_GLOBAL_STATIC_FUNCTION(CommandStorage, getCommandStorage)

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
    getDb().load(commands);
}

void CommandStorage::save()
{
    getDb().save(commands);
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
        for (auto &a : c.args)
            s += a + "\n";
        s.resize(s.size() - 1);
    }
    return s;
}

bool Command::check_if_file_newer(const path &p, const String &what) const
{
    auto s = File(p, *fs).isChanged(mtime);
    if (s && isExplainNeeded())
        EXPLAIN_OUTDATED("command", true, what + " changed " + normalize_path(p) + ": " + *s, getCommandId(*this));
    return !!s;
}

bool Command::isOutdated() const
{
    bool changed = false;

    auto k = getHash();
    auto r = getCommandStorage().commands.insert_ptr(k, 0);
    if (r.second)
    {
        // we have insertion, no previous value available
        // so outdated
        if (isExplainNeeded())
            EXPLAIN_OUTDATED("command", true, "new command: " + print(), getCommandId(*this));
        changed = true;
    }
    else
    {
        *((int64_t*)&mtime) = *r.first;
        changed |= isTimeChanged();
    }

    if (always)
    {
        if (isExplainNeeded())
            EXPLAIN_OUTDATED("command", true, "always build", getCommandId(*this));
        changed = true;
    }

    return changed;
}

bool Command::isTimeChanged() const
{
    bool changed = false;

    //DEBUG_BREAK_IF_STRING_HAS(outputs.begin()->string(), "lzma_encoder_optimum_normal.c.a27c4553.obj");

    /*if (outputs.size())
    {
        DEBUG_BREAK_IF_STRING_HAS(outputs.begin()->string(), "range.yy.cpp");
        DEBUG_BREAK_IF_STRING_HAS(outputs.begin()->string(), "range.yy.hpp");
    }*/

    // always check program and all deps are known
    changed |= check_if_file_newer(program, "program");
    for (auto &i : inputs)
        changed |= check_if_file_newer(i, "input");
    for (auto &i : outputs)
        changed |= check_if_file_newer(i, "output");

    return changed;
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
    auto r = getCommandStorage().commands.insert_ptr(k, c);
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

/*void Command::addPreciseInputOutputDependency(const path &in, const path &out)
{
    precise_input_output_deps[out].insert(in);
}*/

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

path Command::redirectStdout(const path &p)
{
    out.file = p;
    addOutput(p);
    return p;
}

path Command::redirectStderr(const path &p)
{
    err.file = p;
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
        program = resolveExecutable(program);

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
            throw SW_RUNTIME_ERROR("Output file was not created: " + normalize_path(i));
        mtime = std::max(mtime, fr.getMaxTime());
    };

    if (record_inputs_mtime)
    {
        mtime = std::max(mtime, File(program, *fs).getFileRecord().getMaxTime());
        for (auto &i : inputs)
            update_time(i);
    }
    if (0)
    for (auto &i : intermediate)
        update_time(i);
    for (auto &i : outputs)
        update_time(i);

    updateCommandTime();

    // probably below is wrong, async writes are queue to one thread (FIFO)
    // so, deps are written first, only then command goes

    // FIXME: rare race is possible here
    // When command is written before all files above
    // and those files failed (deps write failed).
    // On the next run command times won't be compared with missing deps,
    // so outdated command wil not be re-runned

    fs->async_command_log(getHash(), mtime.time_since_epoch().count());
}

path Command::getResponseFilename() const
{
    return unique_path() += ".rsp";
    //return std::to_string(getHash()) + ".rsp";
}

String Command::getResponseFileContents(bool showIncludes) const
{
    String rsp;
    for (auto &a : args)
    {
        if (!showIncludes && a == "-showIncludes")
            continue;
        if (protect_args_with_quotes)
            rsp += "\"" + escape_cmd_arg(a) + "\"";
        else
            rsp += escape_cmd_arg(a);
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

    auto make_rsp_file = [this](const auto &rsp_file, bool show_includes = true)
    {
        write_file(rsp_file, getResponseFileContents(show_includes));
    };

    path rsp_file;
    if (needsResponseFile())
    {
        auto t = temp_directory_path() / getResponseFilename();
        auto fn = t.filename();
        t = t.parent_path();
        rsp_file = t / getProgramName() / "rsp" / fn;
        make_rsp_file(rsp_file);
        rsp_args.push_back("@" + rsp_file.u8string());
    }

    SCOPE_EXIT
    {
        if (rsp_file.empty())
            return;

#ifdef _WIN32
        // sometimes rsp file is used by children of cl.exe, for example
        // fs::remove() fails in this case

        error_code ec;
        fs::remove(rsp_file, ec);
        if (ec)
        {
            auto processes = getFileUsers(rsp_file);
            if (!processes.empty())
            {
                if (WaitForMultipleObjects(processes.size(), processes.data(), TRUE, INFINITE) != WAIT_OBJECT_0)
                    LOG_WARN(logger, "Cannot remove rsp file: " << normalize_path(rsp_file) << " for pid = " << pid << ", WaitForMultipleObjects() failed: " << GetLastError());

                /*FILETIME ftCreate, ftExit, ftKernel, ftUser;
                if (GetProcessTimes(hProcess, &ftCreate, &ftExit, &ftKernel, &ftUser) &&
                    CompareFileTime(&rgpi[i].Process.ProcessStartTime, &ftCreate) == 0)
                {
                    WCHAR sz[MAX_PATH];
                    DWORD cch = MAX_PATH;
                    if (QueryFullProcessImageNameW(hProcess, 0, sz, &cch) && cch <= MAX_PATH)
                    {
                        LOG_WARN(logger, i << ".Process.image = " << sz);
                    }
                }*/
                for (auto h : processes)
                    CloseHandle(h);

                fs::remove(rsp_file);
            }
        }
#else
        fs::remove(rsp_file);
#endif
    };

    auto save_command = [this, &make_rsp_file]()
    {
        if (do_not_save_command)
            return String{};

        auto p = fs::current_path() / SW_BINARY_DIR / "rsp" / getResponseFilename();
        auto pbat = p;
        String t;

        String s;
        s += "\n";
        s += "pid = " + std::to_string(pid) + "\n";
        s += "command is copied to " + p.u8string() + "\n";

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

        for (auto &[k, v] : environment)
        {
            if (bat)
                t += "set ";
            t += k + "=" + v + "\n\n";
        }

        if (!working_directory.empty())
            t += "cd " + norm(working_directory) + "\n\n";

        t += "\"" + norm(program) + "\" ";
        if (!rsp_args.empty())
        {
            make_rsp_file(p, false);
            t += "@" + p.u8string() + " ";
        }
        else
        {
            for (auto &a : args)
            {
                if (a == "-showIncludes")
                    continue;
                t += "\"" + escape_cmd_arg(a) + "\" ";
                if (!bat)
                    t += "\\\n\t";
            }
            if (!bat && !args.empty())
                t.resize(t.size() - 3);
        }
        if (bat)
            t += "%";
        else
            t += "$";
        t += "* ";

        if (!in.file.empty())
            t += "< " + norm(in.file) + " ";
        if (!out.file.empty())
            t += "> " + norm(out.file) + " ";
        if (!err.file.empty())
            t += "2> " + norm(err.file) + " ";

        t += "\n";

        write_file(pbat, t);
        fs::permissions(pbat,
            fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
            fs::perm_options::add);

        return s;
    };

    auto print_outputs = [this]()
    {
        /*boost::trim(out.text);
        boost::trim(err.text);
        String s;
        if (!out.text.empty())
            s += out.text + "\n";
        if (!err.text.empty())
            s += err.text + "\n";
        if (!s.empty())
            LOG_INFO(logger, s);*/
    };

    auto make_error_string = [this, &save_command, &print_outputs](const String &e)
    {
        postProcess(false);
        print_outputs();

        String s = "When building: " + getName();
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
            s += save_command();
        return s;
    };

    // create generated dirs
    for (auto &d : getGeneratedDirs())
        fs::create_directories(d);

    //LOG_INFO(logger, print());
    LOG_TRACE(logger, print());

    try
    {
        if (ec)
        {
            Base::execute(*ec);
            if (ec)
            {
                // TODO: save error string
                make_error_string("FIXME");
                return;
            }
        }
        else
            Base::execute();

        if (save_executed_commands || save_all_commands)
            save_command();

        postProcess(); // process deps
        print_outputs();
    }
    catch (std::exception &e)
    {
        auto err = make_error_string(e.what());
        throw SW_RUNTIME_ERROR(err);
    }
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
    if (use_response_files)
        return true;
    // 3 = 1 + 2 = space + quotes
    size_t sz = program.u8string().size() + 3;
    for (auto &a : args)
        sz += a.size() + 3;
    return sz >
#ifdef _WIN32
        8100 // win have 8192 limit, we take a bit fewer symbols
#else
        8100
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
    //return "Building: \"" + (short_name ? name_short : name) + "\"";
    return "\"" + name + "\"";
}

void Command::printLog() const
{
    if (silent)
        return;
    static Executor eprinter(1);
    if (current_command)
    {
        std::string msg = "[" + std::to_string((*current_command)++) + "/" + std::to_string(total_commands->load()) + "] " + getName();
        eprinter.push([msg]
        {
            LOG_INFO(logger, msg);
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
    Files dirs;
    for (auto &d : intermediate)
        dirs.insert(d.parent_path());
    for (auto &d : outputs)
        dirs.insert(d.parent_path());
    return dirs;
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
        auto e = getenv(env);
        if (!e)
            throw SW_RUNTIME_ERROR("getenv() failed");
        environment[env] = e;
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
    return dependendent_commands.size() > dependendent_commands.size();
}

void Command::onBeforeRun()
{
    tid = std::this_thread::get_id();
    t_begin = Clock::now();
}

void Command::onEnd()
{
    t_end = Clock::now();
}

/*void Command::load(BinaryContext &bctx)
{

}

void Command::save(BinaryContext &bctx)
{

}*/

}

// libuv cannot resolve /such/paths/on/cygwin, so we explicitly use which/where
path resolveExecutable(const path &in)
{
    if (in.empty())
        throw SW_RUNTIME_ERROR("empty input");

    if (in.is_absolute())
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
        return {};

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
                //c.fs = &getFileStorage("service");
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
