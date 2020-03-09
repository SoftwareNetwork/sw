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
#include <sw/driver/driver.h>

#include <boost/algorithm/string.hpp>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "build");

SUBCOMMAND_DECL(build)
{
    auto swctx = createSwContext(options);
    cli_build(*swctx, options);
}

static sw::TargetSettings compilerTypeFromStringCaseI(const sw::UnresolvedPackage &compiler)
{
    sw::TargetSettings ts;

    auto with_version = [&compiler](const sw::PackagePath &ppath)
    {
        return sw::UnresolvedPackage(ppath, compiler.range);
    };

    auto set_with_version = [&with_version](const sw::PackagePath &ppath)
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
        ts["native"]["program"]["asm"] = ts["native"]["program"]["c"].getValue();
    }
    else if (compiler.ppath == "clang")
    {
        ts["native"]["program"]["c"] = set_with_version("org.LLVM.clang");
        ts["native"]["program"]["cpp"] = set_with_version("org.LLVM.clangpp");
        ts["native"]["program"]["asm"] = ts["native"]["program"]["c"].getValue();
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
    else if (os == "cyg" || os == "cygwin")
        return "org.cygwin";
    else if (os == "mingw" || os == "mingw32" || os == "mingw64" || os == "msys")
        return "org.mingw";
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
    s.mergeFromString(jsonstr);
}

static std::vector<sw::TargetSettings> applySettingsFromCppFile(sw::SwContext &swctx, const Options &options, const path &fn)
{
    auto b = createBuild(swctx, options);
    auto inputs = swctx.addInput(fn);
    SW_CHECK(inputs.size() == 1);
    sw::InputWithSettings i(*inputs[0]);
    auto ts = createInitialSettings(swctx);
#ifdef NDEBUG
    ts["native"]["configuration"] = "releasewithdebuginformation";
#endif
    i.addSettings(ts);
    b->addInput(i);
    b->build();

    // load module
    sw::TargetContainer *tc = nullptr;
    for (auto &[pkg, tgts] : b->getTargetsToBuild())
    {
        if (pkg.getPath().isRelative())
            tc = &tgts;
    }
    if (!tc)
        throw SW_RUNTIME_ERROR("No relative targets found");
    if (tc->empty())
        throw SW_RUNTIME_ERROR("Empty cfg target");
    auto &t = **tc->begin();
    auto is = t.getInterfaceSettings();
    auto m = swctx.getModuleStorage().get(is["output_file"].getValue());
    if (m.symbol_storage().get_function<std::map<std::string, std::string>()>("createJsonSettings").empty())
        throw SW_RUNTIME_ERROR("Cannot find 'std::map<std::string, std::string> createJsonSettings()'");

    auto selected_cfgs = std::set<String>(options.settings_file_config.begin(), options.settings_file_config.end());
    auto result = m.get_function<std::map<std::string, std::string>()>("createJsonSettings")();
    std::vector<sw::TargetSettings> r;
    for (auto &[k, v] : result)
    {
        if (v.empty())
            throw SW_RUNTIME_ERROR("Empty settings");
        if (selected_cfgs.empty() || selected_cfgs.find(k) != selected_cfgs.end())
        {
            sw::TargetSettings ts;
            ts.mergeFromString(v);
            r.push_back(ts);
        }
    }
    return r;
}

static std::vector<sw::TargetSettings> getSettingsFromFile(sw::SwContext &swctx, const Options &options)
{
    std::vector<sw::TargetSettings> ts;
    for (auto &fn : options.settings_file)
    {
        if (fn.extension() == ".json")
        {
            sw::TargetSettings s;
            applySettingsFromJson(s, read_file(fn));
            ts.push_back(s);
        }
        else if (fn.extension() == ".cpp")
        {
            auto ts1 = applySettingsFromCppFile(swctx, options, fn);
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

std::vector<sw::TargetSettings> createSettings(sw::SwContext &swctx, const Options &options)
{
    auto initial_settings = createInitialSettings(swctx);
    if (!options.host_settings_file.empty())
    {
        auto s = swctx.getHostSettings();
        applySettingsFromJson(s, read_file(options.host_settings_file));
        swctx.setHostSettings(s);
        if (s["host"])
            LOG_WARN(logger, "'host' key present in host settings. Probably misuse. Remove it and put everything under root.");
    }

    if (options.static_dependencies)
        initial_settings["static-deps"] = "true";
    if (options.reproducible_build)
        initial_settings["reproducible-build"] = "true";

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
    for (auto &c : options.configuration)
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
    if (options.static_build && options.shared_build)
    {
        // preserve order
        int st = 0, sh = 1;
        if (cl_static_build.getPosition() > cl_shared_build.getPosition())
            st = 1, sh = 0;
        mult_and_action(2, [st,sh](auto &s, int i)
        {
            if (i == st)
                s["native"]["library"] = "static";
            if (i == sh)
                s["native"]["library"] = "shared";
        });
    }
    else
    {
        for (auto &s : settings)
        {
            if (options.static_build)
                s["native"]["library"] = "static";
            if (options.shared_build)
                s["native"]["library"] = "shared";
        }
    }

    // mt/md
    if (options.win_mt && options.win_md)
    {
        // preserve order
        int mt = 0, md = 1;
        if (cl_win_mt.getPosition() > cl_win_md.getPosition())
            mt = 1, md = 0;
        mult_and_action(2, [mt, md](auto &s, int i)
        {
            if (i == mt)
                s["native"]["mt"] = "true";
            if (i == md)
                s["native"]["mt"] = "false";
        });
    }
    else
    {
        for (auto &s : settings)
        {
            if (options.win_mt)
                s["native"]["mt"] = "true";
            if (options.win_md)
                s["native"]["mt"] = "false";
        }
    }

    // platform
    mult_and_action(options.platform.size(), [&options](auto &s, int i)
    {
        s["os"]["arch"] = archTypeFromStringCaseI(options.platform[i]);
    });

    // target_os
    mult_and_action(options.os.size(), [&options](auto &s, int i)
    {
        s["os"]["kernel"] = osTypeFromStringCaseI(options.os[i]);
    });

    // libc
    mult_and_action(options.libc.size(), [&options](auto &s, int i)
    {
        s["native"]["stdlib"]["c"] = archTypeFromStringCaseI(options.libc[i]);
    });

    // libcpp
    mult_and_action(options.libcpp.size(), [&options](auto &s, int i)
    {
        s["native"]["stdlib"]["cpp"] = archTypeFromStringCaseI(options.libcpp[i]);
    });

    // compiler
    mult_and_action(options.compiler.size(), [&options](auto &s, int i)
    {
        s.mergeAndAssign(compilerTypeFromStringCaseI(options.compiler[i]));
    });

    // settings
    mult_and_action(options.settings.size(), [&options](auto &s, int i)
    {
        applySettings(s, options.settings[i]);
    });

    // settings-file
    auto sf = getSettingsFromFile(swctx, options);
    mult_and_action(sf.size(), [&sf](auto &s, int i)
    {
        s.mergeAndAssign(sf[i]);
    });

    // settings-json
    mult_and_action(options.settings_json.size(), [&options](auto &s, int i)
    {
        applySettingsFromJson(s, options.settings_json[i]);
    });

    // also we support inline host settings
    if (settings.size() == 1 && settings[0]["host"])
    {
        auto s = swctx.getHostSettings();
        s.mergeAndAssign(settings[0]["host"].getSettings());
        swctx.setHostSettings(s);
        settings[0]["host"].reset();
    }

    if (!options.options_build.output_dir.empty())
    {
        if (settings.size() != 1)
            throw SW_RUNTIME_ERROR("Cannot set output-dir, multiple configurations requested");
        auto d = fs::absolute(options.options_build.output_dir);
        fs::create_directories(d);
        d = fs::canonical(d);
        for (auto &s : settings)
        {
            s["output_dir"] = normalize_path(d);
            s["output_dir"].useInHash(false);
            s["output_dir"].ignoreInComparison(true);
        }
    }

    if (!options.config_name.empty())
    {
        if (options.config_name.size() != settings.size())
        {
            throw SW_RUNTIME_ERROR("Number of config names (" + std::to_string(options.config_name.size()) +
                ") must be equal to number of configs (" + std::to_string(settings.size()) + ")");
        }
        for (const auto &[i,s] : enumerate(settings))
        {
            if (s["name"])
                throw SW_RUNTIME_ERROR("Some config already has its name");
            s["name"] = options.config_name[i];
            s["name"].useInHash(false);
            s["name"].ignoreInComparison(true);
        }
        LOG_DEBUG(logger, "WARNING: Setting config names may result in wrong config-name pair assignment, "
            "because of unspecified config creation order.");
    }

    return settings;
}

static void addInputs(sw::SwBuild &b, const Inputs &i, const Options &options)
{
    for (auto &[ts,in] : i.getInputPairs())
    {
        for (auto i : b.getContext().addInput(in))
        {
            sw::InputWithSettings p(*i);
            p.addSettings(ts);
            b.addInput(p);
        }
    }

    for (auto &a : i.getInputs())
    {
        for (auto i : b.getContext().addInput(a))
        {
            sw::InputWithSettings ii(*i);
            for (auto &s : createSettings(b.getContext(), options))
                ii.addSettings(s);
            b.addInput(ii);
        }
    }
}

std::unique_ptr<sw::SwBuild> createBuildAndPrepare(sw::SwContext &swctx, const Inputs &i, const Options &options)
{
    auto b = createBuild(swctx, options);
    addInputs(*b, i, options);
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

static void isolated_build(sw::SwContext &swctx, const Options &options)
{
    // get targets
    // create dirs

    LOG_INFO(logger, "Determining targets");

    auto b1 = createBuild(swctx, options);
    auto &b = *b1;

    auto ts = createInitialSettings(swctx);
    for (auto &ii : getInput(b))
    {
        sw::InputWithSettings i(*ii);
        i.addSettings(ts);
        b.addInput(i);
    }
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
        auto b1 = createBuild(swctx, options);
        auto &b = *b1;

        for (auto &ii : getInput(b))
        {
            sw::InputWithSettings i(*ii);
            i.addSettings(ts);
            b.addInput(i);
        }
        b.build();
    }
}

std::unique_ptr<sw::SwBuild> createBuild(sw::SwContext &swctx, const Options &options)
{
    auto b = swctx.createBuild();

    b->setName(options.build_name);

    sw::TargetSettings bs;

    // this is coming from the outside to distinguish from
    // internal builds (checks, scripts builds)
    bs["master_build"] = "true";

    std::optional<bool> use_lock;
    if (cl_use_lock_file.getNumOccurrences()) // always respect when specified
        use_lock = options.use_lock_file;
    if (!use_lock) // try heuristics
    {
        use_lock = fs::exists(fs::current_path() / "sw.lock");
        if (*use_lock)
            LOG_INFO(logger, "Lock file is found, using it: " << fs::current_path() / "sw.lock");
    }
    if (*use_lock)
    {
        // save lock file near input? what if we have multiple inputs?
        if (options.lock_file.empty())
            bs["lock_file"] = normalize_path(fs::current_path() / "sw.lock");
        else
            bs["lock_file"] = normalize_path(options.lock_file);
    }

    //
    if (options.build_always)
        bs["build_always"] = "true";
    if (options.use_saved_configs)
        bs["use_saved_configs"] = "true";
    if (!options.options_build.ide_copy_to_dir.empty())
        bs["build_ide_copy_to_dir"] = normalize_path(options.options_build.ide_copy_to_dir);
    if (!options.options_build.ide_fast_path.empty())
        bs["build_ide_fast_path"] = normalize_path(options.options_build.ide_fast_path);
    if (options.skip_errors)
        bs["skip_errors"] = std::to_string(options.skip_errors);
    if (options.time_trace)
        bs["time_trace"] = "true";
    if (cl_show_output)
        bs["show_output"] = "true";
    if (cl_write_output_to_file)
        bs["write_output_to_file"] = "true";
    if (!options.options_build.time_limit.empty())
        bs["time_limit"] = options.options_build.time_limit;
    if (gVerbose || options.trace)
        bs["measure"] = "true";
    for (auto &t : options.targets_to_build)
        bs["target-to-build"].push_back(t);
    for (auto &t : options.targets_to_ignore)
        bs["target-to-exclude"].push_back(t);
    bs["build-jobs"] = std::to_string(select_number_of_threads(options.build_jobs));
    for (auto &t : options.Dvariables)
    {
        auto p = t.find('=');
        bs["D"][t.substr(0, p)] = t.substr(p + 1);
    }
    b->setSettings(bs);

    return b;
}

std::unique_ptr<sw::SwBuild> createBuild(sw::SwContext &swctx, const Inputs &i, const Options &options)
{
    auto b = createBuild(swctx, options);
    addInputs(*b, i, options);
    return b;
}

SUBCOMMAND_DECL2(build)
{
    if (!options.options_build.build_explan.empty())
    {
        auto b = createBuild(swctx, options);
        b->overrideBuildState(sw::BuildState::Prepared);
        auto [cmds, p] = sw::ExecutionPlan::load(options.options_build.build_explan, swctx);
        b->execute(p);
        return;
    }

    if (options.options_build.build_fetch)
    {
        options.options_fetch.build_after_fetch = true;
        return cli_fetch(swctx, options);
    }

    if (cl_isolated_build)
    {
        isolated_build(swctx, options);
        return;
    }

    // defaults or only one of build_arg and -S specified
    //  -S == build_arg
    //  -B == fs::current_path()

    // if -S and build_arg specified:
    //  source dir is taken as -S, config dir is taken as build_arg

    // if -B specified, it is used as is

    Inputs inputs(options.options_build.build_inputs);
    const auto &pairs = options.options_build.input_settings_pairs;
    if (pairs.size() % 2 == 1)
        throw SW_RUNTIME_ERROR("Incorrect input settings pairs. Something is missing. Size must be even, but size = " + std::to_string(pairs.size()));
    for (int i = 0; i < pairs.size(); i += 2)
    {
        sw::TargetSettings s;
        s.mergeFromString(pairs[i + 1]);
        inputs.addInputPair(s, pairs[i]);
    }

    auto b = createBuild(swctx, inputs, options);
    if (options.options_build.build_default_explan)
    {
        b->loadInputs();
        swctx.clearFileStorages();
        b->runSavedExecutionPlan();
        return;
    }
    b->build();
}
