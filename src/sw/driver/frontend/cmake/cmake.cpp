// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#include "cmake_fe.h"

#include <sw/driver/build.h>
#include <sw/driver/target/native.h>

#include <cmake.h>
#include <cmGlobalGenerator.h>
#include <cmMakefile.h>
#include <cmState.h>
#include <cmStringAlgorithms.h>
#include <cmTargetPropertyComputer.h>
// commands
#include <cmIncludeCommand.h>
#include <cmProjectCommand.h>

static bool sw_cmIncludeCommand(std::vector<std::string> const &args,
    cmExecutionStatus &status)
{
    return cmIncludeCommand(args, status);
}

static bool sw_cmProjectCommand(std::vector<std::string> const &args,
    cmExecutionStatus &status)
{
    // we can add real cmProjectCommand from cmake and remove makefile.AddLanguages() call there
    return true;
    //return cmProjectCommand(args, status);
}

namespace sw::driver::cpp
{

CmakeTargetEntryPoint::CmakeTargetEntryPoint(const path &fn)
    : rootfn(fn)
{
    cm = std::make_unique<cmake>(cmake::RoleProject, cmState::Mode::Project);
    cm->SetHomeDirectory(normalize_path(fn.parent_path()));
    cm->SetHomeOutputDirectory(normalize_path(fn.parent_path() / ".sw" / "cmake"));
    //cm.SetWorkingMode(cmake::SCRIPT_MODE);
    cm->GetState()->RemoveBuiltinCommand("include");
    cm->GetState()->AddBuiltinCommand("include", sw_cmIncludeCommand);
    cm->GetState()->RemoveBuiltinCommand("project");
    cm->GetState()->AddBuiltinCommand("project", sw_cmProjectCommand);
    //cm.GetState()->SetCacheValue("CMAKE_PLATFORM_INFO_INITIALIZED", "1");
    auto r = cm->Configure();
    if (r < 0)
        throw SW_RUNTIME_ERROR("Cannot parse " + normalize_path(fn));
}

void CmakeTargetEntryPoint::loadPackages1(Build &b) const
{
    auto &mfs = cm->GetGlobalGenerator()->GetMakefiles();
    for (auto &mf : mfs)
    {
        auto &ts = mf->GetTargets();
        for (auto &[n, cmt] : ts)
        {
            NativeCompiledTarget *nt;
            switch (cmt.GetType())
            {
            case cmStateEnums::TargetType::EXECUTABLE:
                nt = &b.addExecutable(cmt.GetName());
                break;
            case cmStateEnums::TargetType::OBJECT_LIBRARY: // consider as static?
            case cmStateEnums::TargetType::STATIC_LIBRARY:
                nt = &b.addStaticLibrary(cmt.GetName());
                break;
            case cmStateEnums::TargetType::MODULE_LIBRARY: // consider as shared
            case cmStateEnums::TargetType::SHARED_LIBRARY:
                nt = &b.addSharedLibrary(cmt.GetName());
                break;
            case cmStateEnums::TargetType::INTERFACE_LIBRARY: // like header only
                nt = &b.addLibrary(cmt.GetName());
                nt->HeaderOnly = true;
                break;
            //UTILITY,
            //GLOBAL_TARGET,
            //UNKNOWN_LIBRARY
            default:
                SW_UNIMPLEMENTED;
            }

            auto &t = *nt;
            cmListFileBacktrace bt;
            auto prop = cmTargetPropertyComputer::GetProperty(&cmt, "SOURCES", cm->GetMessenger(), bt);
            for (auto &sf : cmExpandedList(*prop))
                t += path(sf);
            for (auto &[n,type] : cmt.GetOriginalLinkLibraries())
                t += std::make_shared<Dependency>(n);
        }
    }
}

}
