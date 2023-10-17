// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>


#include "cmake_fe.h"

#include <sw/driver/sw.h>

#include <cmake.h>
#include <cmExecutionStatus.h>
#include <cmGlobalGenerator.h>
#include <cmList.h>
#include <cmMakefile.h>
#include <cmSourceFile.h>
#include <cmState.h>
#include <cmStringAlgorithms.h>
#include <cmTargetPropertyComputer.h>
// commands
#include <cmIncludeCommand.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "fe.cmake");

#define DEFINE_CMAKE_COMMAND(x)                  \
    bool x(std::vector<std::string> const &args, \
           cmExecutionStatus &status)
#define DEFINE_STATIC_CMAKE_COMMAND(x) static DEFINE_CMAKE_COMMAND(x)
#define DEFAULT_CMAKE_CHECK_SET_NAME "cmake"

namespace
{

struct cmakeCxxSourceCompiles : sw::SourceCompiles
{
    using Base = sw::SourceCompiles;

    cmakeCxxSourceCompiles(const String &source, const String &def)
        : Base(def, source) // swap
    {
        setCpp();
    }
};

struct cmakeCxxCompilerFlag : sw::CompilerFlag
{
    using Base = sw::CompilerFlag;

    cmakeCxxCompilerFlag(const String &flag, const String &def)
        : Base(def, flag) // swap
    {
        setCpp();
    }
};

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
        //cm->GetState()->SetGlobalProperty("CMAKE_CXX_KNOWN_FEATURES", "");

        // we can set it here manually
        //cm->SetPolicyVersion("3.18.0", "3.18.0");

        //cm->GetState()->SetCacheEntryValue("CMAKE_PLATFORM_INFO_INITIALIZED", "1");
        //cm->GetState()->SetCacheEntryValue("CMAKE_CFG_INTDIR", "");
        // set some empty cmake vars
        //cm->GetState()->SetCacheEntryValue("CMAKE_C_FLAGS_RELEASE", "");
        //cm->GetState()->SetCacheEntryValue("CMAKE_CXX_FLAGS_RELEASE", "");
    }
};

}

static thread_local sw::driver::cpp::CmakeTargetEntryPoint *cmep;

DEFINE_STATIC_CMAKE_COMMAND(sw_cmIncludeCommand)
{
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
        "CheckIncludeFileCXX",
        "CheckLibraryExists",
        "CheckPrototypeDefinition",
        "CheckStructHasMember",
        "CheckSymbolExists",
        "CheckTypeSize",
        "TestBigEndian",
    };

    if (args.empty())
        return cmIncludeCommand(args, status);

    path i = args[0];

    // pass through
    if (i.is_absolute())
        return cmIncludeCommand(args, status);

    // filter out
    if (overridden_includes.find(i.stem().string()) != overridden_includes.end())
        return true;

    // swallow errors
    if (!cmIncludeCommand(args, status))
        LOG_WARN(logger, "Cannot open: " << args[0]);
    return true;
}

template <class Check, int NArgs = 0>
DEFINE_STATIC_CMAKE_COMMAND(sw_cm_check)
{
    if (args.size() == 0)
        return true;

    sw::Check *i;
    if constexpr (NArgs == 0)
    {
        if (args.size() == 1)
            i = &*cmep->cs->add<Check>(args[0]);
        else
            i = &*cmep->cs->add<Check>(args[0], args[1]);
    }
    else if constexpr (NArgs == 2)
    {
        i = &*cmep->cs->add<Check>(args[0], args[1]);

        for (int n = 2; n < args.size(); n++)
        {
            if (args[n] == "FAIL_REGEX" && n + 1 < args.size())
            {
                static_cast<sw::CompilerFlag *>(i)->fail_regex.push_back(args[n + 1]);
            }
        }
    }
    static_assert(NArgs <= 2);

    auto get_prop = [&status](const String &s) -> Strings
    {
        if (auto prop = status.GetMakefile().GetDefinition(s))
            return cmList{*prop};
        return {};
    };

    for (auto &p : get_prop("CMAKE_REQUIRED_FLAGS"))
        i->Parameters.CompileOptions.push_back(p);
    for (auto &p : get_prop("CMAKE_REQUIRED_DEFINITIONS"))
    {
        if (!p.empty() && p[0] == '-')
        {
            // pass directly
            i->Parameters.CompileOptions.push_back(p);
            continue;
        }
        auto [k, v] = sw::string2definition(p);
        i->Parameters.Definitions[k] = v;
    }
    for (auto &p : get_prop("CMAKE_REQUIRED_INCLUDES"))
        i->Parameters.IncludeDirectories.push_back(p);
    for (auto &p : get_prop("CMAKE_REQUIRED_LINK_OPTIONS"))
        i->Parameters.LinkOptions.push_back(p);
    for (auto &p : get_prop("CMAKE_REQUIRED_LIBRARIES"))
        i->Parameters.Libraries.push_back(p);

    try
    {
        cmep->cs->performChecks(*cmep->b, cmep->ts);
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
    sw::Check *i = &cmep->cs->testBigEndian();
    if (!args.empty())
        i->Definitions.insert(args[0]);

    try
    {
        cmep->cs->performChecks(*cmep->b, cmep->ts);
    }
    catch (std::exception &)
    {
        return false;
    }
    for (auto &d : i->Definitions)
        cmep->cm->AddCacheEntry(d, std::to_string(*i->Value).c_str(), "", cmStateEnums::STRING);
    return true;
}

namespace sw::driver::cpp
{

CmakeTargetEntryPoint::CmakeTargetEntryPoint(const path &fn)
    : rootfn(fn)
{
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

    cm = std::make_unique<cmake>(cmake::RoleProject, cmState::Mode::Project);
    cm->SetHomeDirectory(to_string(normalize_path(rootfn.parent_path())));
    auto bdir = rootfn.parent_path() / ".sw" / "cmake";
    cm->SetHomeOutputDirectory(to_string(normalize_path(bdir)));
    // set install dir, some packages require that
    cm->AddCacheEntry("CMAKE_INSTALL_PREFIX", to_string(normalize_path(bdir / "install")).c_str(), "", cmStateEnums::STRING);

    override_command("include", sw_cmIncludeCommand);
    reset_command("find_package");
    reset_command("install");
    //reset_command("cmake_minimum_required");

    // we also hook and reset our own commands
    reset_command("sw_add_package");
    reset_command("sw_execute");
    cm->AddCacheEntry("SW_BUILD", "1", "", cmStateEnums::STRING);

    // checks
    override_command("check_function_exists", sw_cm_check<FunctionExists>);
    override_command("check_include_files", sw_cm_check<IncludeExists>);
    override_command("check_type_size", sw_cm_check<TypeSize>);
    override_command("check_cxx_source_compiles", sw_cm_check<cmakeCxxSourceCompiles, 2>);
    override_command("check_cxx_compiler_flag", sw_cm_check<cmakeCxxCompilerFlag, 2>);
    override_command("test_big_endian", sw_cm_check_test_big_endian);

    // dev settings
    cm->AddCacheEntry("CMAKE_SUPPRESS_DEVELOPER_WARNINGS", "1", "", cmStateEnums::STRING);

    cm->SetGlobalGenerator(std::make_unique<SwCmakeGenerator>(cm.get()));

    // state will be cleared here, so if you need to set some values, do it in SwCmakeGenerator::EnableLanguage()
    cmep = (sw::driver::cpp::CmakeTargetEntryPoint *)this;
    auto r = cm->Configure();
    if (r < 0)
        throw SW_RUNTIME_ERROR("Cannot parse " + to_string(normalize_path(rootfn)));
}

std::vector<ITargetPtr> CmakeTargetEntryPoint::loadPackages(SwBuild &mb, const TargetSettings &ts, const PackageIdSet &pkgs, const PackagePath &prefix) const
{
    // before init
    this->b = &mb;
    this->ts = ts;

    sw::Build b(mb);
    b.module_data.current_settings = ts;
    b.setSourceDirectory(mb.getBuildDirectory());
    b.BinaryDir = mb.getBuildDirectory();
    t = &b.addLibrary("dummy");
    // checks
    cs = &b.checker.addSet(DEFAULT_CMAKE_CHECK_SET_NAME);
    cs->t = t;

    // init every time because we set settings specific to current request
    init();

    // per settings configuration
    // by default BUILD_SHARED_LIBS is off in cmake, we follow that
    //cm->AddCacheEntry("BUILD_SHARED_LIBS", t->getBuildSettings().Native.LibrariesType == LibraryType::Shared ? "1" : "0", "", cmStateEnums::BOOL);

    return Base::loadPackages(mb, ts, pkgs, prefix);
}

void CmakeTargetEntryPoint::loadPackages1(Build &b) const
{
    //
    auto &mfs = cm->GetGlobalGenerator()->GetMakefiles();

    // gather targets
    StringSet list_of_targets;
    for (auto &mf : mfs)
    {
        auto &ts = mf->GetTargets();
        for (auto &[n, cmt] : ts)
        {
            list_of_targets.insert(n);
        }
    }

    //
    for (auto &mf : mfs)
    {
        auto &ts = mf->GetTargets();
        for (auto &[n, cmt] : ts)
        {
            auto nt = addTarget(b, cmt);
            if (!nt)
                continue;
            setupTarget(*mf, cmt, *nt, list_of_targets);
        }
    }
}

NativeCompiledTarget *CmakeTargetEntryPoint::addTarget(Build &b, cmTarget &cmt)
{
    switch (cmt.GetType())
    {
    case cmStateEnums::TargetType::EXECUTABLE:
        return &b.addExecutable(cmt.GetName());
    case cmStateEnums::TargetType::OBJECT_LIBRARY: // consider as static?
    case cmStateEnums::TargetType::STATIC_LIBRARY:
        return &b.addStaticLibrary(cmt.GetName());
    case cmStateEnums::TargetType::MODULE_LIBRARY: // consider as shared
    case cmStateEnums::TargetType::SHARED_LIBRARY:
        return &b.addSharedLibrary(cmt.GetName());
    case cmStateEnums::TargetType::INTERFACE_LIBRARY: // like header only
    {
        auto nt = &b.addLibrary(cmt.GetName());
        nt->HeaderOnly = true;
        return nt;
    }
    case cmStateEnums::TargetType::UTILITY:
        return nullptr; // skip
                        //GLOBAL_TARGET,
                        //UNKNOWN_LIBRARY
    default:
        SW_UNIMPLEMENTED;
    }
}

void CmakeTargetEntryPoint::setupTarget(cmMakefile &mf, cmTarget &cmt, NativeCompiledTarget &t, const StringSet &list_of_targets) const
{
    // properties
    if (auto prop = cmt.GetProperty("CXX_STANDARD"))
    {
        if (*prop == "11")
            t += cpp11;
        if (*prop == "14")
            t += cpp14;
        if (*prop == "17")
            t += cpp17;
        if (*prop == "20")
            t += cpp20;
    }
    if (auto prop = cmt.GetProperty("CXX_EXTENSIONS"); prop && cmIsOn(*prop))
        t.CPPExtensions = true;
    if (auto prop = cmt.GetProperty("WINDOWS_EXPORT_ALL_SYMBOLS"); prop && cmIsOn(*prop) &&
        t.getBuildSettings().TargetOS.is(OSType::Windows))
        t.ExportAllSymbols = true;

    // sources
    if (auto prop = cmTargetPropertyComputer::GetProperty(&cmt, "SOURCES", mf))
    {
        for (auto &sf : cmList{*prop})
        {
            path p = sf;
            if (p.is_absolute())
            {
                t += p;
                continue;
            }

            auto psf = mf.GetSource(sf);
            if (psf)
            {
                auto fp = psf->ResolveFullPath();
                if (!fp.empty())
                {
                    t += path(fp);
                    continue;
                }
            }

            t += path(sf);
        }
    }

    // defs
    for (auto &d : mf.GetCompileDefinitionsEntries())
    {
        for (auto &def : cmList{d.Value})
            t += Definition(def);
    }
    for (auto &d : cmt.GetCompileDefinitionsEntries())
    {
        for (auto &def : cmList{d.Value})
            t += Definition(def);
    }

    if (auto prop = cmt.GetProperty("INTERFACE_COMPILE_DEFINITIONS"))
    {
        for (auto &def : cmList{*prop})
            t.Public += Definition(def);
    }

    // idirs
    for (auto &i : cmt.GetIncludeDirectoriesEntries())
    {
        for (auto &idir : cmList{i.Value})
            t += IncludeDirectory(idir);
    }

    // ldirs
    for (auto &ld : cmt.GetLinkDirectoriesEntries())
    {
        for (auto &d : cmList{ld.Value})
            t += LinkDirectory(d);
    }

    // libs
    auto add_link_library_to = [&list_of_targets, &settings = t.getBuildSettings()](auto &t, const String &n)
    {
        if (list_of_targets.find(n) != list_of_targets.end())
        {
            t += std::make_shared<Dependency>(n);
            return;
        }

        auto add_syslib = [&t, &settings](const String &n)
        {
            path p = n;
            if (p.has_extension())
                t += SystemLinkLibrary(p); // or link library?
            else if (settings.TargetOS.is(OSType::Windows))
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
                t += u;
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
    };

    for (auto &[n, type] : cmt.GetOriginalLinkLibraries())
    {
        add_link_library_to(t, n);
    }

    // more libs
    for (auto &li : cmt.GetLinkImplementationEntries())
    {
        for (auto &n : cmList{li.Value})
        {
            add_link_library_to(t, n);
        }
    }

    // public libs
    if (auto prop = cmt.GetProperty("INTERFACE_LINK_LIBRARIES"))
    {
        for (auto &n : cmList{*prop})
        {
            add_link_library_to(t.Public, n);
        }
    }
}

}
