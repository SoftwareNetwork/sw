// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "target/native.h"
#include "command.h"

#include "jumppad.h"
#include "solution.h"
#include "platform.h"

#include <primitives/symbol.h>

#include <boost/algorithm/string.hpp>
#include <boost/dll.hpp>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "cpp.command");

namespace sw::driver::cpp
{

namespace detail
{

Command::Command(::sw::FileStorage &fs)
    : Base::Command(fs)
{
}

}

Command::Command(::sw::FileStorage &fs)
    : Base::Command(fs)
{
}

std::shared_ptr<Command> Command::clone() const
{
    return std::make_shared<Command>(*this);
}

void Command::prepare()
{
    if (prepared)
        return;

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
        auto t = d->target;
        if (!t)
            throw SW_RUNTIME_ERROR("Command dependency target was not resolved: " + d->getPackage().toString());
        ((NativeTarget*)t)->setupCommand(*this);
    }

    Base::prepare();
}

path Command::getProgram() const
{
    // target may set program explicitly (e.g. to system program)
    // so we don't check other conditions below
    if (!program.empty())
        return program;

    auto d = dependency.lock();
    path p;
    if (d)
    {
        auto t = d->target;
        if (!t)
            throw SW_RUNTIME_ERROR("Command dependency target was not resolved: " + d->getPackage().toString());
        if (auto nt = t->as<NativeTarget>())
        {
            p = nt->getOutputFile();
            if (p.empty())
                throw SW_RUNTIME_ERROR("Empty program from package: " + t->getPackage().toString());
            if (!File(p, *fs).isGenerated())
            {
                if (auto nt = t->as<NativeExecutedTarget>(); nt && *nt->HeaderOnly)
                    throw SW_RUNTIME_ERROR("Program is used from package: " + t->getPackage().toString() + " which is header only");
                if (!File(p, *fs).isGeneratedAtAll())
                    throw SW_RUNTIME_ERROR("Program from package: " + t->getPackage().toString() + " is not generated at all: " + normalize_path(p));
                throw SW_RUNTIME_ERROR("Program from package: " + t->getPackage().toString() + " is not generated: " + normalize_path(p));
            }
        }
        else
            throw SW_RUNTIME_ERROR("Package: " + t->getPackage().toString() + " has unknown type");
    }
    else if (dependency_set)
    {
        throw SW_RUNTIME_ERROR("Command dependency was not resolved: ???UNKNOWN_PROGRAM??? " + print());
    }
    else
        p = Base::getProgram();
    return p;
}

void Command::setProgram(const std::shared_ptr<Dependency> &d)
{
    if (dependency_set)
        throw SW_RUNTIME_ERROR("Setting program twice"); // probably throw, but who knows...
    dependency = d;
    dependency_set = true;
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

std::shared_ptr<Command> VSCommand::clone() const
{
    return std::make_shared<VSCommand>(*this);
}

void VSCommand::postProcess1(bool)
{
    // deps are placed into command output,
    // so we can't skip this filtering

    // filter out includes and file name
    static const auto pattern = "Note: including file:"s;

    auto perform = [this](auto &text)
    {
        std::deque<String> lines;
        boost::split(lines, text, boost::is_any_of("\n"));
        text.clear();
        // remove filename
        lines.pop_front();

        for (auto &line : lines)
        {
            auto p = line.find(pattern);
            if (p != 0)
            {
                text += line + "\n";
                continue;
            }
            auto include = line.substr(pattern.size());
            boost::trim(include);
            for (auto &f : outputs)
                File(f, *fs).addImplicitDependency(include);
        }
    };

    // on errors msvc puts everything to stderr instead of stdout
    perform(out.text);
    perform(err.text);
}

std::shared_ptr<Command> GNUCommand::clone() const
{
    return std::make_shared<GNUCommand>(*this);
}

void GNUCommand::postProcess1(bool ok)
{
    // deps are placed into separate file, so we can skip our jobs
    if (!ok)
        return;

    if (deps_file.empty())
        return;
    if (!fs::exists(deps_file))
    {
        LOG_DEBUG(logger, "Missing deps file: " + normalize_path(deps_file));
        return;
    }

    static const std::regex space_r("[^\\\\] ");

    // deps file is a make in form
    // target: dependencies
    // deps are split by spaces on several lines with \ at the end of each line except the last one
    //
    // example:
    //
    // file.o: dep1.cpp dep2.cpp \
    //  dep1.h dep2.h \
    //  dep3.h \
    //  dep4.h
    //

    auto f = read_file(deps_file);

    // skip target
    //  use exactly ': ' because on windows target is 'C:/path/to/file: '
    //                                           skip up to this space ^
    f = f.substr(f.find(": ") + 1);

    boost::trim(f);
    boost::replace_all(f, "\\\r", ""); // CR LF case or just CR
    boost::replace_all(f, "\\\n", "");
    boost::replace_all(f, "\r", "");
    boost::replace_all(f, "\n", "");

    FilesOrdered files;
    size_t p = 0;
    while (1)
    {
        auto p2 = f.find(' ', p);
        if (p2 == f.npos)
        {
            auto s = f.substr(p);
            if (!s.empty())
                files.push_back(s);
            break;
        }
        if (f[p2 - 1] != '\\')
        {
            auto s = f.substr(p, p2 - p);
            if (!s.empty())
                files.push_back(s);
        }
        p = p2;
        p++;
    }

    for (auto &f2 : files)
    {
        auto f3 = normalize_path(f2);
#ifdef CPPAN_OS_WINDOWS_NO_CYGWIN
        static const String cyg = "/cygdrive/";
        if (f3.find(cyg) == 0)
        {
            f3 = f3.substr(cyg.size());
            f3 = toupper(f3[0]) + ":" + f3.substr(1);
        }
#endif
        for (auto &f : outputs)
            File(f, *fs).addImplicitDependency(f3);
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
            cb.c->args.push_back(t.prefix + (t.normalize ? normalize_path(p) : p.u8string()));
        cb.c->addInput(p);
        if (t.add_to_targets)
        {
            for (auto tgt : all)
            {
                *tgt += p;
                (*tgt)[p].skip = t.skip;
            }
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
            cb.c->args.push_back(t.prefix + (t.normalize ? normalize_path(p) : p.u8string()));
        cb.c->addOutput(p);
        if (t.add_to_targets)
        {
            for (auto tgt : all)
            {
                *tgt += p;
                (*tgt)[p].skip = t.skip;
            }
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
        {
            *tgt += p;
            (*tgt)[p].skip = t.skip;
        }
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
        {
            *tgt += p;
            (*tgt)[p].skip = t.skip;
        }
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
        {
            *tgt += p;
            (*tgt)[p].skip = t.skip;
        }
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

ExecuteBuiltinCommand::ExecuteBuiltinCommand(const String &cmd_name, void *f, int version)
    : ExecuteBuiltinCommand()
{
    args.push_back("internal-call-builtin-function");
    args.push_back(normalize_path(primitives::getModuleNameForSymbol(f))); // add dependency on this? or on function (command) version
    args.push_back(cmd_name);
    args.push_back(std::to_string(version));
}

void ExecuteBuiltinCommand::push_back(const Files &files)
{
    args.push_back(std::to_string(files.size()));
    for (auto &o : FilesSorted{ files.begin(), files.end() })
        args.push_back(normalize_path(o));
}

void ExecuteBuiltinCommand::execute1(std::error_code *ec)
{
    // add try catch?
    jumppad_call(args[1], args[2], std::stoi(args[3]), Strings{ args.begin() + 4, args.end() });
}

bool ExecuteBuiltinCommand::isTimeChanged() const
{
    try
    {
        return std::any_of(inputs.begin(), inputs.end(), [this](const auto &i) {
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

size_t ExecuteBuiltinCommand::getHash1() const
{
    size_t h = 0;
    // ignore program!

    hash_combine(h, std::hash<String>()(args[2])); // include function name
    hash_combine(h, std::hash<String>()(args[3])); // include version

    // must sort args first, why?
    std::set<String> args_sorted(args.begin() + 4, args.end());
    for (auto &a : args_sorted)
        hash_combine(h, std::hash<String>()(a));

    return h;
}

}
