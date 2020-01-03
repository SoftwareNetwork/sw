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

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "build");

DEFINE_SUBCOMMAND(build, "Build files, dirs or packages.");
DEFINE_SUBCOMMAND_ALIAS(build, b)

extern ::cl::opt<bool> build_after_fetch;

static ::cl::list<String> build_arg(::cl::Positional, ::cl::desc("Files or directories to build (paths to config)"), ::cl::sub(subcommand_build));

//static ::cl::opt<String> build_source_dir("S", ::cl::desc("Explicitly specify a source directory."), ::cl::sub(subcommand_build), ::cl::init("."));
//static ::cl::opt<String> build_binary_dir("B", ::cl::desc("Explicitly specify a build directory."), ::cl::sub(subcommand_build), ::cl::init(SW_BINARY_DIR));

static ::cl::opt<bool> build_fetch("fetch", ::cl::desc("Fetch sources, then build"), ::cl::sub(subcommand_build));
static ::cl::opt<path> build_explan("ef", ::cl::desc("Build execution plan from specified file"), ::cl::sub(subcommand_build));
static ::cl::opt<bool> build_default_explan("e", ::cl::desc("Build execution plan"), ::cl::sub(subcommand_build));

static ::cl::opt<bool> isolated_build("isolated", cl::desc("Copy source files to isolated folders to check build like just after uploading"), ::cl::sub(subcommand_build));

::cl::opt<path> build_ide_fast_path("ide-fast-path", ::cl::sub(subcommand_build), ::cl::Hidden);
static ::cl::opt<path> build_ide_copy_to_dir("ide-copy-to-dir", ::cl::sub(subcommand_build), ::cl::Hidden);

static ::cl::opt<String> time_limit("time-limit", ::cl::sub(subcommand_build));

//

//cl::opt<bool> dry_run("n", cl::desc("Dry run"));

static cl::opt<bool> build_always("B", cl::desc("Build always"));
static cl::opt<int> skip_errors("k", cl::desc("Skip errors"));
static cl::opt<bool> time_trace("time-trace", cl::desc("Record chrome time trace events"));

static cl::opt<bool> cl_show_output("show-output");
static cl::opt<bool> cl_write_output_to_file("write-output-to-file");
//static cl::opt<bool> print_graph("print-graph", cl::desc("Print file with build graph"));

Strings targets_to_build;
static ::cl::list<String, Strings> cl_targets_to_build("target", ::cl::desc("Targets to build"), ::cl::location(targets_to_build));
static ::cl::list<String> targets_to_ignore("exclude-target", ::cl::desc("Targets to ignore"));

static ::cl::list<String> Dvariables("D", ::cl::desc("Input variables"), ::cl::ZeroOrMore, ::cl::Prefix);

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
// toolchain file
static cl::list<path> settings_file("settings-file", cl::desc("Read settings from file"), cl::ZeroOrMore);
static cl::list<String> settings_file_config("settings-file-config", cl::desc("Select settings from file"), cl::ZeroOrMore);
static cl::list<String> settings_json("settings-json", cl::desc("Read settings from json string"), cl::ZeroOrMore);
static cl::opt<path> host_settings_file("host-settings-file", cl::desc("Read host settings from file"));

static cl::list<String> input_settings_pairs("input-settings-pairs", cl::value_desc("<input settings>"), cl::desc("Read settings from json string"), ::cl::sub(subcommand_build), ::cl::SpaceSeparated);

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
    if (build_arg.empty() && input_settings_pairs.empty())
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
        ts["native"]["program"]["lib"] = set_with_version("com.Microsoft.VisualStudio.VC.lib");
        ts["native"]["program"]["link"] = set_with_version("com.Microsoft.VisualStudio.VC.link");
        ts["native"]["stdlib"]["cpp"] = set_with_version("com.Microsoft.VisualStudio.VC.libcpp");
    }
    else if (compiler.ppath == "intel")
    {
        ts["native"]["program"]["c"] = set_with_version("com.intel.compiler.c");
        ts["native"]["program"]["cpp"] = set_with_version("com.intel.compiler.cpp");
        ts["native"]["program"]["asm"] = set_with_version("com.Microsoft.VisualStudio.VC.ml");
        ts["native"]["program"]["lib"] = sw::UnresolvedPackage("com.intel.compiler.lib").toString();
        ts["native"]["program"]["link"] = sw::UnresolvedPackage("com.intel.compiler.link").toString();
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

static std::vector<sw::TargetSettings> applySettingsFromCppFile(sw::SwContext &swctx, const path &fn)
{
    auto b = createBuild(swctx);
    sw::Input i1(fn, sw::InputType::InlineSpecification, swctx);
    sw::InputWithSettings i(i1);
    auto ts = createInitialSettings(swctx);
#ifdef NDEBUG
    ts["native"]["configuration"] = "releasewithdebuginformation";
#endif
    i.addSettings(ts);
    b->addInput(i);
    b->build();

    // load module
    auto tgts = b->getTargetsToBuild();
    if (tgts.size() != 1)
        throw SW_RUNTIME_ERROR("Must be exactly one target");
    auto &tgts2 = tgts.begin()->second;
    if (tgts2.empty())
        throw SW_RUNTIME_ERROR("Empty cfg target");
    auto &t = **tgts2.begin();
    auto is = t.getInterfaceSettings();
    auto m = swctx.getModuleStorage().get(is["output_file"].getValue());
    if (m.symbol_storage().get_function<std::map<std::string, std::string>()>("createJsonSettings").empty())
        throw SW_RUNTIME_ERROR("Cannot find 'std::map<std::string, std::string> createJsonSettings()'");

    auto selected_cfgs = std::set<String>(settings_file_config.begin(), settings_file_config.end());
    auto result = m.get_function<std::map<std::string, std::string>()>("createJsonSettings")();
    std::vector<sw::TargetSettings> r;
    for (auto &[k, v] : result)
    {
        if (v.empty())
            throw SW_RUNTIME_ERROR("Empty settings");
        if (selected_cfgs.empty() || selected_cfgs.find(k) != selected_cfgs.end())
        {
            sw::TargetSettings ts;
            ts.merge(v);
            r.push_back(ts);
        }
    }
    return r;
}

std::vector<sw::TargetSettings> getSettingsFromFile(sw::SwContext &swctx)
{
    std::vector<sw::TargetSettings> ts;
    for (auto &fn : settings_file)
    {
        if (fn.extension() == ".json")
        {
            sw::TargetSettings s;
            applySettingsFromJson(s, read_file(fn));
            ts.push_back(s);
        }
        else if (fn.extension() == ".cpp")
        {
            auto ts1 = applySettingsFromCppFile(swctx, fn);
            ts.insert(ts.end(), ts1.begin(), ts1.end());
        }
        else
            throw SW_RUNTIME_ERROR("Unknown settings file: " + normalize_path(fn));
    }
    return ts;
}

sw::TargetSettings createInitialSettings(const sw::SwContext &swctx)
{
    auto s = swctx.getHostSettings();
    return s;
}

std::vector<sw::TargetSettings> createSettings(sw::SwContext &swctx)
{
    auto initial_settings = createInitialSettings(swctx);
    if (!host_settings_file.empty())
    {
        auto s = swctx.getHostSettings();
        applySettingsFromJson(s, read_file(host_settings_file));
        swctx.setHostSettings(s);
        if (s["host"])
            LOG_WARN(logger, "'host' key present in host settings. Probably misuse. Remove it and put everything under root.");
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
    auto sf = getSettingsFromFile(swctx);
    mult_and_action(sf.size(), [&sf](auto &s, int i)
    {
        s.merge(sf[i]);
    });

    // settings-json
    mult_and_action(settings_json.size(), [](auto &s, int i)
    {
        applySettingsFromJson(s, settings_json[i]);
    });

    // also we support inline host settings
    if (settings.size() == 1 && settings[0]["host"])
    {
        auto s = swctx.getHostSettings();
        s.merge(settings[0]["host"].getSettings());
        swctx.setHostSettings(s);
        settings[0]["host"].reset();
    }

    return settings;
}

void createInputs(sw::SwBuild &b)
{
    auto &pairs = (Strings &)input_settings_pairs;
    if (!pairs.empty())
    {
        if (pairs.size() % 2 == 1)
            throw SW_RUNTIME_ERROR("Incorrect input settings pairs. Something is missing. Size must be even, but size = " + std::to_string(pairs.size()));
        for (int i = 0; i < pairs.size(); i += 2)
        {
            sw::InputWithSettings p(b.getContext().addInput(pairs[i]));
            sw::TargetSettings s;
            s.merge(pairs[i + 1]);
            p.addSettings(s);
            b.addInput(p);
        }
    }

    for (auto &a : build_arg)
    {
        sw::InputWithSettings i(b.getContext().addInput(a));
        for (auto &s : createSettings(b.getContext()))
            i.addSettings(s);
        b.addInput(i);
    }
}

std::unique_ptr<sw::SwBuild> setBuildArgsAndCreateBuildAndPrepare(sw::SwContext &swctx, const Strings &build_args)
{
    ((Strings&)build_arg) = build_args;
    return createBuildAndPrepare(swctx);
}

std::unique_ptr<sw::SwBuild> createBuildAndPrepare(sw::SwContext &swctx)
{
    auto b = createBuild(swctx);
    createInputs(*b);
    b->loadInputs();
    b->setTargetsToBuild();
    b->resolvePackages();
    b->loadPackages();
    b->prepare();
    return std::move(b);
}

static decltype(auto) getInput(sw::SwBuild &b)
{
    return b.getContext().addInput(fs::current_path());
}

static void isolated_build1(sw::SwContext &swctx)
{
    // get targets
    // create dirs

    LOG_INFO(logger, "Determining targets");

    auto b1 = createBuild(swctx);
    auto &b = *b1;

    auto ts = createInitialSettings(swctx);
    auto &ii = getInput(b);
    sw::InputWithSettings i(ii);
    i.addSettings(ts);
    b.addInput(i);
    b.loadInputs();
    b.setTargetsToBuild();
    b.resolvePackages();
    b.loadPackages();
    b.prepare();

    // get sources to pass them into getPackages()
    sw::SourceDirMap srcs;
    for (const auto &[pkg, tgts] : b.getTargetsToBuild())
    {
        if (tgts.empty())
            throw SW_RUNTIME_ERROR("Empty targets");

        auto &t = **tgts.begin();
        auto s = t.getSource().clone(); // make a copy!
        s->applyVersion(pkg.getVersion());
        if (srcs.find(s->getHash()) != srcs.end())
            continue;
        srcs[s->getHash()].requested_dir = fs::current_path();
    }

    LOG_INFO(logger, "Copying files");

    auto m = getPackages(b, srcs);
    auto d = b.getBuildDirectory() / "isolated";

    for (const auto &[pkg, tgts] : b.getTargetsToBuild())
    {
        if (tgts.empty())
            throw SW_RUNTIME_ERROR("Empty targets");

        auto dir = d / pkg.toString();
        for (auto &[from, to] : m[pkg]->getData().files_map)
        {
            fs::create_directories((dir / to).parent_path());
            fs::copy_file(from, dir / to, fs::copy_options::update_existing);
        }

        ts["driver"]["source-dir-for-package"][pkg.toString()] = normalize_path(dir);
    }

    LOG_INFO(logger, "Building in isolated environment");

    //
    {
        auto b1 = createBuild(swctx);
        auto &b = *b1;

        auto &ii = getInput(b);
        sw::InputWithSettings i(ii);
        i.addSettings(ts);
        b.addInput(i);
        b.build();
    }
}

std::unique_ptr<sw::SwBuild> createBuild(sw::SwContext &swctx)
{
    auto b = swctx.createBuild();

    sw::TargetSettings bs;
    if (build_always)
        bs["build_always"] = "true";
    if (!build_ide_copy_to_dir.empty())
        bs["build_ide_copy_to_dir"] = normalize_path(build_ide_copy_to_dir);
    if (!build_ide_fast_path.empty())
        bs["build_ide_fast_path"] = normalize_path(build_ide_fast_path);
    if (skip_errors)
        bs["skip_errors"] = std::to_string(skip_errors);
    if (time_trace)
        bs["time_trace"] = "true";
    if (cl_show_output)
        bs["show_output"] = "true";
    if (cl_write_output_to_file)
        bs["write_output_to_file"] = "true";
    if (!time_limit.empty())
        bs["time_limit"] = time_limit;
    for (auto &t : targets_to_build)
        bs["target-to-build"].push_back(t);
    for (auto &t : targets_to_ignore)
        bs["target-to-exclude"].push_back(t);
    for (auto &t : Dvariables)
    {
        auto p = t.find('=');
        bs["D"][t.substr(0, p)] = t.substr(p + 1);
    }
    b->setSettings(bs);

    return b;
}

SUBCOMMAND_DECL2(build)
{
    if (!build_explan.empty())
    {
        auto b = createBuild(swctx);
        b->overrideBuildState(sw::BuildState::Prepared);
        auto [cmds, p] = sw::ExecutionPlan::load(build_explan, swctx);
        b->execute(p);
        return;
    }

    if (build_fetch)
    {
        build_after_fetch = true;
        return cli_fetch(swctx);
    }

    if (isolated_build)
    {
        isolated_build1(swctx);
        return;
    }

    // defaults or only one of build_arg and -S specified
    //  -S == build_arg
    //  -B == fs::current_path()

    // if -S and build_arg specified:
    //  source dir is taken as -S, config dir is taken as build_arg

    // if -B specified, it is used as is

    auto b = createBuild(swctx);
    createInputs(*b);
    if (build_default_explan)
    {
        b->loadInputs();
        swctx.clearFileStorages();
        b->runSavedExecutionPlan();
        return;
    }
    b->build();
}
