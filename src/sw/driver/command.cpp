/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

 // for serialization
#include <primitives/filesystem.h>
#include <boost/serialization/access.hpp>
#include <sw/support/serialization.h>
#include <boost/serialization/export.hpp>
#include <boost/serialization/void_cast.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <sw/builder/command_storage.h>
//
#include "command.h"

#include "build.h"
#include "target/native.h"
#include "program_version_storage.h"

#include <sw/builder/platform.h>
#include <sw/core/sw_context.h>

#include <primitives/symbol.h>

#include <boost/algorithm/string.hpp>
#include <boost/dll.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <pystring.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "cpp.command");

BOOST_CLASS_EXPORT(::sw::driver::Command)
BOOST_CLASS_EXPORT(::sw::driver::VSCommand)
BOOST_CLASS_EXPORT(::sw::driver::GNUCommand)

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

#ifdef BOOST_SERIALIZATION_ACCESS_HPP
    friend class boost::serialization::access;
    template <class Ar>
    void serialize(Ar &ar, unsigned)
    {
        ar & boost::serialization::base_object<::primitives::command::Argument>(*this);
        auto s = toString();
        ar & s;
        cached_value = s;
    }
#endif
};
BOOST_CLASS_EXPORT(LazyArgument)

static struct register_s11n_casts
{
    register_s11n_casts()
    {
        boost::serialization::void_cast_register((::sw::builder::Command*)0, (::sw::CommandNode*)0);
        boost::serialization::void_cast_register((::sw::driver::detail::Command*)0, (::sw::builder::Command*)0);
        boost::serialization::void_cast_register((::sw::driver::Command*)0, (::sw::driver::detail::Command*)0);
        boost::serialization::void_cast_register((::sw::driver::VSCommand*)0, (::sw::driver::Command*)0);
        boost::serialization::void_cast_register((::sw::driver::GNUCommand*)0, (::sw::driver::Command*)0);
        boost::serialization::void_cast_register((LazyArgument*)0, (::primitives::command::Argument*)0);
        boost::serialization::void_cast_register((::primitives::command::SimpleArgument*)0, (::primitives::command::Argument*)0);
        boost::serialization::void_cast_register((::primitives::command::SimplePositionalArgument*)0, (::primitives::command::SimpleArgument*)0);
    }
} _______x;

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
        auto d = dependency.lock();
        if (d)
        {
            auto &t = d->getTarget();
            if (auto nt = t.as<NativeTarget *>())
                nt->setupCommand(*this);
            else if (auto nt = t.as<PredefinedTarget *>())
            {
                const auto &is = nt->getInterfaceSettings();
                if (is["run_command"])
                {
                    for (auto &[k, v] : is["run_command"]["environment"].getSettings())
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
                    if (!p.empty() && !File(p, getContext().getFileStorage()).isGenerated())
                    {
                        if (*nt->HeaderOnly)
                            throw SW_RUNTIME_ERROR("Program is used from package: " + t.getPackage().toString() + " which is header only");
                        if (!File(p, getContext().getFileStorage()).isGeneratedAtAll())
                            throw SW_RUNTIME_ERROR("Program from package: " + t.getPackage().toString() + " is not generated at all: " + normalize_path(p));
                        throw SW_RUNTIME_ERROR("Program from package: " + t.getPackage().toString() + " is not generated: " + normalize_path(p));
                    }
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
                    p = of.getValue();
                }
                else
                    throw SW_RUNTIME_ERROR("Package: " + t.getPackage().toString() + " has unknown type");

                if (p.empty())
                    throw SW_RUNTIME_ERROR("Empty program from package: " + t.getPackage().toString());
                setProgram(p);
            }
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

        auto &t = d->getTarget();
        if (auto nt = t.as<NativeTarget *>())
            nt->setupCommand(*this);
        else if (auto nt = t.as<PredefinedTarget *>())
        {
            const auto &is = nt->getInterfaceSettings();
            if (is["run_command"])
            {
                for (auto &[k, v] : is["run_command"]["environment"].getSettings())
                {
                    if (k == "PATH")
                        appendEnvironmentArrayValue(k, v.getValue(), true);
                    else
                        environment[k] = v.getValue();
                }
            }
        }
        else
            throw SW_RUNTIME_ERROR("missing predefined target code");
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
    String prefix;
    auto &p = getMsvcIncludePrefixes();
    auto i = p.find(getProgram());
    if (i == p.end())
    {
        // clangcl uses this one (default)
        // with or without space? clang has some code with '.' instead of ' '
        //prefix = "Note: including file: ";
        throw SW_RUNTIME_ERROR("Cannot find msvc prefix");
    }
    else
        prefix = i->second;

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

    if (deps_file.empty() || !has_deps)
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
    //  use exactly ": " because on windows target is 'C:/path/to/file: '
    //                                           skip up to this space ^
    f = f.substr(f.find(": ") + 1);

    FilesOrdered files;

    enum
    {
        EMPTY,
        FILE,
    };
    int state = EMPTY;
    auto p = f.c_str();
    auto begin = p;
    while (*p)
    {
        switch (state)
        {
        case EMPTY:
            if (isspace(*p) || *p == '\\')
                break;
            state = FILE;
            begin = p;
            break;
        case FILE:
            if (!isspace(*p))
                break;
            if (*(p - 1) == '\\')
                break;
            String s(begin, p);
            if (!s.empty())
            {
                boost::replace_all(s, "\\ ", " ");
                if (pystring::endswith(s, "\\\n")) // protobuf does not put space after filename
                    s.resize(s.size() - 2);
                files.push_back(fs::u8path(s));
            }
            state = EMPTY;
            break;
        }
        p++;
    }

#ifndef _WIN32
    for (auto &f : files)
        addImplicitInput(f);
#else
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
        //if (!fs::exists(fs::u8path(f3)))
        addImplicitInput(fs::u8path(f3));
    }
#endif
}

///

CommandBuilder::CommandBuilder(const SwBuilderContext &swctx)
{
    c = std::make_shared<Command>(swctx);
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

const CommandBuilder &operator<<(const CommandBuilder &cb, const Target &t)
{
    auto nt = (Target *)&t;
    cb.targets.push_back(nt);
    nt->Storage.push_back(cb.c);
    return cb;
}

const CommandBuilder &operator<<(const CommandBuilder &cb, const ::sw::cmd::tag_wdir &t)
{
    auto p = t.p;
    if (p.is_relative() && !cb.targets.empty())
        p = cb.targets[0]->SourceDir / p;

    cb.c->working_directory = p;
    return cb;
}

static NativeCompiledTarget &cast_as_nct(Target &t)
{
    return dynamic_cast<NativeCompiledTarget &>(t);
}
static NativeCompiledTarget *cast_as_nct(Target *t)
{
    return &cast_as_nct(*t);
}

const CommandBuilder &operator<<(const CommandBuilder &cb, const ::sw::cmd::tag_in &t)
{
    decltype(cb.targets) all;
    all.insert(all.end(), cb.targets.begin(), cb.targets.end());
    all.insert(all.end(), t.targets.begin(), t.targets.end());

    if (!all.empty() && all[0]->DryRun)
        return cb;

    for (auto p : t.files)
    {
        if (p.is_relative() && !all.empty())
            if (!cast_as_nct(all[0])->check_absolute(p, true))
                p = all[0]->SourceDir / p;

        if (!cb.stopped)
            cb.c->arguments.push_back(t.prefix + (t.normalize ? normalize_path(p) : p.u8string()));
        cb.c->addInput(p);
        if (t.add_to_targets)
        {
            for (auto tgt : all)
            {
                cast_as_nct(*tgt).getMergeObject() += p;
                cast_as_nct(*tgt).getMergeObject()[p].skip = t.skip;
                // also add into private
                cast_as_nct(*tgt).add(cast_as_nct(*tgt).getMergeObject().getFileInternal(p));
            }
        }
    }
    for (auto tgt : t.targets)
        tgt->Storage.push_back(cb.c);
    return cb;
}

const CommandBuilder &operator<<(const CommandBuilder &cb, const ::sw::cmd::tag_out &t)
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
            if (!cast_as_nct(all[0])->check_absolute(p, true))
                p = all[0]->BinaryDir / p;

        if (!cb.stopped)
            cb.c->arguments.push_back(t.prefix + (t.normalize ? normalize_path(p) : p.u8string()));
        if (!dry_run)
            cb.c->addOutput(p);
        if (t.add_to_targets)
        {
            for (auto tgt : all)
            {
                cast_as_nct(*tgt).getMergeObject() += p;
                cast_as_nct(*tgt).getMergeObject()[p].skip = t.skip;
                // also add into private
                cast_as_nct(*tgt).add(cast_as_nct(*tgt).getMergeObject().getFileInternal(p));
            }
        }
    }
    for (auto tgt : t.targets)
        tgt->Storage.push_back(cb.c);
    return cb;
}

const CommandBuilder &operator<<(const CommandBuilder &cb, const ::sw::cmd::tag_stdin &t)
{
    decltype(cb.targets) all;
    all.insert(all.end(), cb.targets.begin(), cb.targets.end());
    all.insert(all.end(), t.targets.begin(), t.targets.end());

    auto p = t.p;
    if (p.is_relative() && !all.empty())
        if (!cast_as_nct(all[0])->check_absolute(p, true))
            p = all[0]->SourceDir / p;

    cb.c->redirectStdin(p);
    if (t.add_to_targets)
    {
        for (auto tgt : all)
        {
            cast_as_nct(*tgt).getMergeObject() += p;
            cast_as_nct(*tgt).getMergeObject()[p].skip = t.skip;
            // also add into private
            cast_as_nct(*tgt).add(cast_as_nct(*tgt).getMergeObject().getFileInternal(p));
        }
    }
    for (auto tgt : t.targets)
        tgt->Storage.push_back(cb.c);
    return cb;
}

const CommandBuilder &operator<<(const CommandBuilder &cb, const ::sw::cmd::tag_stdout &t)
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
        if (!cast_as_nct(all[0])->check_absolute(p, true))
            p = all[0]->BinaryDir / p;

    if (!dry_run)
        cb.c->redirectStdout(p, t.append);
    if (t.add_to_targets)
    {
        for (auto tgt : all)
        {
            cast_as_nct(*tgt).getMergeObject() += p;
            cast_as_nct(*tgt).getMergeObject()[p].skip = t.skip;
            // also add into private
            cast_as_nct(*tgt).add(cast_as_nct(*tgt).getMergeObject().getFileInternal(p));
        }
    }
    for (auto tgt : t.targets)
        tgt->Storage.push_back(cb.c);
    return cb;
}

const CommandBuilder &operator<<(const CommandBuilder &cb, const ::sw::cmd::tag_stderr &t)
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
        if (!cast_as_nct(all[0])->check_absolute(p, true))
            p = all[0]->BinaryDir / p;

    if (!dry_run)
        cb.c->redirectStderr(p, t.append);
    if (t.add_to_targets)
    {
        for (auto tgt : all)
        {
            cast_as_nct(*tgt).getMergeObject() += p;
            cast_as_nct(*tgt).getMergeObject()[p].skip = t.skip;
            // also add into private
            cast_as_nct(*tgt).add(cast_as_nct(*tgt).getMergeObject().getFileInternal(p));
        }
    }
    for (auto tgt : t.targets)
        tgt->Storage.push_back(cb.c);
    return cb;
}

const CommandBuilder &operator<<(const CommandBuilder &cb, const ::sw::cmd::tag_end &t)
{
    cb.stopped = true;
    return cb;
}

const CommandBuilder &operator<<(const CommandBuilder &cb, const ::sw::cmd::tag_dep &t)
{
    SW_UNIMPLEMENTED;
}

const CommandBuilder &operator<<(const CommandBuilder &cb, const ::sw::cmd::tag_env &t)
{
    cb.c->environment[t.k] = t.v;
    return cb;
}

const CommandBuilder &operator<<(const CommandBuilder &cb, const ::sw::cmd::tag_prog_dep &t)
{
    std::dynamic_pointer_cast<::sw::driver::Command>(cb.c)->setProgram(t.d);
    for (auto tgt : cb.targets)
        tgt->addDummyDependency(t.d);
    return cb;
}

const CommandBuilder &operator<<(const CommandBuilder &cb, const ::sw::cmd::tag_prog_prog &t)
{
    cb.c->setProgram(t.p);
    return cb;
}

const CommandBuilder &operator<<(const CommandBuilder &cb, const ::sw::cmd::tag_prog_tgt &t)
{
    auto d = std::make_shared<Dependency>(*t.t);
    cb << cmd::prog(d);
    return cb;
}

const CommandBuilder &operator<<(const CommandBuilder &cb, const Command::LazyCallback &t)
{
    if (!cb.stopped)
        cb.c->arguments.push_back(std::make_unique<LazyArgument>(t));
    return cb;
}

} // namespace driver

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

static Version gatherVersion1(builder::detail::ResolvableCommand &c, const String &in_regex)
{
    error_code ec;
    c.execute(ec);

    if (c.pid == -1)
        throw SW_RUNTIME_ERROR(normalize_path(c.getProgram()) + ": " + ec.message());

    Version v;
    if (!in_regex.empty())
    {
        std::regex r_in(in_regex);
        std::smatch m;
        if (std::regex_search(c.err.text.empty() ? c.out.text : c.err.text, m, r_in))
            v = m[0].str();
    }
    else
    {
        static std::regex r_default("(\\d+)(\\.(\\d+)){2,}(-[[:alnum:]]+([.-][[:alnum:]]+)*)?");
        std::smatch m;
        if (std::regex_search(c.err.text.empty() ? c.out.text : c.err.text, m, r_default))
        {
            auto s = m[0].str();
            if (m[4].matched)
            {
                // some programs write extra as 'beta2-123-123' when we expect 'beta2.123.123'
                // this math skips until m[4] started plus first '-'
                std::replace(s.begin() + (m[4].first - m[0].first) + 1, s.end(), '-', '.');
            }
            v = s;
        }
    }
    return v;
}

static Version gatherVersion(const path &program, const String &arg, const String &in_regex)
{
    builder::detail::ResolvableCommand c; // for nice program resolving
    c.setProgram(program);
    if (!arg.empty())
        c.push_back(arg);
    return gatherVersion1(c, in_regex);
}

Version getVersion(const SwManagerContext &swctx, builder::detail::ResolvableCommand &c, const String &in_regex)
{
    auto &vs = getVersionStorage(swctx);
    static boost::upgrade_mutex m;

    const auto program = c.getProgram();

    boost::upgrade_lock lk(m);
    auto i = vs.versions.find(program);
    if (i != vs.versions.end())
        return i->second;

    boost::upgrade_to_unique_lock lk2(lk);

    vs.addVersion(program, gatherVersion1(c, in_regex));
    return vs.versions[program];
}

Version getVersion(const SwManagerContext &swctx, const path &program, const String &arg, const String &in_regex)
{
    auto &vs = getVersionStorage(swctx);
    static boost::upgrade_mutex m;

    boost::upgrade_lock lk(m);
    auto i = vs.versions.find(program);
    if (i != vs.versions.end())
        return i->second;

    boost::upgrade_to_unique_lock lk2(lk);

    vs.addVersion(program, gatherVersion(program, arg, in_regex));
    return vs.versions[program];
}

} // namespace sw
