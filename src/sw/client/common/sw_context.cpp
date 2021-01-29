// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#include "sw_context.h"

#include <cl.llvm.h>

#include <boost/algorithm/string.hpp>
#include <boost/dll.hpp>
#include <boost/dll/import_mangled.hpp>
#include <boost/dll/smart_library.hpp>
#include <primitives/emitter.h>
#include <primitives/executor.h>
#include <primitives/http.h>
#include <sw/core/build.h>
#include <sw/core/input.h>
#include <sw/core/sw_context.h>
#include <sw/driver/driver.h> // register driver
#include <sw/driver/compiler/detect.h>
#include <sw/manager/settings.h>
#include <sw/support/filesystem.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "client.context");

static void setHttpTlsSettings()
{
    // 1. old systems may not have our letsencrypt certs
    // 2. grpc require explicit certs file
    //primitives::http::setupSafeTls();
    primitives::http::setupSafeTls(false, false, sw::support::get_ca_certs_filename());
    //httpSettings.ca_certs_file = sw::support::get_ca_certs_filename();
}

void setHttpSettings(const Options &options)
{
    httpSettings.verbose = options.curl_verbose;
    httpSettings.ignore_ssl_checks = options.ignore_ssl_checks;
    httpSettings.proxy = sw::Settings::get_local_settings().proxy;

    static std::once_flag f;
    std::call_once(f, setHttpTlsSettings);
}

static void applySettingsFromJson(sw::PackageSettings &s, const String &jsonstr)
{
    s.mergeFromString(jsonstr);
}

static sw::PackageSettings compilerTypeFromStringCaseI(const sw::UnresolvedPackageName &compiler)
{
    sw::PackageSettings ts;

    auto with_version = [&compiler](const sw::PackagePath &ppath)
    {
        return sw::UnresolvedPackageName(ppath, compiler.getRange());
    };

    auto set_with_version = [&with_version](const sw::PackagePath &ppath)
    {
        return with_version(ppath).toString();
    };

    if (0);
    // g++ is not possible for package path
    else if (0
        || compiler.getPath() == "gcc"
        || compiler.getPath() == "gnu"
        || compiler.getPath() == "org.gnu.gcc"
        || compiler.getPath() == "org.gnu.gpp"
        )
    {
        ts["rule"]["c"]["package"] = set_with_version("org.gnu.gcc");
        ts["rule"]["cpp"]["package"] = set_with_version("org.gnu.gpp");
        ts["rule"]["asm"]["package"] = ts["rule"]["c"]["package"].getValue();
        for (auto &[k, v] : ts["rule"].getMap())
            v["type"] = "gnu";
    }
    else if (0
        || compiler.getPath() == "clang"
        || compiler.getPath() == "org.LLVM.clang"
        || compiler.getPath() == "org.LLVM.clangpp"
        )
    {
        ts["rule"]["c"]["package"] = set_with_version("org.LLVM.clang");
        ts["rule"]["cpp"]["package"] = set_with_version("org.LLVM.clangpp");
        ts["rule"]["asm"]["package"] = ts["rule"]["c"]["package"].getValue();
        for (auto &[k, v] : ts["rule"].getMap())
            v["type"] = "clang";
    }
    else if (0
        || compiler.getPath() == "appleclang"
        || compiler.getPath() == "com.Apple.clang"
        || compiler.getPath() == "com.Apple.clangpp"
        )
    {
        ts["rule"]["c"]["package"] = set_with_version("com.Apple.clang");
        ts["rule"]["cpp"]["package"] = set_with_version("com.Apple.clangpp");
        ts["rule"]["asm"]["package"] = ts["rule"]["c"]["package"].getValue();
        for (auto &[k, v] : ts["rule"].getMap())
            v["type"] = "appleclang";
    }
    // clang-cl is not possible for package path
    else if (0
        || compiler.getPath() == "clangcl"
        /* || compiler.ppath == "clang-cl"*/
        || compiler.getPath() == "org.LLVM.clangcl"
        )
    {
        ts["rule"]["c"]["package"] = set_with_version("org.LLVM.clangcl");
        ts["rule"]["cpp"]["package"] = set_with_version("org.LLVM.clangcl");
        //ts["rule"]["link"]["package"] = set_with_version("org.LLVM.lld.link");
        for (auto &[k, v] : ts["rule"].getMap())
            v["type"] = "clangcl";
    }
    else if (0
        || compiler.getPath() == "msvc"
        || compiler.getPath() == "vs"
        || compiler.getPath() == "com.Microsoft.VisualStudio.VC.cl"
        )
    {
        ts["rule"]["c"]["package"] = set_with_version("com.Microsoft.VisualStudio.VC.cl");
        ts["rule"]["cpp"]["package"] = set_with_version("com.Microsoft.VisualStudio.VC.cl");
        ts["rule"]["asm"]["package"] = set_with_version("com.Microsoft.VisualStudio.VC.ml");
        ts["rule"]["lib"]["package"] = set_with_version("com.Microsoft.VisualStudio.VC.lib");
        ts["rule"]["link"]["package"] = set_with_version("com.Microsoft.VisualStudio.VC.link");
        for (auto &[k, v] : ts["rule"].getMap())
            v["type"] = "msvc";
        ts["native"]["stdlib"]["cpp"] = set_with_version("com.Microsoft.VisualStudio.VC.libcpp");
    }
    else if (0
        || compiler.getPath() == "intel"
        || compiler.getPath() == "com.intel.compiler.c"
        || compiler.getPath() == "com.intel.compiler.cpp"
        )
    {
        ts["rule"]["c"]["package"] = set_with_version("com.intel.compiler.c");
        ts["rule"]["cpp"]["package"] = set_with_version("com.intel.compiler.cpp");
        ts["rule"]["asm"]["package"] = set_with_version("com.Microsoft.VisualStudio.VC.ml");
        ts["rule"]["lib"]["package"] = sw::UnresolvedPackageName("com.intel.compiler.lib").toString();
        ts["rule"]["link"]["package"] = sw::UnresolvedPackageName("com.intel.compiler.link").toString();
        for (auto &[k, v] : ts["rule"].getMap())
            v["type"] = "intel";
        ts["rule"]["asm"]["type"] = "msvc";
    }
    else
    {
        ts["rule"]["c"]["package"] = compiler.toString();
        ts["rule"]["cpp"]["package"] = compiler.toString();
    }

    return ts;
}

static sw::PackageSettings linkerTypeFromStringCaseI(const sw::UnresolvedPackageName &linker)
{
    sw::PackageSettings ts;

    //ts["rule"]["lib"] = linker->toString();
    ts["rule"]["link"]["package"] = linker.toString();

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

static std::tuple<String, std::optional<sw::PackageVersion>> osTypeFromStringCaseI(const String &in)
{
    auto os = boost::to_lower_copy(in);
    std::optional<sw::PackageVersion> v;
    auto p = os.find("-");
    if (p != os.npos)
    {
        v = sw::PackageVersion(os.substr(p + 1));
        os = os.substr(0, p);
    }
    if (os == "win" || os == "windows")
        return {"com.Microsoft.Windows.NT", v};
    else if (os == "linux")
        return {"org.torvalds.linux", v};
    else if (os == "mac" || os == "macos")
        return {"com.Apple.Macos", v}; // XNU? Darwin?
    else if (os == "cyg" || os == "cygwin")
        return {"org.cygwin", v};
    else if (os == "mingw" || os == "mingw32" || os == "mingw64" || os == "msys")
        return {"org.mingw", v};
    return {os, v};
}

static void applySettings(sw::PackageSettings &s, const String &in_settings)
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
                ts = &((*ts)[key_parts[i]].getMap());
            (*ts)[key_parts[key_parts.size() - 1]].reset();
            continue;
        }

        auto key = p.substr(0, pos);
        auto value = p.substr(pos + 1);

        Strings key_parts;
        boost::split(key_parts, key, boost::is_any_of("."));
        auto *ts = &s;
        for (int i = 0; i < key_parts.size() - 1; i++)
            ts = &((*ts)[key_parts[i]].getMap());
        (*ts)[key_parts[key_parts.size() - 1]] = value;
    }
}

static std::vector<sw::PackageSettings> applySettingsFromCppFile(SwClientContext &swctx, const Options &options, const path &fn)
{
    SW_UNIMPLEMENTED;
//    auto b = swctx.createBuild();
//    auto inputs = swctx.getContext().makeInput(fn);
//    SW_CHECK(inputs.size() == 1);
//    auto &i = inputs[0];
//    auto ts = swctx.createInitialSettings();
//#ifdef NDEBUG
//    ts["native"]["configuration"] = "releasewithdebuginformation";
//#endif
//    i.addSettings(ts);
//    b->addInput(i);
//    b->build();
//
//    // load module
//    sw::TargetContainer *tc = nullptr;
//    SW_UNIMPLEMENTED;
//    /*for (auto &[pkg, tgts] : b->getTargetsToBuild())
//    {
//        if (pkg.getPath().isRelative())
//            tc = &tgts;
//    }*/
//    if (!tc)
//        throw SW_RUNTIME_ERROR("No relative targets found");
//    if (tc->empty())
//        throw SW_RUNTIME_ERROR("Empty cfg target");
//    auto &t = **tc->begin();
//    auto is = t.getInterfaceSettings();
//    auto m = std::make_unique<boost::dll::experimental::smart_library>(is["output_file"].getValue(),
//        boost::dll::load_mode::rtld_now | boost::dll::load_mode::rtld_global);
//    if (m->symbol_storage().get_function<std::map<std::string, std::string>()>("createJsonSettings").empty())
//        throw SW_RUNTIME_ERROR("Cannot find 'std::map<std::string, std::string> createJsonSettings()'");
//
//    auto selected_cfgs = std::set<String>(options.settings_file_config.begin(), options.settings_file_config.end());
//    auto result = m->get_function<std::map<std::string, std::string>()>("createJsonSettings")();
//    std::vector<sw::PackageSettings> r;
//    for (auto &[k, v] : result)
//    {
//        if (v.empty())
//            throw SW_RUNTIME_ERROR("Empty settings");
//        if (selected_cfgs.empty() || selected_cfgs.find(k) != selected_cfgs.end())
//        {
//            sw::PackageSettings ts;
//            ts.mergeFromString(v);
//            r.push_back(ts);
//        }
//    }
//    return r;
}

static std::vector<sw::PackageSettings> getSettingsFromFile(SwClientContext &swctx, const Options &options)
{
    std::vector<sw::PackageSettings> ts;
    for (auto &fn : options.settings_file)
    {
        if (fn.extension() == ".json")
        {
            sw::PackageSettings s;
            applySettingsFromJson(s, read_file(fn));
            ts.push_back(s);
        }
        else if (fn.extension() == ".cpp")
        {
            auto ts1 = applySettingsFromCppFile(swctx, options, fn);
            ts.insert(ts.end(), ts1.begin(), ts1.end());
        }
        else
            throw SW_RUNTIME_ERROR("Unknown settings file: " + to_string(normalize_path(fn)));
    }
    return ts;
}

Inputs::Inputs(const String &s)
{
    if (s.empty())
        throw SW_RUNTIME_ERROR("Empty input");
    inputs.push_back(s);
}

Inputs::Inputs(const Strings &s)
{
    for (auto &v : s)
    {
        if (!v.empty())
            inputs.push_back(v);
    }
    if (inputs.empty())
        throw SW_RUNTIME_ERROR("Empty inputs");
}

Inputs::Inputs(const Strings &s, const Strings &pairs)
{
    for (auto &v : s)
    {
        if (!v.empty())
            inputs.push_back(v);
    }

    if (inputs.empty() && pairs.empty())
        throw SW_RUNTIME_ERROR("Empty inputs and input pairs");

    if (pairs.size() % 2 == 1)
        throw SW_RUNTIME_ERROR("Incorrect input settings pairs. Something is missing. Size must be even, but size = " + std::to_string(pairs.size()));
    for (int i = 0; i < pairs.size(); i += 2)
    {
        sw::PackageSettings s;
        s.mergeFromString(pairs[i + 1]);
        if (pairs[i].empty())
            throw SW_RUNTIME_ERROR("Empty input in pair");
        input_pairs.push_back({s, pairs[i]});
    }
}

SwClientContext::SwClientContext(const Options &options)
    : local_storage_root_dir(options.storage_dir.empty() ? sw::Settings::get_user_settings().storage_dir : options.storage_dir)
    , options(std::make_unique<Options>(options))
{
    // maybe put outside ctx, because it will be recreated every time
    // but since this is a rare operation, maybe it's fine
    executor = std::make_unique<Executor>(select_number_of_threads(options.global_jobs));
    getExecutor(executor.get());
}

SwClientContext::~SwClientContext()
{
}

std::unique_ptr<sw::SwBuild> SwClientContext::createBuild()
{
    return createBuildInternal();
}

std::unique_ptr<sw::SwBuild> SwClientContext::createBuildInternal()
{
    SW_UNIMPLEMENTED;
//    auto b = getContext().createBuild();
//    auto &options = getOptions();
//
//    b->setName(options.build_name);
//
//    sw::PackageSettings bs;
//
//    // this is coming from the outside to distinguish from
//    // internal builds (checks, scripts builds)
//    bs["master_build"] = "true";
//
//    std::optional<bool> use_lock;
//    if (getOptions().getClOptions().use_lock_file.getNumOccurrences()) // always respect when specified
//        use_lock = options.use_lock_file;
//    if (!use_lock) // try heuristics
//    {
//        use_lock = fs::exists(fs::current_path() / "sw.lock");
//        if (*use_lock)
//            LOG_INFO(logger, "Lock file is found, using it: " << fs::current_path() / "sw.lock");
//    }
//    if (*use_lock)
//    {
//        // save lock file near input? what if we have multiple inputs?
//        if (options.lock_file.empty())
//            bs["lock_file"] = to_string(normalize_path(fs::current_path() / "sw.lock"));
//        else
//            bs["lock_file"] = to_string(normalize_path(options.lock_file));
//    }
//
//#define SET_BOOL_OPTION(x) if (options.x) bs[#x] = true;
//
//    //
//    SET_BOOL_OPTION(build_always);
//    SET_BOOL_OPTION(use_saved_configs);
//    if (!options.options_build.ide_copy_to_dir.empty())
//        bs["build_ide_copy_to_dir"] = to_string(normalize_path(options.options_build.ide_copy_to_dir));
//    if (!options.options_build.ide_fast_path.empty())
//        bs["build_ide_fast_path"] = to_string(normalize_path(options.options_build.ide_fast_path));
//    if (options.skip_errors)
//        bs["skip_errors"] = std::to_string(options.skip_errors);
//
//    SET_BOOL_OPTION(time_trace);
//    SET_BOOL_OPTION(show_output);
//    SET_BOOL_OPTION(write_output_to_file);
//
//    if (!options.options_build.time_limit.empty())
//        bs["time_limit"] = options.options_build.time_limit;
//    if (options.verbose || options.trace)
//        bs["measure"] = "true";
//    bs["verbose"] = (options.verbose || options.trace) ? "true" : "";
//
//    SET_BOOL_OPTION(standalone);
//    SET_BOOL_OPTION(do_not_mangle_object_names);
//    SET_BOOL_OPTION(ignore_source_files_errors);
//
//    // checks
//    SET_BOOL_OPTION(checks_single_thread);
//    SET_BOOL_OPTION(print_checks);
//    SET_BOOL_OPTION(wait_for_cc_checks);
//    bs["cc_checks_command"] = options.cc_checks_command;
//
//#undef SET_BOOL_OPTION
//
//    for (auto &t : options.targets_to_build)
//        bs["target-to-build"].push_back(t);
//    for (auto &t : options.targets_to_ignore)
//        bs["target-to-exclude"].push_back(t);
//    if (options.build_jobs)
//        bs["build-jobs"] = std::to_string(select_number_of_threads(options.build_jobs));
//    if (options.prepare_jobs)
//        bs["prepare-jobs"] = std::to_string(select_number_of_threads(options.prepare_jobs));
//    for (auto &t : options.Dvariables)
//    {
//        auto p = t.find('=');
//        bs["D"][t.substr(0, p)] = t.substr(p + 1);
//    }
//    b->setSettings(bs);
//
//    return b;
}

std::unique_ptr<sw::SwBuild> SwClientContext::createBuildAndPrepare(const Inputs &i)
{
    auto b = createBuild(i);
    b->loadInputs();
    SW_UNIMPLEMENTED;
    //b->setTargetsToBuild();
    //b->resolvePackages();
    //b->loadPackages();
    //b->prepare();
    return std::move(b);
}

Strings &SwClientContext::getInputs()
{
    return getOptions().getClOptions().getStorage().inputs;
}

const Strings &SwClientContext::getInputs() const
{
    return getOptions().getClOptions().getStorage().inputs;
}

void SwClientContext::addInputs(sw::SwBuild &b, const Inputs &i)
{
    SW_UNIMPLEMENTED;
    /*for (auto &[ts,in] : i.getInputPairs())
    {
        for (auto &i : getContext().makeInput(in))
        {
            i.addSettings(ts);
            b.addInput(i);
        }
    }

    auto settings = createSettings();
    for (auto &a : i.getInputs())
    {
        for (auto &i : getContext().makeInput(a))
        {
            for (auto &s : settings)
                i.addSettings(s);
            b.addInput(i);
        }
    }*/
}

std::vector<sw::UserInput> SwClientContext::makeCurrentPathInputs()
{
    SW_UNIMPLEMENTED;
    //return getContext().makeInput(fs::current_path());
}

std::unique_ptr<sw::SwBuild> SwClientContext::createBuildWithDefaultInputs()
{
    return createBuild({getInputs(), getOptions().input_settings_pairs});
}

std::unique_ptr<sw::SwBuild> SwClientContext::createBuild(const Inputs &i)
{
    auto b = createBuildInternal();
    addInputs(*b, i);
    return b;
}

sw::PackageSettings SwClientContext::createInitialSettings()
{
    auto s = getContext().getHostSettings();
    return s;
}

std::vector<sw::PackageSettings> SwClientContext::createSettings()
{
    auto &options = getOptions();

    auto initial_settings = createInitialSettings();

    if (options.use_same_config_for_host_dependencies)
    {
        initial_settings["use_same_config_for_host_dependencies"] = "true";
        initial_settings["use_same_config_for_host_dependencies"].ignoreInComparison(true);
        getContext().setHostSettings(initial_settings);
    }

    if (!options.host_settings_file.empty())
    {
        auto s = getContext().getHostSettings();
        applySettingsFromJson(s, read_file(options.host_settings_file));
        getContext().setHostSettings(s);
        if (s["host"])
            LOG_WARN(logger, "'host' key present in host settings. Probably misuse. Remove it and put everything under root.");
    }

    if (options.static_dependencies)
        initial_settings["static-deps"] = "true";
    if (options.reproducible_build)
        initial_settings["reproducible-build"] = "true";

    std::vector<sw::PackageSettings> settings;
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
        if (getOptions().getClOptions().static_build.getPosition() > getOptions().getClOptions().shared_build.getPosition())
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
        if (getOptions().getClOptions().win_mt.getPosition() > getOptions().getClOptions().win_md.getPosition())
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
        auto [k, v] = osTypeFromStringCaseI(options.os[i]);
        s["os"]["kernel"] = k;
        if (v)
            s["os"]["version"] = v->toString();
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

    // compiler & linker
    {
        auto csz = options.compiler.size();
        auto lsz = options.linker.size();
        if (csz != 0 && lsz != 0 && csz != lsz)
            throw SW_RUNTIME_ERROR("Number of linker entries must match compiler entries.");
        if (csz == 0 && lsz != 0 && lsz != 1)
            throw SW_RUNTIME_ERROR("You cannot provide more than one linker if compilers are not explicit.");

        mult_and_action(csz, [&options, &csz, &lsz](auto &s, int i)
        {
            if (options.compiler[i] == "clang-cl")
                options.compiler[i] = "clangcl";
            s.mergeAndAssign(compilerTypeFromStringCaseI(options.compiler[i]));
            if (csz == lsz)
                s.mergeAndAssign(linkerTypeFromStringCaseI(options.linker[i]));
        });

        // set only linker
        if (csz == 0 && lsz != 0 && lsz == 1)
        {
            for (auto &s : settings)
                s.mergeAndAssign(linkerTypeFromStringCaseI(options.linker[0]));
        }
    }

    // settings
    mult_and_action(options.settings.size(), [&options](auto &s, int i)
    {
        applySettings(s, options.settings[i]);
    });

    // settings-file
    auto sf = getSettingsFromFile(*this, options);
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
        auto s = getContext().getHostSettings();
        s.mergeAndAssign(settings[0]["host"].getMap());
        getContext().setHostSettings(s);
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
            s["output_dir"] = to_string(normalize_path(d));
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
            s["name"].ignoreInComparison(true);
        }
        LOG_DEBUG(logger, "WARNING: Setting config names may result in wrong config-name pair assignment, "
            "because of unspecified config creation order.");
    }

    return settings;
}

void SwClientContext::initNetwork()
{
    setHttpSettings(getOptions());
}

sw::SwContext &SwClientContext::getContext(bool in_allow_network)
{
    if (swctx_)
        return *swctx_;

    bool allow_network = in_allow_network && !getOptions().no_network;

    // load proxy settings before SwContext
    if (allow_network)
        initNetwork();

    auto &u = sw::Settings::get_user_settings();

    // remotes
    u.setDefaultRemote(getOptions().default_remote);

    // db
    u.gForceServerQuery = getOptions().force_server_query;
    u.gForceServerDatabaseUpdate = getOptions().force_server_db_check;

    // commands
    u.save_failed_commands = getOptions().save_failed_commands;
    u.save_all_commands = getOptions().save_all_commands;
    u.save_executed_commands = getOptions().save_executed_commands;

    u.explain_outdated = getOptions().explain_outdated;
    u.explain_outdated_full = getOptions().explain_outdated_full;
    u.gExplainOutdatedToTrace = getOptions().explain_outdated_to_trace;

    u.save_command_format = getOptions().save_command_format;

    //
    sw::PackageSettings cs;
#define SET_BOOL_OPTION(x) cs[#x] = getOptions().x ? "true" : ""
    SET_BOOL_OPTION(debug_configs);
    SET_BOOL_OPTION(ignore_outdated_configs);
    SET_BOOL_OPTION(do_not_remove_bad_module);
#undef SET_BOOL_OPTION

    // create ctx
    swctx_ = std::make_unique<sw::SwContext>(local_storage_root_dir, allow_network);
    //swctx_->setSettings(cs);
    // TODO:
    // before default?
    //for (auto &d : drivers)
    //swctx->registerDriver(std::make_unique<sw::driver::cpp::Driver>());
    swctx_->registerDriver(sw::driver::cpp::Driver::getPackageId(), std::make_unique<sw::driver::cpp::Driver>(*swctx_));
    //swctx->registerDriver(std::make_unique<sw::CDriver>(sw_create_driver));
    return *swctx_;
}

void SwClientContext::resetContext()
{
    swctx_.reset();
}

const sw::TargetMap &SwClientContext::getPredefinedTargets(sw::SwContext &swctx)
{
    if (!tm)
    {
        sw::TargetMap tm;
        SW_UNIMPLEMENTED;
        //sw::getProgramDetector().detectProgramsAndLibraries(swctx, tm);
        this->tm = tm;
    }
    return *tm;
}

String SwClientContext::listPredefinedTargets()
{
    using OrderedTargetMap = sw::PackageVersionMapBase<sw::TargetContainer, std::map, sw::VersionMap>;

    OrderedTargetMap m;
    for (auto &[pkg, tgts] : getPredefinedTargets(getContext()))
        m[pkg] = tgts;
    primitives::Emitter ctx;
    for (auto &[pkg, tgts] : m)
    {
        ctx.addLine(pkg.toString());
    }
    return ctx.getText();
}

String SwClientContext::listPrograms()
{
    auto m = getPredefinedTargets(getContext());

    primitives::Emitter ctx("  ");
    ctx.addLine("List of detected programs:");

    auto print_program = [&m, &ctx](const sw::PackagePath &p, const String &title)
    {
        ctx.increaseIndent();
        auto i = m.find(p);
        if (i != m.end(p) && !i->second.empty())
        {
            ctx.addLine(title + ":");
            ctx.increaseIndent();
            if (!i->second.releases().empty())
                ctx.addLine("release:");

            auto add_archs = [](auto &tgts)
            {
                String a;
                for (auto &tgt : tgts)
                {
                    auto &s = tgt->getSettings();
                    if (s["os"]["arch"])
                        a += s["os"]["arch"].getValue() + ", ";
                }
                if (!a.empty())
                {
                    a.resize(a.size() - 2);
                    a = " (" + a + ")";
                }
                return a;
            };

            ctx.increaseIndent();
            for (auto &[v,tgts] : i->second.releases())
            {
                ctx.addLine("- " + v.toString());
                ctx.addText(add_archs(tgts));
            }
            ctx.decreaseIndent();
            if (std::any_of(i->second.begin(), i->second.end(), [](const auto &p) { return !p.first.isRelease(); }))
            {
                ctx.addLine("preview:");
                ctx.increaseIndent();
                for (auto &[v, tgts] : i->second)
                {
                    if (v.isRelease())
                        continue;
                    ctx.addLine("- " + v.toString());
                    ctx.addText(add_archs(tgts));
                }
                ctx.decreaseIndent();
            }
            ctx.decreaseIndent();
        }
        ctx.decreaseIndent();
    };

    print_program("com.Microsoft.VisualStudio.VC.cl", "Microsoft Visual Studio C/C++ Compiler (short form - msvc)");
    print_program("org.LLVM.clang", "Clang C/C++ Compiler (short form - clang)");
    print_program("org.LLVM.clangcl", "Clang C/C++ Compiler in MSVC compatibility mode (short form - clangcl)");
    print_program("com.Apple.clang", "Apple Clang C/C++ Compiler");

    ctx.addLine();
    ctx.addLine("Use short program form plus version to select it for use.");
    ctx.addLine("   short-version");
    ctx.addLine("Examples: msvc-19.16, msvc-19.24-preview, clang-10");

    return ctx.getText();
}

Programs SwClientContext::listCompilers()
{
    auto m = getPredefinedTargets(getContext());

    Programs progs;

    auto print_program = [&m, &progs](const sw::PackagePath &p, const String &title)
    {
        Program prog;
        prog.ppath = p;
        prog.desc = title;
        auto i = m.find(p);
        if (i != m.end(p) && !i->second.empty())
        {
            for (auto &[v,tgts] : i->second.releases())
                prog.releases[{p, v}] = { &tgts };
            if (std::any_of(i->second.begin(), i->second.end(), [](const auto &p) { return !p.first.isRelease(); }))
            {
                for (auto &[v, tgts] : i->second)
                {
                    if (v.isRelease())
                        continue;
                    prog.prereleases[{p, v}] = { &tgts };
                }
            }
            progs.push_back(prog);
        }
    };

    print_program("com.Microsoft.VisualStudio.VC.cl", "Microsoft Visual Studio C/C++ Compiler");
    print_program("org.LLVM.clang", "Clang C/C++ Compiler");
    print_program("org.LLVM.clangcl", "Clang C/C++ Compiler in MSVC compatibility mode (clang-cl)");
    print_program("com.Apple.clang", "Apple Clang C/C++ Compiler");

    return progs;
}

StringSet SwClientContext::listCommands()
{
    StringSet cmds;
#define SUBCOMMAND(n) cmds.insert(#n);
#include "commands.inl"
#undef SUBCOMMAND
    return cmds;
}

void setupLogger(const std::string &log_level, const Options &options, bool simple)
{
    LoggerSettings log_settings;
    log_settings.log_level = log_level;
    if (options.write_log_to_file && 1/*bConsoleMode*/)
        log_settings.log_file = (sw::support::get_root_directory() / "sw").string();
    log_settings.simple_logger = simple;
    log_settings.print_trace = true;
    initLogger(log_settings);

    // first trace message
    LOG_TRACE(logger, "----------------------------------------");
    LOG_TRACE(logger, "Starting sw...");
}
