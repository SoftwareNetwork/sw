// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>


#include "cmake_fe.h"

#include <sw/driver/build.h>
#include <sw/driver/checks.h>
#include <sw/driver/target/native.h>

#include <cmake.h>
#include <cmGlobalGenerator.h>
#include <cmMakefile.h>
#include <cmState.h>
#include <cmStringAlgorithms.h>
#include <cmTargetPropertyComputer.h>
// commands
#include <cmIncludeCommand.h>

#define DEFINE_CMAKE_COMMAND(x)                  \
    bool x(std::vector<std::string> const &args, \
           cmExecutionStatus &status)
#define DEFINE_STATIC_CMAKE_COMMAND(x) static DEFINE_CMAKE_COMMAND(x)
#define DEFAULT_CMAKE_CHECK_SET_NAME "cmake"

static thread_local sw::driver::cpp::CmakeTargetEntryPoint *cmep;

DEFINE_STATIC_CMAKE_COMMAND(sw_cmIncludeCommand)
{
    if (args.empty())
        return cmIncludeCommand(args, status);

    static StringSet overridden_includes
    {
        "CheckCCompilerFlag",
        "CheckCXXCompilerFlag",
        "CheckCSourceCompiles",
        "CheckCSourceRuns",
        "CheckCXXSourceCompiles",
        "CheckCXXSourceRuns",
        "CheckFunctionExists",
        "CheckIncludeFiles",
        "CheckLibraryExists",
        "CheckPrototypeDefinition",
        "CheckStructHasMember",
        "CheckSymbolExists",
        "CheckTypeSize",
        "TestBigEndian",
    };
    path i = args[0];
    if (overridden_includes.find(i.filename().u8string()) != overridden_includes.end())
        return true;
    return cmIncludeCommand(args, status);
}

template <class Check>
DEFINE_STATIC_CMAKE_COMMAND(sw_cm_check)
{
    if (args.size() == 0)
        return true;
    sw::Checker c(*cmep->b);
    auto &s = c.addSet(DEFAULT_CMAKE_CHECK_SET_NAME);
    sw::Check *i;
    if (args.size() == 1)
        i = &*s.add<Check>(args[0]);
    else
        i = &*s.add<Check>(args[0], args[1]);
    try
    {
        s.t = cmep->t;
        s.performChecks(*cmep->b, cmep->ts);
    }
    catch (std::exception &)
    {
        return false;
    }
    for (auto &d : i->Definitions)
        cmep->cm->AddCacheEntry(d, std::to_string(*i->Value).c_str(), "", cmStateEnums::STRING);
    return true;
}

DEFINE_STATIC_CMAKE_COMMAND(sw_cm_check_test_big_endian)
{
    sw::Checker c(*cmep->b);
    auto &s = c.addSet(DEFAULT_CMAKE_CHECK_SET_NAME);
    sw::Check *i = &s.testBigEndian();
    if (!args.empty())
        i->Definitions.insert(args[0]);
    try
    {
        s.t = cmep->t;
        s.performChecks(*cmep->b, cmep->ts);
    }
    catch (std::exception &)
    {
        return false;
    }
    for (auto &d : i->Definitions)
        cmep->cm->AddCacheEntry(d, std::to_string(*i->Value).c_str(), "", cmStateEnums::STRING);
    return true;
}

namespace
{

struct SwCmakeGenerator : cmGlobalGenerator
{
    using Base = cmGlobalGenerator;

    SwCmakeGenerator(cmake* cm)
        : Base(cm)
    {
    }

    void EnableLanguage(std::vector<std::string> const &languages,
        cmMakefile *cm, bool optional) override
    {
        // allow to use BUILD_SHARED_LIBS
        cm->GetState()->SetGlobalProperty("TARGET_SUPPORTS_SHARED_LIBS", "1");

        // add empty list
        cm->GetState()->SetGlobalProperty("CMAKE_CXX_KNOWN_FEATURES", "");

        //cm->GetState()->SetCacheEntryValue("CMAKE_PLATFORM_INFO_INITIALIZED", "1");
        //cm->GetState()->SetCacheEntryValue("CMAKE_CFG_INTDIR", "");
        // set some empty cmake vars
        //cm->GetState()->SetCacheEntryValue("CMAKE_C_FLAGS_RELEASE", "");
        //cm->GetState()->SetCacheEntryValue("CMAKE_CXX_FLAGS_RELEASE", "");
    }
};

}

namespace sw::driver::cpp
{

CmakeTargetEntryPoint::CmakeTargetEntryPoint(const path &fn)
    : rootfn(fn)
{
    cm = std::make_unique<cmake>(cmake::RoleProject, cmState::Mode::Project);
}

CmakeTargetEntryPoint::~CmakeTargetEntryPoint()
{
}

void CmakeTargetEntryPoint::init() const
{
    auto override_command = [this](const String &name, auto cmd)
    {
        cm->GetState()->RemoveBuiltinCommand(name);
        cm->GetState()->AddBuiltinCommand(name, cmd);
    };

    auto reset_command = [&override_command](const String &name)
    {
        override_command(name, [](std::vector<std::string> const &, cmExecutionStatus &){return true;});
    };

    cm->SetHomeDirectory(normalize_path(rootfn.parent_path()));
    cm->SetHomeOutputDirectory(normalize_path(rootfn.parent_path() / ".sw" / "cmake"));

    override_command("include", sw_cmIncludeCommand);
    reset_command("find_package");
    reset_command("install");
    reset_command("cmake_minimum_required");

    // we also hook and reset our own commands
    reset_command("sw_add_package");
    reset_command("sw_execute");
    cm->AddCacheEntry("SW_BUILD", "1", "", cmStateEnums::STRING);

    // checks
    override_command("check_function_exists", sw_cm_check<FunctionExists>);
    override_command("check_include_files", sw_cm_check<IncludeExists>);
    override_command("test_big_endian", sw_cm_check_test_big_endian);

    // dev settings
    cm->AddCacheEntry("CMAKE_SUPPRESS_DEVELOPER_WARNINGS", "1", "", cmStateEnums::STRING);

    cm->SetGlobalGenerator(std::make_unique<SwCmakeGenerator>(cm.get()));

    // state will be cleared here, so if you need to set some values, do it in SwCmakeGenerator::EnableLanguage()
    cmep = (sw::driver::cpp::CmakeTargetEntryPoint *)this;
    auto r = cm->Configure();
    if (r < 0)
        throw SW_RUNTIME_ERROR("Cannot parse " + normalize_path(rootfn));
}

std::vector<ITargetPtr> CmakeTargetEntryPoint::loadPackages(SwBuild &mb, const TargetSettings &ts, const PackageIdSet &pkgs, const PackagePath &prefix) const
{
    this->b = &mb;
    this->ts = ts;

    sw::Build b(mb);
    b.module_data.current_settings = ts;
    b.setSourceDirectory(mb.getBuildDirectory());
    b.BinaryDir = mb.getBuildDirectory();
    t = &b.addLibrary("dummy");

    // per settings configuration
    cm->AddCacheEntry("BUILD_SHARED_LIBS", t->getBuildSettings().Native.LibrariesType == LibraryType::Shared ? "1" : "0", "", cmStateEnums::BOOL);

    return Base::loadPackages(mb, ts, pkgs, prefix);
}

void CmakeTargetEntryPoint::loadPackages1(Build &b) const
{
    std::call_once(f, &CmakeTargetEntryPoint::init, this);

    //
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
            case cmStateEnums::TargetType::UTILITY:
                continue; // skip
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
            for (auto &ld : cmt.GetLinkDirectoriesEntries())
            {
                for (auto &d : cmExpandedList(ld))
                    t += LinkDirectory(d);
            }
            for (auto &[n, type] : cmt.GetOriginalLinkLibraries())
            {
                if (n.empty())
                    continue;

                auto add_syslib = [&t](const String &n)
                {
                    path p = n;
                    if (p.has_extension())
                        t += SystemLinkLibrary(p); // or link library?
                    else if (t.getBuildSettings().TargetOS.is(OSType::Windows))
                        t += SystemLinkLibrary(p += ".lib");
                    else
                        t += SystemLinkLibrary(p); // or link library?
                };

                try
                {
                    UnresolvedPackage u(n);
                    if (u.getPath().size() == 1)
                    {
                        // probably system link library
                        add_syslib(n);
                    }
                    else
                        t += std::make_shared<Dependency>(n);
                }
                catch (...)
                {
                    // link option?
                    if (n[0] == '-')
                    {

                    }
                    else
                    {
                        add_syslib(n);
                    }
                }
            }
            for (auto &d : mf->GetCompileDefinitionsEntries())
            {
                for (auto &def : cmExpandedList(d))
                    t += Definition(def);
            }
            for (auto &d : cmt.GetCompileDefinitionsEntries())
            {
                for (auto &def : cmExpandedList(d))
                    t += Definition(def);
            }
            for (auto &i : cmt.GetIncludeDirectoriesEntries())
            {
                for (auto &idir : cmExpandedList(i))
                t += IncludeDirectory(idir);
            }
        }
    }
}

}
