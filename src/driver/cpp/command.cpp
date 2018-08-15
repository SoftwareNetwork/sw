// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "target.h"
#include "command.h"

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "command");

namespace sw::driver::cpp
{

void Command::prepare()
{
    // evaluate lazy args
    for (auto &[pos, f] : callbacks)
        args[pos] = f();

    // early cleanup
    callbacks.clear();

    Base::prepare();
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
    else if (dependency)
    {
        if (!dependency->target)
            throw std::runtime_error("Command dependency target was not resolved: " + dependency->getPackage().toString());
        p = dependency->target->getOutputFile();
        if (p.empty())
            throw std::runtime_error("Empty program from package: " + dependency->target->getPackage().target_name);
    }
    else
    {
        p = program;
        if (p.empty())
            throw std::runtime_error("Empty program: was not set");
    }
    return p;
}

void Command::setProgram(const std::shared_ptr<Dependency> &d)
{
    dependency = d;
}

void Command::setProgram(const NativeTarget &t)
{
    setProgram(t.getOutputFile());
}

void Command::pushLazyArg(LazyCallback f)
{
    callbacks[(int)args.size()] = f;
    args.push_back("");
}

///

CommandBuilder &operator<<(CommandBuilder &cb, const NativeExecutedTarget &t)
{
    cb.targets.push_back((NativeExecutedTarget*)&t);
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

    auto p = t.p;
    if (p.is_relative() && !all.empty())
        p = all[0]->SourceDir / p;

    if (!cb.stopped)
        cb.c->args.push_back(p.u8string());
    cb.c->addInput(p);
    for (auto tgt : all)
        *tgt += p;
    return cb;
}

CommandBuilder &operator<<(CommandBuilder &cb, const ::sw::cmd::tag_out &t)
{
    decltype(cb.targets) all;
    all.insert(all.end(), cb.targets.begin(), cb.targets.end());
    all.insert(all.end(), t.targets.begin(), t.targets.end());

    auto p = t.p;
    if (p.is_relative() && !all.empty())
        p = all[0]->BinaryDir / p;

    if (!cb.stopped)
        cb.c->args.push_back(p.u8string());
    cb.c->addOutput(p);
    for (auto tgt : all)
        *tgt += p;
    return cb;
}

CommandBuilder &operator<<(CommandBuilder &cb, const ::sw::cmd::tag_stdout &t)
{
    decltype(cb.targets) all;
    all.insert(all.end(), cb.targets.begin(), cb.targets.end());
    all.insert(all.end(), t.targets.begin(), t.targets.end());

    auto p = t.p;
    if (p.is_relative() && !all.empty())
        p = all[0]->BinaryDir / p;

    cb.c->redirectStdout(p);
    for (auto tgt : all)
        *tgt += p;
    return cb;
}

CommandBuilder &operator<<(CommandBuilder &cb, const ::sw::cmd::tag_stderr &t)
{
    decltype(cb.targets) all;
    all.insert(all.end(), cb.targets.begin(), cb.targets.end());
    all.insert(all.end(), t.targets.begin(), t.targets.end());

    auto p = t.p;
    if (p.is_relative() && !all.empty())
        p = all[0]->BinaryDir / p;

    cb.c->redirectStderr(p);
    for (auto tgt : all)
        *tgt += p;
    return cb;
}

CommandBuilder &operator<<(CommandBuilder &cb, const ::sw::cmd::tag_end &t)
{
    cb.stopped = true;
    return cb;
}

CommandBuilder &operator<<(CommandBuilder &cb, const Command::LazyCallback &t)
{
    if (!cb.stopped)
        cb.c->pushLazyArg(t);
    return cb;
}

}
