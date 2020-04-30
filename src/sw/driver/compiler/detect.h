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

#include "sw/core/sw_context.h"

#include <sw/builder/command.h>
#include <sw/builder/program.h>

// TODO: actually detect.cpp may be rewritten as entry point

#define DETECT_ARGS ::sw::SwCoreContext &s, ::sw::TargetMap &tm
#define DETECT_ARGS_PASS s, tm
#define DETECT_ARGS_PASS_TO_LAMBDA &s, &tm
#define DETECT_ARGS_PASS_FIRST_CALL(ctx) (::sw::SwContext&)(ctx), ((::sw::SwContext&)ctx).getPredefinedTargets()
#define DETECT_ARGS_PASS_FIRST_CALL_SIMPLE DETECT_ARGS_PASS_FIRST_CALL(getContext())

namespace sw
{

struct PredefinedProgramTarget : PredefinedTarget, PredefinedProgram
{
    using PredefinedTarget::PredefinedTarget;
};

struct SimpleProgram : Program
{
    using Program::Program;

    std::shared_ptr<Program> clone() const override { return std::make_shared<SimpleProgram>(*this); }
    std::shared_ptr<builder::Command> getCommand() const override
    {
        if (!cmd)
        {
            cmd = std::make_shared<builder::Command>();
            cmd->setProgram(file);
        }
        return cmd;
    }

private:
    mutable std::shared_ptr<builder::Command> cmd;
};

PredefinedProgramTarget &addProgram(DETECT_ARGS, const PackageId &id, const TargetSettings &ts, const std::shared_ptr<Program> &p);

struct VSInstance
{
    path root;
    Version version;
};

using VSInstances = VersionMap<VSInstance>;
VSInstances &gatherVSInstances();

void log_msg_detect_target(const String &m);

template <class T>
T &addTarget(DETECT_ARGS, const PackageId &id, const TargetSettings &ts)
{
    log_msg_detect_target("Detected target: " + id.toString() + ": " + ts.toString());

    auto t = std::make_shared<T>(sw::LocalPackage(s.getLocalStorage(), id), ts);
    tm[id].push_back(t);
    return *t;
}

// combined function for users
SW_DRIVER_CPP_API
void detectProgramsAndLibraries(DETECT_ARGS);

#define DETECT(x) void detect##x##Compilers(DETECT_ARGS);
#include "detect.inl"
#undef DETECT

void addSettingsAndSetPrograms(const SwCoreContext &, TargetSettings &);
void addSettingsAndSetHostPrograms(const SwCoreContext &, TargetSettings &);

}
