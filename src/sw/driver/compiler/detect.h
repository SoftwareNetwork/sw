// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
            cmd = std::make_shared<builder::Command>(swctx);
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
    log_msg_detect_target("Detected target: " + id.toString());

    auto t = std::make_shared<T>(id, ts);
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
