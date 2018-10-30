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

namespace sw
{

CommandStorage &getCommandStorage()
{
    static CommandStorage cs;
    return cs;
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
    getDb().load(commands);
}

void CommandStorage::save()
{
    getDb().save(commands);
}

bool CommandStorage::isOutdated(const sw::builder::Command &c)
{
    // TODO: rewrite explain if needed

    bool changed = false;

    // always check program and all deps are known
    changed = File(c.program, *c.fs).isChanged();
    for (auto &i : c.inputs)
        changed |= File(i, *c.fs).isChanged();
    for (auto &i : c.outputs)
        changed |= File(i, *c.fs).isChanged();

    auto k = std::hash<sw::builder::Command>()(c);
    auto r = commands.insert_ptr(k, 0);
    if (r.second)
    {
        // we have insertion, no previous value available
        // so outdated
        EXPLAIN_OUTDATED("command", true, "new command: " + c.print(), c.getName());
        changed = true;
    }

    if (c.always)
    {
        EXPLAIN_OUTDATED("command", true, "always build", c.getName());
        changed = true;
    }

    //auto h = c.calculateFilesHash();
    //return *r.first != h;

    // we have this command, now check if it outdated
    /*if (changed)
    {
        EXPLAIN_OUTDATED("command", true, "program changed", c.getName());
    }*/

    /*if (std::any_of(c.inputs.begin(), c.inputs.end(),
                    [](auto &d) { return File(d).isChanged(); }) ||
        std::any_of(c.outputs.begin(), c.outputs.end(),
                    [](auto &d) { return File(d).isChanged(); }))
    {
        EXPLAIN_OUTDATED("command", true, "i/o file is changed", c.getName());
        changed = true;
    }*/

    //EXPLAIN_OUTDATED("command", false, "ok", c.getName());

    // comment to turn on command hashes feature
    return changed;

    // we don't see changes, now check command hash
    if (!r.second)
        return *r.first != c.calculateFilesHash();

    return false;
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

bool Command::isOutdated() const
{
    return getCommandStorage().isOutdated(*this);
}

size_t Command::getHash() const
{
    if (hash != 0)
        return hash;

    auto h = std::hash<path>()(program);

    // must sort args first
    std::set<String> args_sorted(args.begin(), args.end());
    for (auto &a : args_sorted)
        hash_combine(h, std::hash<String>()(a));

    // redirections are also considered as args
    if (!out.file.empty())
        hash_combine(h, std::hash<path>()(out.file));
    if (!err.file.empty())
        hash_combine(h, std::hash<path>()(out.file));

    // add wdir, env?
    return h;
}

size_t Command::getHashAndSave() const
{
    return hash = getHash();
}

size_t Command::calculateFilesHash() const
{
    auto h = getHash();
    hash_combine(h, File(program, *fs).getFileRecord().getHash());
    for (auto &i : inputs)
        hash_combine(h, File(i, *fs).getFileRecord().getHash());
    for (auto &i : outputs)
        hash_combine(h, File(i, *fs).getFileRecord().getHash());
    return h;
}

void Command::updateFilesHash() const
{
    auto h = calculateFilesHash();
    auto k = std::hash<Command>()(*this);
    auto r = getCommandStorage().commands.insert_ptr(k, h);
    if (!r.second)
        *r.first = h;
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
    if (base)
    {
        p = base->file;
        if (p.empty())
            throw std::runtime_error("Empty program from base program");
    }
    else if (!program.empty())
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
    auto &r = File(p, *fs).getFileRecord();
    r.setGenerator(shared_from_this());
}

void Command::addOutput(const path &p)
{
    if (p.empty())
        return;
    outputs.insert(p);
    auto &r = File(p, *fs).getFileRecord();
    r.setGenerator(shared_from_this());
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
    for (auto &p : outputs)
    {
        File f(p, *fs);
        f.addExplicitDependency(inputs);
        //f.addImplicitDependency(inputs);
    }
}

void Command::prepare()
{
    if (prepared)
        return;

    program = getProgram();
    //if (!program.is_absolute())
        //program = ::primitives::resolve_executable(program);
    getHashAndSave();

    //DEBUG_BREAK_IF_PATH_HAS(program, "google.tensorflow.gen_proto_text_functions-1.10.1.exe");

    // add redirected generated files
    if (!out.file.empty())
        addOutput(out.file);
    if (!err.file.empty())
        addOutput(err.file);

    // add more deps
    if (File(program, *fs).isGenerated())
        dependencies.insert(File(program, *fs).getFileRecord().getGenerator());
    addInputOutputDeps();

    prepared = true;
}

void Command::execute1(std::error_code *ec)
{
    prepare();

    if (!isOutdated())
    {
        executed_ = true;
        return;
    }

    if (isExecuted())
        throw std::logic_error("Trying to execute command twice: " + getName());

    //DEBUG_BREAK_IF_PATH_HAS(program, "rcc.exe");

    executed_ = true;

    printLog();

    if (remove_outputs_before_execution)
    {
        // Some programs won't update their binaries even in case of updated sources/deps.
        // E.g., msvc bug: https://developercommunity.visualstudio.com/content/problem/97608/libexe-does-not-update-import-library.html
        error_code ec;
        for (auto &o : outputs)
            fs::remove(o, ec);
    }

    //static std::atomic_int n = 0;
    //LOG_INFO(logger, "command #" << ++n << " is outdated: " + getName());

    // check our resources
    auto rp = getResourcePool();
    if (rp)
        rp->lock();
    SCOPE_EXIT
    {
        if (rp)
            rp->unlock();
    };

    // Try to construct command line first.
    // Some systems have limitation on its length.

    auto escape_cmd_arg = [](auto &a)
    {
        boost::replace_all(a, "\\", "\\\\");
        boost::replace_all(a, "\"", "\\\"");
        return a;
    };

    auto args_saved = args;
    auto make_rsp_file = [this, &escape_cmd_arg, &args_saved](const auto &rsp_file, bool show_includes = true)
    {
        String rsp;
        for (auto &a : args_saved)
        {
            if (!show_includes && a == "-showIncludes")
                continue;
            if (protect_args_with_quotes)
                rsp += "\"" + escape_cmd_arg(a) + "\"";
            else
                rsp += escape_cmd_arg(a);
            rsp += "\n";
        }
        if (!rsp.empty())
            rsp.resize(rsp.size() - 1);
        args.clear();
        args.push_back("@" + rsp_file.u8string());
        write_file(rsp_file, rsp);
    };

    path rsp_file;
    bool use_rsp = use_response_files || needsResponseFile();
    if (use_rsp)
    {
        auto t = get_temp_filename();
        auto fn = t.filename();
        t = t.parent_path();
        rsp_file = t / getProgramName() / "rsp" / fn;
        rsp_file += ".rsp";
        make_rsp_file(rsp_file);
    }

    SCOPE_EXIT
    {
        if (rsp_file.empty())
            return;
        error_code ec;
        fs::remove(rsp_file, ec);
    };

    auto save_command = [this, &rsp_file, &escape_cmd_arg, &make_rsp_file, &args_saved, &use_rsp](String &s)
    {
        if (rsp_file.empty())
        {
            rsp_file = unique_path();
            rsp_file += ".rsp";
        }
        else
            rsp_file = rsp_file.filename();
        s += "\n";
        auto p = getDirectories().storage_dir_tmp / "rsp" / rsp_file;
        auto pbat = p;
        String t;

#ifdef _WIN32
        pbat += ".bat";
#else
        pbat += ".sh";
#endif

        s += "pid = " + std::to_string(pid) + "\n";
        s += "command is copied to " + p.u8string() + "\n";

#ifdef _WIN32
        t += "@echo off\n";
        t += "setlocal";
#else
        t += "#!/bin/sh";
#endif
        t += "\n";

        for (auto &[k, v] : environment)
        {
#ifdef _WIN32
            t += "set";
#endif
            t += " " + k + "=" + v + "\n";
        }

        if (!working_directory.empty())
            t += "cd " + working_directory.u8string() + "\n";

        t += "\"" + program.u8string() + "\" ";
        if (use_rsp)
        {
            make_rsp_file(p, false);
            t += "@" + p.u8string() + " ";
        }
        else
        {
            for (auto &a : args_saved)
            {
                if (a == "-showIncludes")
                    continue;
                t += "\"" + escape_cmd_arg(a) + "\" ";
            }
        }
#ifdef _WIN32
        t += "%";
#else
        t += "$";
#endif
        t += "* ";

        if (!in.file.empty())
            t += "< " + in.file.u8string() + " ";
        if (!out.file.empty())
            t += "> " + out.file.u8string() + " ";
        if (!err.file.empty())
            t += "2> " + err.file.u8string() + " ";

        t += "\n";

        write_file(pbat, t);
        fs::permissions(pbat,
            fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
            fs::perm_options::add);
    };

    auto make_error_string = [this, &save_command](const String &e)
    {
        postProcess(false);

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
            save_command(s);
        return s;
    };

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
        {
            String s;
            save_command(s);
        }

        postProcess(); // process deps

        // force outputs update
        /*for (auto &i : inputs)
        {
            auto &fr = f.getFileRecord();
            fr.refreshed = false;
            fr.isChanged();
        }*/
        for (auto &i : intermediate)
        {
            File f(i, *fs);
            /*if (!fs::exists(i))
                f.getFileRecord().flags.set(ffNotExists);
            else*/
            //f.getFileRecord().load();
            auto &fr = f.getFileRecord();
            fr.data->refreshed = false;
            fr.isChanged();
            fr.updateLwt();
        }
        for (auto &i : outputs)
        {
            File f(i, *fs);
            /*if (!fs::exists(i))
                f.getFileRecord().flags.set(ffNotExists);
            else*/
            //f.getFileRecord().load();
            auto &fr = f.getFileRecord();
            fr.data->refreshed = false;
            fr.isChanged();
            fr.updateLwt();
        }

        updateFilesHash();
    }
    catch (std::exception &e)
    {
        throw std::runtime_error(make_error_string(e.what()));
    }
}

bool Command::needsResponseFile() const
{
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
        std::string msg = "[" + std::to_string((*current_command)++) + "/" + std::to_string(total_commands) + "] " + getName();
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
    base = p;
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
    String s = getenv("Path");
    environment["Path"] = s + ";" + normalize_path_windows(p);
#else
    String s = getenv("PATH");
    environment["PATH"] = s + ":" + p.u8string();
#endif
}

/*void Command::load(BinaryContext &bctx)
{

}

void Command::save(BinaryContext &bctx)
{

}*/

}

_ExecuteCommand::~_ExecuteCommand()
{
}

size_t _ExecuteCommand::getHash() const
{
    if (hash != 0)
        return hash;

    auto h = std::hash<path>()(getProgram());
    hash_combine(h, std::hash<String>()(file ? file : ""));
    hash_combine(h, std::hash<int>()(line));
    for (auto &i : inputs)
        hash_combine(h, File(i, *fs).getFileRecord().getHash());
    for (auto &i : outputs)
        hash_combine(h, File(i, *fs).getFileRecord().getHash());
    return hash = h;
}

void _ExecuteCommand::prepare()
{
    if (prepared)
        return;
    addInputOutputDeps();
    prepared = true;
}

bool _ExecuteCommand::isOutdated() const
{
    if (std::none_of(inputs.begin(), inputs.end(),
        [this](auto &d) { return File(d, *fs).isChanged(); }) &&
        std::none_of(outputs.begin(), outputs.end(),
            [this](auto &d) { return File(d, *fs).isChanged(); }))
        return false;
    return true;
}

void _ExecuteCommand::execute()
{
    if (always)
    {
        f();
        return;
    }

    if (!isOutdated())
        return;

    printLog();

    f();

    // force outputs update
    for (auto &o : outputs)
        File(o, *fs).getFileRecord().load();
}

}
