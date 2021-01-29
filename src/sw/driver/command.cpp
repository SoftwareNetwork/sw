// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "command.h"

#include "build.h"
#include "target/native.h"
#include "program_version_storage.h"

#include <sw/builder/platform.h>
#include <sw/core/sw_context.h>
#include <sw/manager/storage.h>

#include <primitives/symbol.h>

#include <boost/algorithm/string.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "cpp.command");

struct LazyArgument : ::primitives::command::Argument
{
    ::sw::driver::Command::LazyCallback cb;

    LazyArgument(::sw::driver::Command::LazyCallback cb = {})
        : cb(cb)
    {}

    String toString() const override
    {
        if (!cached_value.empty())
            return cached_value;
        return cached_value = cb();
    }

    std::unique_ptr<::primitives::command::Argument> clone() const
    {
        return std::make_unique<LazyArgument>(cb);
    }

private:
    mutable String cached_value;
};

namespace sw
{

namespace driver
{

std::shared_ptr<Command> Command::clone() const
{
    return std::make_shared<Command>(*this);
}

void Command::prepare()
{
    if (prepared)
        return;

    // target may set program explicitly (e.g. to system program)
    // so we don't check other conditions below
    if (!isProgramSet())
    {
        if (rd)
        {
            if (!rd->dep)
                throw SW_RUNTIME_ERROR("No dependency set for rule: " + rd->rule_name);
            dependency = rd->dep;
        }
        auto d = dependency.lock();
        if (d)
        {
            SW_UNIMPLEMENTED;
            /*auto &t = d->getTarget();
            if (auto nt = t.as<NativeTarget *>())
                nt->setupCommand(*this);
            else if (auto nt = t.as<PredefinedTarget *>())
            {
                const auto &is = nt->getInterfaceSettings();
                if (is["run_command"])
                {
                    for (auto &[k, v] : is["run_command"]["environment"].getMap())
                        environment[k] = v.getValue();
                }
            }
            else
                throw SW_RUNTIME_ERROR("missing predefined target code");

            // command may be set inside setupCommand()
            if (!isProgramSet())
            {
                path p;
                if (auto nt = t.as<NativeCompiledTarget *>())
                {
                    p = nt->getOutputFile();
                    //if (!p.empty() && !File(p, getContext().getFileStorage()).isGenerated())
                    //{
                    //    if (*nt->HeaderOnly)
                    //        throw SW_RUNTIME_ERROR("Program is used from package: " + t.getPackage().toString() + " which is header only");
                    //    if (!File(p, getContext().getFileStorage()).isGenerated())
                    //        throw SW_RUNTIME_ERROR("Program from package: " + t.getPackage().toString() + " is not generated at all: " + to_string(normalize_path(p)));
                    //    throw SW_RUNTIME_ERROR("Program from package: " + t.getPackage().toString() + " is not generated: " + to_string(normalize_path(p)));
                    //}
                }
                else if (auto nt = t.as<NativeTarget *>())
                    p = nt->getOutputFile();
                else if (auto nt = t.as<PredefinedProgram *>())
                {
                    p = nt->getProgram().file;
                }
                else if (auto nt = t.as<ITarget *>())
                {
                    auto &of = nt->getInterfaceSettings()["output_file"];
                    if (!of)
                        throw SW_RUNTIME_ERROR("Empty output file in target: " + nt->getPackage().toString());
                    SW_UNIMPLEMENTED;
                    //p = of.getPathValue(getContext().getLocalStorage());
                }
                else
                    throw SW_RUNTIME_ERROR("Package: " + t.getPackage().toString() + " has unknown type");

                if (p.empty())
                    throw SW_RUNTIME_ERROR("Empty program from package: " + t.getPackage().toString());
                setProgram(p);
                addInput(p);
            }*/
        }
        else if (dependency_set)
        {
            throw SW_RUNTIME_ERROR("Command dependency was not resolved: ???UNKNOWN_PROGRAM??? " + print());
        }
    }

    // more setup
    for (auto &d1 : dependencies)
    {
        auto d = d1.lock();
        if (!d)
            throw SW_RUNTIME_ERROR("Command dependency was not resolved: ???UNKNOWN_PROGRAM??? " + print());

        SW_UNIMPLEMENTED;
        /*auto &t = d->getTarget();
        if (auto nt = t.as<NativeTarget *>())
            nt->setupCommand(*this);
        else if (auto nt = t.as<PredefinedTarget *>())
        {
            const auto &is = nt->getInterfaceSettings();
            if (is["run_command"])
            {
                for (auto &[k, v] : is["run_command"]["environment"].getMap())
                {
                    if (k == "PATH")
                        appendEnvironmentArrayValue(k, v.getValue(), true);
                    else
                        environment[k] = v.getValue();
                }
            }
        }
        else
            throw SW_RUNTIME_ERROR("missing predefined target code");*/
    }

    Base::prepare();
}

void Command::setProgram(const DependencyPtr &d)
{
    if (dependency_set)
        throw SW_RUNTIME_ERROR("Setting program twice"); // probably throw, but who knows...
    dependency = d;
    dependency_set = true;
}

void Command::addProgramDependency(const DependencyPtr &d)
{
    dependencies.push_back(d);
}

Command &Command::operator|(CommandBuilder &c)
{
    Base::operator|(*c.getCommand());
    return *this;
}

///

CommandBuilder::CommandBuilder(Target &t, const std::shared_ptr<::sw::builder::Command> &in)
    : target(&t)
{
    setCommand(in ? in : std::make_shared<Command>());
}

void CommandBuilder::setCommand(const std::shared_ptr<::sw::builder::Command> &c2)
{
    c = c2;
    getTarget().addGeneratedCommand(c);
}

Target &CommandBuilder::getTarget() const
{
    return *target;
}

const CommandBuilder &CommandBuilder::operator|(const CommandBuilder &c2) const
{
    operator|(*c2.c);
    return *this;
}

const CommandBuilder &CommandBuilder::operator|(::sw::builder::Command &c2) const
{
    *c | c2;
    return *this;
}

CommandBuilder &CommandBuilder::operator<<(const ::sw::cmd::tag_wdir &t)
{
    auto p = t.p;
    if (p.is_relative())
        p = getTarget().SourceDir / p;
    c->working_directory = p;
    return *this;
}

static NativeTargetOptionsGroup &cast_as_nct(Target &t)
{
    return dynamic_cast<NativeTargetOptionsGroup &>(t);
}
static NativeTargetOptionsGroup *cast_as_nct(Target *t)
{
    return &cast_as_nct(*t);
}

CommandBuilder &CommandBuilder::operator<<(const ::sw::cmd::tag_in &t)
{
    auto &tt = getTarget();
    if (tt.DryRun)
        return *this;

    for (auto p : t.files)
    {
        if (p.is_relative() && !cast_as_nct(tt).check_absolute(p, true))
            p = tt.SourceDir / p;

        if (!stopped)
            c->arguments.push_back(t.prefix + to_printable_string(t.normalize ? normalize_path(p) : p));
        c->addInput(p);
        if (t.add_to_targets)
        {
            cast_as_nct(tt).getMergeObject() += p;
            cast_as_nct(tt).getMergeObject()[p].skip = t.skip;
            // also add into private
            cast_as_nct(tt).add(cast_as_nct(tt).getMergeObject().getFileInternal(p));
        }
    }
    return *this;
}

CommandBuilder &CommandBuilder::operator<<(const ::sw::cmd::tag_out &t)
{
    auto &tt = getTarget();
    if (tt.DryRun)
        return *this;

    for (auto p : t.files)
    {
        if (p.is_relative() && !cast_as_nct(tt).check_absolute(p, true))
            p = tt.BinaryDir / p;

        if (!stopped)
            c->arguments.push_back(t.prefix + to_printable_string(t.normalize ? normalize_path(p) : p));
        c->addOutput(p, tt.getFs());
        if (t.add_to_targets)
        {
            cast_as_nct(tt).getMergeObject() += p;
            cast_as_nct(tt).getMergeObject()[p].skip = t.skip;
            // also add into private
            cast_as_nct(tt).add(cast_as_nct(tt).getMergeObject().getFileInternal(p));
        }
    }
    return *this;
}

CommandBuilder &CommandBuilder::operator<<(const ::sw::cmd::tag_stdin &t)
{
    auto &tt = getTarget();
    if (tt.DryRun)
        return *this;

    auto p = t.p;
    if (p.is_relative() && !cast_as_nct(tt).check_absolute(p, true))
        p = tt.SourceDir / p;

    c->redirectStdin(p);
    if (t.add_to_targets)
    {
        cast_as_nct(tt).getMergeObject() += p;
        cast_as_nct(tt).getMergeObject()[p].skip = t.skip;
        // also add into private
        cast_as_nct(tt).add(cast_as_nct(tt).getMergeObject().getFileInternal(p));
    }
    return *this;
}

CommandBuilder &CommandBuilder::operator<<(const ::sw::cmd::tag_stdout &t)
{
    auto &tt = getTarget();
    if (tt.DryRun)
        return *this;

    auto p = t.p;
    if (p.is_relative() && !cast_as_nct(tt).check_absolute(p, true))
        p = tt.BinaryDir / p;

    c->redirectStdout(p, tt.getFs(), t.append);
    if (t.add_to_targets)
    {
        cast_as_nct(tt).getMergeObject() += p;
        cast_as_nct(tt).getMergeObject()[p].skip = t.skip;
        // also add into private
        cast_as_nct(tt).add(cast_as_nct(tt).getMergeObject().getFileInternal(p));
    }
    return *this;
}

CommandBuilder &CommandBuilder::operator<<(const ::sw::cmd::tag_stderr &t)
{
    auto &tt = getTarget();
    if (tt.DryRun)
        return *this;

    auto p = t.p;
    if (p.is_relative() && !cast_as_nct(tt).check_absolute(p, true))
        p = tt.BinaryDir / p;

    c->redirectStderr(p, tt.getFs(), t.append);
    if (t.add_to_targets)
    {
        cast_as_nct(tt).getMergeObject() += p;
        cast_as_nct(tt).getMergeObject()[p].skip = t.skip;
        // also add into private
        cast_as_nct(tt).add(cast_as_nct(tt).getMergeObject().getFileInternal(p));
    }
    return *this;
}

CommandBuilder &CommandBuilder::operator<<(const ::sw::cmd::tag_end &t)
{
    stopped = true;
    return *this;
}

CommandBuilder &CommandBuilder::operator<<(const ::sw::cmd::tag_dep &t)
{
    for (auto &d : t.targets)
        getTarget().addSourceDependency(*d);
    for (auto &d : t.target_ptrs)
        getTarget().addSourceDependency(d);
    return *this;
}

CommandBuilder &CommandBuilder::operator<<(const ::sw::cmd::tag_env &t)
{
    c->environment[t.k] = t.v;
    return *this;
}

CommandBuilder &CommandBuilder::operator<<(const ::sw::cmd::tag_prog_dep &t)
{
    std::dynamic_pointer_cast<::sw::driver::Command>(c)->setProgram(t.d);
    getTarget().addDummyDependency(t.d);
    return *this;
}

CommandBuilder &CommandBuilder::operator<<(const ::sw::cmd::tag_prog_prog &t)
{
    c->setProgram(t.p);
    return *this;
}

CommandBuilder &CommandBuilder::operator<<(const ::sw::cmd::tag_prog_rule &t)
{
    std::dynamic_pointer_cast<::sw::driver::Command>(c)->setProgram(t.rd);
    return *this;
}

CommandBuilder &CommandBuilder::operator<<(const ::sw::cmd::tag_prog_tgt &t)
{
    auto d = std::make_shared<Dependency>(t.t->getPackage());
    *this << cmd::prog(d);
    return *this;
}

CommandBuilder &CommandBuilder::operator<<(const Command::LazyCallback &t)
{
    if (!stopped)
        c->arguments.push_back(std::make_unique<LazyArgument>(t));
    return *this;
}

} // namespace driver

static String getOutput(builder::detail::ResolvableCommand &c)
{
    error_code ec;
    c.execute(ec);

    if (c.pid == -1)
        throw SW_RUNTIME_ERROR(to_string(normalize_path(c.getProgram())) + ": " + ec.message());

    return c.err.text.empty() ? c.out.text : c.err.text;
}

static std::pair<String, PackageVersion> gatherVersion1(builder::detail::ResolvableCommand &c, const String &in_regex)
{
    static std::regex r_default("(\\d+)(\\.(\\d+)){2,}(-[[:alnum:]]+([.-][[:alnum:]]+)*)?");

    auto o = getOutput(c);

    PackageVersion v;
    auto get_default_version = [&o, &v](const String &in_string)
    {
        std::smatch m;
        if (!std::regex_search(in_string, m, r_default))
            return;
        auto s = m[0].str();
        if (m[4].matched)
        {
            // some programs write extra as 'beta2-123-123' when we expect 'beta2.123.123'
            // this math skips until m[4] started plus first '-'
            std::replace(s.begin() + (m[4].first - m[0].first) + 1, s.end(), '-', '.');
        }
        v = s;
        o = in_string;
    };

    if (!in_regex.empty())
    {
        std::regex r_in(in_regex);
        std::smatch m;
        if (std::regex_search(o, m, r_in))
        {
            if (m.size() >= 4)
            {
                auto make_int = [&m](int i)
                {
                    return std::stoi(m[i].str());
                };
                v = PackageVersion(PackageVersion::Version(make_int(1), make_int(2), make_int(3)));
            }
            else
            {
                try
                {
                    v = m[0].str();
                }
                catch (std::exception &)
                {
                    // we got exception from converting provided string into version
                    // now we try to find standard regex in found string
                    get_default_version(m[0].str());
                }
            }
        }
    }
    else
        get_default_version(o);
    return { o,v };
}

static auto gatherVersion(const path &program, const String &arg, const String &in_regex)
{
    builder::detail::ResolvableCommand c; // for nice program resolving
    c.setProgram(program);
    if (!arg.empty())
        c.push_back(arg);
    return gatherVersion1(c, in_regex);
}

PackageVersion getVersion(const SwManagerContext &swctx, builder::detail::ResolvableCommand &c, const String &in_regex)
{
    auto &vs = getVersionStorage(swctx);
    static boost::upgrade_mutex m;

    const auto program = c.getProgram();

    boost::upgrade_lock lk(m);
    auto i = vs.versions.find(program);
    if (i != vs.versions.end())
        return i->second;

    boost::upgrade_to_unique_lock lk2(lk);

    auto [o, v] = gatherVersion1(c, in_regex);
    vs.addVersion(program, v, o);
    return vs.versions[program];
}

std::pair<String, PackageVersion> getVersionAndOutput(const SwManagerContext &swctx, const path &program, const String &arg, const String &in_regex)
{
    auto &vs = getVersionStorage(swctx);
    static boost::upgrade_mutex m;

    boost::upgrade_lock lk(m);
    auto i = vs.versions.find(program);
    if (i != vs.versions.end())
        return { i->second.output, i->second.v };

    boost::upgrade_to_unique_lock lk2(lk);

    auto [o, v] = gatherVersion(program, arg, in_regex);
    vs.addVersion(program, v, o);
    return { o, v };
}

PackageVersion getVersion(const SwManagerContext &swctx, const path &program, const String &arg, const String &in_regex)
{
    return getVersionAndOutput(swctx, program, arg, in_regex).second;
}

} // namespace sw
