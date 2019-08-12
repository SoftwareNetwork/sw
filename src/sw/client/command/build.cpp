/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2019 Egor Pugin
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

#include "commands.h"

#include <sw/builder/execution_plan.h>
#include <sw/core/input.h>

#include <boost/algorithm/string.hpp>

DEFINE_SUBCOMMAND(build, "Build files, dirs or packages");

extern ::cl::opt<bool> build_after_fetch;

static ::cl::list<String> build_arg(::cl::Positional, ::cl::desc("Files or directories to build (paths to config)"), ::cl::sub(subcommand_build));

static ::cl::opt<String> build_source_dir("S", ::cl::desc("Explicitly specify a source directory."), ::cl::sub(subcommand_build), ::cl::init("."));
static ::cl::opt<String> build_binary_dir("B", ::cl::desc("Explicitly specify a build directory."), ::cl::sub(subcommand_build), ::cl::init(SW_BINARY_DIR));

static ::cl::opt<bool> build_fetch("fetch", ::cl::desc("Fetch sources, then build"), ::cl::sub(subcommand_build));
static ::cl::opt<path> build_explan("ef", ::cl::desc("Build execution plan from specified file"), ::cl::sub(subcommand_build));
static ::cl::opt<bool> build_default_explan("e", ::cl::desc("Build execution plan"), ::cl::sub(subcommand_build));

////////////////////////////////////////////////////////////////////////////////
//
// build configs
//
////////////////////////////////////////////////////////////////////////////////

//static cl::opt<bool> append_configs("append-configs", cl::desc("Append configs for generation"));

static cl::list<String> target_os("target-os", cl::CommaSeparated);
cl::list<String> compiler("compiler", cl::desc("Set compiler"), cl::CommaSeparated);
cl::list<String> configuration("configuration", cl::desc("Set build configuration"), cl::CommaSeparated);
static cl::alias configuration2("config", cl::desc("Alias for -configuration"), cl::aliasopt(configuration));
static cl::list<String> platform("platform", cl::desc("Set build platform"), cl::CommaSeparated);
static cl::alias platform2("arch", cl::desc("Alias for -platform"), cl::aliasopt(platform));
static cl::list<String> os("os", cl::desc("Set build target os"), cl::CommaSeparated);
// rename to stdc, stdcpp?
static cl::list<String> libc("libc", cl::desc("Set build libc"), cl::CommaSeparated);
static cl::list<String> libcpp("libcpp", cl::desc("Set build libcpp"), cl::CommaSeparated);

static ::cl::opt<bool> static_deps("static-dependencies", ::cl::desc("Build static dependencies of inputs"));
static cl::alias static_deps2("static-deps", cl::aliasopt(static_deps));

// -setting k1=v1,k2=v2,k3="v3,v3" -setting k4=v4,k5,k6 etc.
// settings in one setting applied simultaneosly
// settings in different settings are multiplied
// k=v assigns value to dot separated key
// complex.key.k1 means s["complex"]["key"]["k1"]
// k= or k="" means empty value
// k means reseted value
static cl::list<String> settings("settings", cl::desc("Set settings directly"), cl::ZeroOrMore);
static cl::list<path> settings_file("settings-file", cl::desc("Read settings from file"), cl::ZeroOrMore);
static cl::list<String> settings_json("settings-json", cl::desc("Read settings from json string"), cl::ZeroOrMore);
static cl::opt<path> host_settings_file("host-settings-file", cl::desc("Read host settings from file"));

// static/shared
static cl::opt<bool> static_build("static-build", cl::desc("Set static build"));
static cl::alias static_build2("static", cl::desc("Alias for -static-build"), cl::aliasopt(static_build));
static cl::opt<bool> shared_build("shared-build", cl::desc("Set shared build (default)"));
static cl::alias shared_build2("shared", cl::desc("Alias for -shared-build"), cl::aliasopt(shared_build));

//mt/md
static cl::opt<bool> win_mt("win-mt", cl::desc("Set /MT build"));
static cl::alias win_mt2("mt", cl::desc("Alias for -win-mt"), cl::aliasopt(win_mt));
static cl::opt<bool> win_md("win-md", cl::desc("Set /MD build (default)"));
static cl::alias win_md2("md", cl::desc("Alias for -win-md"), cl::aliasopt(win_md));

////////////////////////////////////////////////////////////////////////////////

SUBCOMMAND_DECL(build)
{
    if (build_arg.empty())
        build_arg.push_back(".");

    auto swctx = createSwContext();
    cli_build(*swctx);
}

static sw::TargetSettings compilerTypeFromStringCaseI(const sw::UnresolvedPackage &compiler)
{
    sw::TargetSettings ts;

    auto with_version = [&compiler](const auto &ppath)
    {
        return sw::UnresolvedPackage(ppath, compiler.range);
    };

    auto set_with_version = [&with_version](const auto &ppath)
    {
        return with_version(ppath).toString();
    };

    if (0);
    /*
    // starts with
    else if (boost::istarts_with(compiler, "appleclang") || boost::iequals(compiler, "apple-clang"))
        return CompilerType::AppleClang;
    */
    // g++ is not possible for package path
    else if (compiler.ppath == "gcc" || compiler.ppath == "gnu")
    {
        ts["native"]["program"]["c"] = set_with_version("org.gnu.gcc");
        ts["native"]["program"]["cpp"] = set_with_version("org.gnu.gpp");
        ts["native"]["program"]["asm"] = set_with_version(ts["native"]["program"]["c"].getValue());
    }
    else if (compiler.ppath == "clang")
    {
        ts["native"]["program"]["c"] = set_with_version("org.LLVM.clang");
        ts["native"]["program"]["cpp"] = set_with_version("org.LLVM.clangpp");
        ts["native"]["program"]["asm"] = set_with_version(ts["native"]["program"]["c"].getValue());
    }
    // clang-cl is not possible for package path
    else if (compiler.ppath == "clangcl"/* || compiler.ppath == "clang-cl"*/)
    {
        ts["native"]["program"]["c"] = set_with_version("org.LLVM.clangcl");
        ts["native"]["program"]["cpp"] = set_with_version("org.LLVM.clangcl");
    }
    else if (compiler.ppath == "msvc" || compiler.ppath == "vs")
    {
        ts["native"]["program"]["c"] = set_with_version("com.Microsoft.VisualStudio.VC.cl");
        ts["native"]["program"]["cpp"] = set_with_version("com.Microsoft.VisualStudio.VC.cl");
        ts["native"]["program"]["asm"] = set_with_version("com.Microsoft.VisualStudio.VC.ml");
    }
    else
    {
        ts["native"]["program"]["c"] = compiler.toString();
        ts["native"]["program"]["cpp"] = compiler.toString();
        if (compiler.ppath == "com.Microsoft.VisualStudio.VC.cl")
            ts["native"]["program"]["asm"] = set_with_version("com.Microsoft.VisualStudio.VC.ml");
    }
    return ts;
}

static String configurationTypeFromStringCaseI(const String &in)
{
    auto configuration = boost::to_lower_copy(in);
    if (configuration == "d")
        return "debug";
    else if (configuration == "r")
        return "release";
    else if (
        configuration == "minsizerel" ||
        configuration == "msr")
        return "minimalsizerelease";
    else if (configuration == "relwithdebinfo" ||
        configuration == "rwdi" ||
        configuration == "releasewithdebinfo")
        return "releasewithdebuginformation";
    return configuration;
}

static String OSTypeFromStringCaseI(const String &in)
{
    auto target_os = boost::to_lower_copy(in);
    if (target_os == "win" || target_os == "windows")
        return "com.Microsoft.Windows.NT";
    return target_os;
}

static String archTypeFromStringCaseI(const String &in)
{
    auto platform = boost::to_lower_copy(in);
    if (platform == "win32" ||
        platform == "x86")
        return "x86";
    else if (
        platform == "win64" ||
        platform == "x64" ||
        platform == "x64_86")
        return "x86_64";
    else if (platform == "arm32")
        return "arm";
    else if (platform == "arm64")
        return "aarch64";
    return platform;
}

static String osTypeFromStringCaseI(const String &in)
{
    auto os = boost::to_lower_copy(in);
    if (os == "win" || os == "windows")
        return "com.Microsoft.Windows.NT";
    else if (os == "linux")
        return "org.torvalds.linux";
    else if (os == "mac" || os == "macos")
        return "com.Apple.Macos"; // XNU? Darwin?
    return os;
}

static void applySettings(sw::TargetSettings &s, const String &in_settings)
{
    Strings pairs;
    boost::split(pairs, in_settings, boost::is_any_of(","));
    for (auto &p : pairs)
    {
        auto pos = p.find("=");
        if (pos == p.npos)
        {
            Strings key_parts;
            boost::split(key_parts, p, boost::is_any_of("."));
            auto *ts = &s;
            for (int i = 0; i < key_parts.size() - 1; i++)
                ts = &((*ts)[key_parts[i]].getSettings());
            (*ts)[key_parts[key_parts.size() - 1]].reset();
            continue;
        }

        auto key = p.substr(0, pos);
        auto value = p.substr(pos + 1);

        Strings key_parts;
        boost::split(key_parts, key, boost::is_any_of("."));
        auto *ts = &s;
        for (int i = 0; i < key_parts.size() - 1; i++)
            ts = &((*ts)[key_parts[i]].getSettings());
        (*ts)[key_parts[key_parts.size() - 1]] = value;
    }
}

static void applySettingsFromJson(sw::TargetSettings &s, const String &jsonstr)
{
    s.merge(jsonstr);
}

static void applySettingsFromFile(sw::TargetSettings &s, const path &fn)
{
    applySettingsFromJson(s, read_file(fn));
}

sw::TargetSettings createSettings(const sw::SwContext &swctx)
{
    auto s = swctx.getHostSettings();
    return s;
}

std::vector<sw::TargetSettings> createSettings(const sw::SwBuild &b)
{
    auto initial_settings = createSettings(b.getContext());
    if (!host_settings_file.empty())
    {
        auto s = b.getContext().getHostSettings();
        applySettingsFromFile(s, host_settings_file);
        ((sw::SwContext &)b.getContext()).setHostSettings(s);
    }

    if (static_deps)
        initial_settings["static-deps"] = "true";

    std::vector<sw::TargetSettings> settings;
    settings.push_back(initial_settings);

    auto times = [&settings](int n)
    {
        if (n <= 1)
            return;
        auto s2 = settings;
        for (int i = 1; i < n; i++)
        {
            for (auto &s : s2)
                settings.push_back(s);
        }
    };

    auto mult_and_action = [&settings, &times](int n, auto f)
    {
        times(n);
        for (int i = 0; i < n; i++)
        {
            int mult = settings.size() / n;
            for (int j = i * mult; j < (i + 1) * mult; j++)
                f(*(settings.begin() + j), i);
        }
    };

    // configuration
    Strings configs;
    for (auto &c : configuration)
    {
        /*if (used_configs.find(c) == used_configs.end())
        {
            if (isConfigSelected(c))
                LOG_WARN(logger, "config was not used: " + c);
        }
        if (!isConfigSelected(c))*/
            configs.push_back(c);
    }
    mult_and_action(configs.size(), [&configs](auto &s, int i)
    {
        s["native"]["configuration"] = configurationTypeFromStringCaseI(configs[i]);
    });

    // static/shared
    if (static_build && shared_build)
    {
        mult_and_action(2, [](auto &s, int i)
        {
            if (i == 0)
                s["native"]["library"] = "static";
            if (i == 1)
                s["native"]["library"] = "shared";
        });
    }
    else
    {
        for (auto &s : settings)
        {
            if (static_build)
                s["native"]["library"] = "static";
            if (shared_build)
                s["native"]["library"] = "shared";
        }
    }

    // mt/md
    if (win_mt && win_md)
    {
        mult_and_action(2, [&settings](auto &s, int i)
        {
            if (i == 0)
                s["native"]["mt"] = "true";
        });
    }
    else
    {
        for (auto &s : settings)
        {
            if (win_mt)
                s["native"]["mt"] = "true";
        }
    }

    // platform
    mult_and_action(platform.size(), [](auto &s, int i)
    {
        s["os"]["arch"] = archTypeFromStringCaseI(platform[i]);
    });

    // os
    mult_and_action(os.size(), [](auto &s, int i)
    {
        s["os"]["kernel"] = osTypeFromStringCaseI(os[i]);
    });

    // libc
    mult_and_action(libc.size(), [](auto &s, int i)
    {
        s["native"]["stdlib"]["c"] = archTypeFromStringCaseI(libc[i]);
    });

    // libcpp
    mult_and_action(libcpp.size(), [](auto &s, int i)
    {
        s["native"]["stdlib"]["cpp"] = archTypeFromStringCaseI(libcpp[i]);
    });

    // compiler
    mult_and_action(compiler.size(), [](auto &s, int i)
    {
        s.merge(compilerTypeFromStringCaseI(compiler[i]));
    });

    // target_os
    mult_and_action(target_os.size(), [](auto &s, int i)
    {
        s["os"]["kernel"] = OSTypeFromStringCaseI(target_os[i]);
    });

    // settings
    mult_and_action(::settings.size(), [](auto &s, int i)
    {
        applySettings(s, ::settings[i]);
    });

    // settings-file
    mult_and_action(settings_file.size(), [](auto &s, int i)
    {
        applySettingsFromFile(s, settings_file[i]);
    });

    // settings-json
    mult_and_action(settings_json.size(), [](auto &s, int i)
    {
        applySettingsFromJson(s, settings_json[i]);
    });

    // also we support inline host settings
    if (settings.size() == 1 && settings[0]["host"])
    {
        auto s = b.getContext().getHostSettings();
        s.merge(settings[0]["host"].getSettings());
        ((sw::SwContext &)b.getContext()).setHostSettings(s);
        settings[0]["host"].reset();
    }

    return settings;
}

std::unique_ptr<sw::SwBuild> setBuildArgsAndCreateBuildAndPrepare(sw::SwContext &swctx, const Strings &build_args)
{
    ((Strings&)build_arg) = build_args;
    return createBuildAndPrepare(swctx);
}

std::unique_ptr<sw::SwBuild> createBuildAndPrepare(sw::SwContext &swctx)
{
    auto b = swctx.createBuild();
    for (auto &a : build_arg)
    {
        sw::InputWithSettings i(swctx.addInput(a));
        for (auto &s : createSettings(*b))
            i.addSettings(s);
        b->addInput(i);
    }
    b->load();
    b->setTargetsToBuild();
    b->resolvePackages();
    b->loadPackages();
    b->prepare();
    return std::move(b);
}

SUBCOMMAND_DECL2(build)
{
    if (!build_explan.empty())
    {
        auto b = swctx.createBuild();
        b->overrideBuildState(sw::BuildState::Prepared);
        auto p = sw::ExecutionPlan::load(build_explan, swctx);
        b->execute(p);
        return;
    }

    if (build_fetch)
    {
        build_after_fetch = true;
        return cli_fetch(swctx);
    }

    // defaults or only one of build_arg and -S specified
    //  -S == build_arg
    //  -B == fs::current_path()

    // if -S and build_arg specified:
    //  source dir is taken as -S, config dir is taken as build_arg

    // if -B specified, it is used as is

    auto b = swctx.createBuild();
    for (auto &a : build_arg)
    {
        sw::InputWithSettings i(swctx.addInput(a));
        for (auto &s : createSettings(*b))
            i.addSettings(s);
        b->addInput(i);
    }
    if (build_default_explan)
    {
        b->load();
        swctx.clearFileStorages();
        b->runSavedExecutionPlan();
        return;
    }
    b->build();
}
