// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "sw/core/sw_context.h"

#include <sw/builder/command.h>
#include <sw/builder/program.h>

// TODO: actually detect.cpp may be rewritten as entry point

#define DETECT_ARGS SwCoreContext &s

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

VSInstances &gatherVSInstances(DETECT_ARGS);

void log_msg_detect_target(const String &m);

template <class T>
T &addTarget(DETECT_ARGS, const PackageId &id, const TargetSettings &ts)
{
    log_msg_detect_target("Detected target: " + id.toString());

    auto t = std::make_shared<T>(id, ts);
    auto &cld = s.getPredefinedTargets();
    cld[id].push_back(t);
    return *t;
}

void detectNativeCompilers(SwCoreContext &);
void addSettingsAndSetPrograms(const SwCoreContext &, TargetSettings &, bool force = false);

}
