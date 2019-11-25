// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "command.h"

#include "build.h"
#include "target/native.h"

#include <sw/builder/platform.h>
#include <sw/core/sw_context.h>

#include <primitives/symbol.h>

#include <boost/algorithm/string.hpp>
#include <boost/dll.hpp>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "cpp.command");

#include <sw/support/serialization.h>
#include <boost/serialization/export.hpp>
/*BOOST_CLASS_EXPORT_GUID(::sw::driver::detail::Command, "sw.driver.command.detail")
BOOST_CLASS_EXPORT_GUID(::sw::driver::VSCommand, "sw.driver.command.vs")
BOOST_CLASS_EXPORT_GUID(::sw::driver::GNUCommand, "sw.driver.command.gnu")*/

//BOOST_CLASS_EXPORT_GUID(::sw::driver::Command, "sw.driver.command2")
BOOST_CLASS_EXPORT_IMPLEMENT(::sw::driver::Command)

/*#include <boost/serialization/type_info_implementation.hpp>
#include <boost/serialization/extended_type_info_typeid.hpp>
BOOST_CLASS_TYPE_INFO(
    ::sw::driver::Command,
    boost::serialization::extended_type_info_typeid<::sw::driver::Command>
)*/

#define SERIALIZATION_TYPE ::sw::driver::Command
SERIALIZATION_BEGIN_SPLIT
    SW_UNIMPLEMENTED;
SERIALIZATION_SPLIT_CONTINUE
    SW_UNIMPLEMENTED;
SERIALIZATION_SPLIT_END

namespace sw::driver
{

std::shared_ptr<Command> Command::clone() const
{
    return std::make_shared<Command>(*this);
}

void Command::prepare()
{
    if (prepared)
        return;

    // evaluate lazy
    for (auto &f : actions)
        f();

    // early cleanup
    actions.clear();

    // target may set program explicitly (e.g. to system program)
    // so we don't check other conditions below
    if (!isProgramSet())
    {
        auto d = dependency.lock();
        if (d)
        {
            auto &t = d->getTarget();
            ((const NativeTarget &)t).setupCommand(*this);

            path p;
            if (auto nt = t.as<NativeCompiledTarget*>())
            {
                p = nt->getOutputFile();
                if (!p.empty() && !File(p, getContext().getFileStorage()).isGenerated())
                {
                    if (*nt->HeaderOnly)
                        throw SW_RUNTIME_ERROR("Program is used from package: " + t.getPackage().toString() + " which is header only");
                    if (!File(p, getContext().getFileStorage()).isGeneratedAtAll())
                        throw SW_RUNTIME_ERROR("Program from package: " + t.getPackage().toString() + " is not generated at all: " + normalize_path(p));
                    throw SW_RUNTIME_ERROR("Program from package: " + t.getPackage().toString() + " is not generated: " + normalize_path(p));
                }
            }
            else if (auto nt = t.as<NativeTarget*>())
                p = nt->getOutputFile();
            else
                throw SW_RUNTIME_ERROR("Package: " + t.getPackage().toString() + " has unknown type");

            if (p.empty())
                throw SW_RUNTIME_ERROR("Empty program from package: " + t.getPackage().toString());
            setProgram(p);
        }
        else if (dependency_set)
        {
            throw SW_RUNTIME_ERROR("Command dependency was not resolved: ???UNKNOWN_PROGRAM??? " + print());
        }
    }

    for (auto &d1 : dependencies)
    {
        auto d = d1.lock();
        if (!d)
            throw SW_RUNTIME_ERROR("Command dependency was not resolved: ???UNKNOWN_PROGRAM??? " + print());

        auto &t = d->getTarget();
        ((const NativeTarget &)t).setupCommand(*this);
    }

    Base::prepare();
}

void Command::setProgram(const std::shared_ptr<Dependency> &d)
{
    if (dependency_set)
        throw SW_RUNTIME_ERROR("Setting program twice"); // probably throw, but who knows...
    dependency = d;
    dependency_set = true;
}

void Command::addProgramDependency(const std::shared_ptr<Dependency> &d)
{
    dependencies.push_back(d);
}

void Command::addLazyAction(LazyAction f)
{
    actions.push_back(f);
}

Command &Command::operator|(CommandBuilder &c)
{
    Base::operator|(*c.c);
    return *this;
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
    // but locales!
    // "Note: including file: filename\r" (english)
    // "Some: other lang: filename\r"
    // "Some: other lang  filename\r" (ita)
    auto &p = getMsvcIncludePrefixes();
    auto i = p.find(getProgram());
    if (i == p.end())
        throw SW_RUNTIME_ERROR("Cannot find msvc prefix");
    auto &prefix = i->second;

    auto perform = [this, &prefix](auto &text)
    {
        std::deque<String> lines;
        boost::split(lines, text, boost::is_any_of("\n"));
        text.clear();
        // remove filename
        lines.pop_front();

        for (auto &line : lines)
        {
            if (line.find(prefix) != 0)
            {
                text += line + "\n";
                continue;
            }
            auto include = line.substr(prefix.size());
            boost::trim(include);
            //if (fs::exists(include)) // slow check? but correct?
                addImplicitInput(include);
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
        // p2 may be 0
        if (p2 && f[p2 - 1] != '\\')
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
        //if (fs::exists(f3))
            addImplicitInput(f3);
    }
}

///

CommandBuilder::CommandBuilder(const SwBuilderContext &swctx)
{
    c = std::make_shared<Command>(swctx);
}

CommandBuilder &CommandBuilder::operator|(CommandBuilder &c2)
{
    operator|(*c2.c);
    return *this;
}

CommandBuilder &CommandBuilder::operator|(::sw::builder::Command &c2)
{
    *c | c2;
    return * this;
}

CommandBuilder &operator<<(CommandBuilder &cb, const NativeCompiledTarget &t)
{
    auto nt = (NativeCompiledTarget*)&t;
    cb.targets.push_back(nt);
    nt->Storage.push_back(cb.c);
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

    if (!all.empty() && all[0]->DryRun)
        return cb;

    for (auto p : t.files)
    {
        if (p.is_relative() && !all.empty())
            if (!all[0]->check_absolute(p, true))
                p = all[0]->SourceDir / p;

        if (!cb.stopped)
            cb.c->arguments.push_back(t.prefix + (t.normalize ? normalize_path(p) : p.u8string()));
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

    bool dry_run = std::all_of(all.begin(), all.end(), [](const auto &t)
    {
        return t->DryRun;
    });

    for (auto p : t.files)
    {
        if (p.is_relative() && !all.empty())
            if (!all[0]->check_absolute(p, true))
                p = all[0]->BinaryDir / p;

        if (!cb.stopped)
            cb.c->arguments.push_back(t.prefix + (t.normalize ? normalize_path(p) : p.u8string()));
        if (!dry_run)
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

    bool dry_run = std::all_of(all.begin(), all.end(), [](const auto &t)
    {
        return t->DryRun;
    });

    auto p = t.p;
    if (p.is_relative() && !all.empty())
        if (!all[0]->check_absolute(p, true))
            p = all[0]->BinaryDir / p;

    if (!dry_run)
        cb.c->redirectStdout(p, t.append);
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

    bool dry_run = std::all_of(all.begin(), all.end(), [](const auto &t)
    {
        return t->DryRun;
    });

    auto p = t.p;
    if (p.is_relative() && !all.empty())
        if (!all[0]->check_absolute(p, true))
            p = all[0]->BinaryDir / p;

    if (!dry_run)
        cb.c->redirectStderr(p, t.append);
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
    SW_UNIMPLEMENTED;
}

CommandBuilder &operator<<(CommandBuilder &cb, const ::sw::cmd::tag_env &t)
{
    cb.c->environment[t.k] = t.v;
    return cb;
}

CommandBuilder &operator<<(CommandBuilder &cb, const Command::LazyCallback &t)
{
    struct LazyArgument : ::primitives::command::Argument
    {
        Command::LazyCallback cb;

        LazyArgument(Command::LazyCallback cb)
            : cb(cb)
        {}

        String toString() const override
        {
            return cb();
        }

        std::unique_ptr<::primitives::command::Argument> clone() const
        {
            return std::make_unique<LazyArgument>(cb);
        }
    };

    if (!cb.stopped)
    {
        cb.c->arguments.push_back(std::make_unique<LazyArgument>(t));
    }
    return cb;
}

}
