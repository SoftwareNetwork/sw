// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "target.h"
#include "command.h"

#include "jumppad.h"
#include "solution.h"

#include <boost/algorithm/string.hpp>
#include <boost/dll.hpp>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "command");

namespace sw::driver::cpp
{

Command::Command()
{
}

Command::Command(::sw::FileStorage &fs)
    : Base::Command(fs)
{

}

Command::~Command()
{
}

void Command::prepare()
{
    // evaluate lazy args
    for (auto &[pos, f] : callbacks)
        args[pos] = f();
    for (auto &f : actions)
        f();

    // early cleanup
    callbacks.clear();
    actions.clear();

    auto d = dependency.lock();
    if (d)
    {
        auto t = d->target.lock();
        if (!t)
            throw std::runtime_error("Command dependency target was not resolved: " + d->getPackage().toString());
        t->setupCommand(*this);
    }

    Base::prepare();
}

path Command::getProgram() const
{
    auto d = dependency.lock();
    path p;
    if (base)
        p = Base::getProgram();
    else if (d)
    {
        auto t = d->target.lock();
        if (!t)
            throw std::runtime_error("Command dependency target was not resolved: " + d->getPackage().toString());
        p = t->getOutputFile();
        if (p.empty())
            throw std::runtime_error("Empty program from package: " + t->getPackage().target_name);
    }
    else if (dependency_set)
    {
        throw std::runtime_error("Command dependency was not resolved: ???UNKNOWN_PROGRAM??? " + print());
    }
    else
        p = Base::getProgram();
    return p;
}

void Command::setProgram(const std::shared_ptr<Dependency> &d)
{
    if (dependency_set)
        throw std::runtime_error("Setting program twice"); // probably throw, but who knows...
    dependency = d;
    dependency_set = true;
    auto l = d->target.lock();
    if (l)
        setProgram(*l);
}

void Command::setProgram(const NativeTarget &t)
{
    setProgram(t.getOutputFile());
    t.setupCommand(*this);
}

void Command::pushLazyArg(LazyCallback f)
{
    callbacks[(int)args.size()] = f;
    args.push_back("");
}

void Command::addLazyAction(LazyAction f)
{
    actions.push_back(f);
}

void VSCommand::postProcess(bool)
{
    // filter out includes and file name
    static const auto pattern = "Note: including file:"s;

    std::deque<String> lines;
    boost::split(lines, out.text, boost::is_any_of("\n"));
    out.text.clear();
    // remove filename
    lines.pop_front();

    //file.clearImplicitDependencies();

    /*for (auto &f : outputs)
        File(f, *fs).clearImplicitDependencies();
    for (auto &f : outputs)
        File(f, *fs).clearImplicitDependencies();*/

    for (auto &line : lines)
    {
        auto p = line.find(pattern);
        if (p != 0)
        {
            out.text += line + "\n";
            continue;
        }
        auto include = line.substr(pattern.size());
        boost::trim(include);
        //file.addImplicitDependency(include);
        for (auto &f : intermediate)
            File(f, *fs).addImplicitDependency(include);
        for (auto &f : outputs)
            File(f, *fs).addImplicitDependency(include);
    }
}

void GNUCommand::postProcess(bool ok)
{
    if (!ok || deps_file.empty())
        return;
    if (!fs::exists(deps_file))
        return;

    static const std::regex space_r("[^\\\\] ");

    auto lines = read_lines(deps_file);
    //file.clearImplicitDependencies();
    for (auto i = lines.begin() + 1; i != lines.end(); i++)
    {
        auto &s = *i;
        s.resize(s.size() - 1);
        boost::trim(s);
        s = std::regex_replace(s, space_r, "\n");
        boost::replace_all(s, "\\ ", " ");
        Strings files;
        boost::split(files, s, boost::is_any_of("\n"));
        //boost::replace_all(s, "\\\"", "\""); // probably no quotes
        //for (auto &f : files)
            //file.addImplicitDependency(f);

        for (auto &f2 : files)
        {
            for (auto &f : intermediate)
                File(f, *fs).addImplicitDependency(f2);
            for (auto &f : outputs)
                File(f, *fs).addImplicitDependency(f2);
        }
    }
}

///

CommandBuilder &operator<<(CommandBuilder &cb, const NativeExecutedTarget &t)
{
    auto nt = (NativeExecutedTarget*)&t;
    cb.targets.push_back(nt);
    nt->Storage.push_back(cb.c);
    if (!cb.c->fs)
        cb.c->fs = nt->getSolution()->fs;
    return cb;
}

CommandBuilder &operator<<(CommandBuilder &cb, const ::sw::cmd::tag_wdir &t)
{
    auto p = t.p;
    if (p.is_relative() && !cb.targets.empty())
        p = cb.targets[0]->SourceDir / p;

    cb.c->working_directory = p;
    return cb;
}

CommandBuilder &operator<<(CommandBuilder &cb, const ::sw::cmd::tag_in &t)
{
    decltype(cb.targets) all;
    all.insert(all.end(), cb.targets.begin(), cb.targets.end());
    all.insert(all.end(), t.targets.begin(), t.targets.end());

    if (!all.empty() && all[0]->PostponeFileResolving)
        return cb;

    for (auto p : t.files)
    {
        if (p.is_relative() && !all.empty())
            if (!all[0]->check_absolute(p, true))
                p = all[0]->SourceDir / p;

        if (!cb.stopped)
            cb.c->args.push_back(t.prefix + p.u8string());
        cb.c->addInput(p);
        if (t.add_to_targets)
        {
            for (auto tgt : all)
                *tgt += p;
        }
    }
    for (auto tgt : t.targets)
        tgt->Storage.push_back(cb.c);
    return cb;
}

CommandBuilder &operator<<(CommandBuilder &cb, const ::sw::cmd::tag_out &t)
{
    decltype(cb.targets) all;
    all.insert(all.end(), cb.targets.begin(), cb.targets.end());
    all.insert(all.end(), t.targets.begin(), t.targets.end());

    for (auto p : t.files)
    {
        if (p.is_relative() && !all.empty())
            if (!all[0]->check_absolute(p, true))
                p = all[0]->BinaryDir / p;

        if (!cb.stopped)
            cb.c->args.push_back(t.prefix + p.u8string());
        cb.c->addOutput(p);
        if (t.add_to_targets)
        {
            for (auto tgt : all)
                *tgt += p;
        }
    }
    for (auto tgt : t.targets)
        tgt->Storage.push_back(cb.c);
    return cb;
}

CommandBuilder &operator<<(CommandBuilder &cb, const ::sw::cmd::tag_stdin &t)
{
    decltype(cb.targets) all;
    all.insert(all.end(), cb.targets.begin(), cb.targets.end());
    all.insert(all.end(), t.targets.begin(), t.targets.end());

    auto p = t.p;
    if (p.is_relative() && !all.empty())
        if (!all[0]->check_absolute(p, true))
            p = all[0]->SourceDir / p;

    cb.c->redirectStdin(p);
    if (t.add_to_targets)
    {
        for (auto tgt : all)
            *tgt += p;
    }
    for (auto tgt : t.targets)
        tgt->Storage.push_back(cb.c);
    return cb;
}

CommandBuilder &operator<<(CommandBuilder &cb, const ::sw::cmd::tag_stdout &t)
{
    decltype(cb.targets) all;
    all.insert(all.end(), cb.targets.begin(), cb.targets.end());
    all.insert(all.end(), t.targets.begin(), t.targets.end());

    auto p = t.p;
    if (p.is_relative() && !all.empty())
        if (!all[0]->check_absolute(p, true))
            p = all[0]->BinaryDir / p;

    cb.c->redirectStdout(p);
    if (t.add_to_targets)
    {
        for (auto tgt : all)
            *tgt += p;
    }
    for (auto tgt : t.targets)
        tgt->Storage.push_back(cb.c);
    return cb;
}

CommandBuilder &operator<<(CommandBuilder &cb, const ::sw::cmd::tag_stderr &t)
{
    decltype(cb.targets) all;
    all.insert(all.end(), cb.targets.begin(), cb.targets.end());
    all.insert(all.end(), t.targets.begin(), t.targets.end());

    auto p = t.p;
    if (p.is_relative() && !all.empty())
        if (!all[0]->check_absolute(p, true))
            p = all[0]->BinaryDir / p;

    cb.c->redirectStderr(p);
    if (t.add_to_targets)
    {
        for (auto tgt : all)
            *tgt += p;
    }
    for (auto tgt : t.targets)
        tgt->Storage.push_back(cb.c);
    return cb;
}

CommandBuilder &operator<<(CommandBuilder &cb, const ::sw::cmd::tag_end &t)
{
    cb.stopped = true;
    return cb;
}

CommandBuilder &operator<<(CommandBuilder &cb, const ::sw::cmd::tag_dep &t)
{
    for (auto tgt : cb.targets)
    {
        for (auto &t : t.targets)
        {
            auto d = *tgt + *t;
            d->Dummy = true;
        }
        for (auto &t : t.target_ptrs)
        {
            auto d = *tgt + t;
            d->Dummy = true;
        }
    }
    return cb;
}

CommandBuilder &operator<<(CommandBuilder &cb, const ::sw::cmd::tag_env &t)
{
    cb.c->environment[t.k] = t.v;
    return cb;
}

CommandBuilder &operator<<(CommandBuilder &cb, const Command::LazyCallback &t)
{
    if (!cb.stopped)
        cb.c->pushLazyArg(t);
    return cb;
}

ExecuteBuiltinCommand::ExecuteBuiltinCommand()
{
    program = boost::dll::program_location().string();
}

ExecuteBuiltinCommand::ExecuteBuiltinCommand(const String &cmd_name, void *f)
    : ExecuteBuiltinCommand()
{
    args.push_back("internal-call-builtin-function");
    args.push_back(getModuleNameForSymbol(f).u8string());
    args.push_back(cmd_name);
}

void ExecuteBuiltinCommand::push_back(const Files &files)
{
    args.push_back(std::to_string(files.size()));
    for (auto &o : FilesSorted{ files.begin(), files.end() })
        args.push_back(o.u8string());
}

void ExecuteBuiltinCommand::execute()
{
    auto call = [this]()
    {
        return jumppad_call(args[1], args[2], Strings{ args.begin() + 3, args.end() });
    };

    if (always)
    {
        call();
        return;
    }

    if (!isOutdated())
        return;

    printLog();

    call();

    // force outputs update
    for (auto &o : outputs)
        File(o, *fs).getFileRecord().load();
}

}
