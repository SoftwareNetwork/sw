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

#include "command/commands.h"

#include <sw/builder/file.h>
#include <sw/builder/jumppad.h>
#include <sw/driver/command.h>
#include <sw/driver/build.h>
#include <sw/driver/driver.h>
#include <sw/manager/api.h>
#include <sw/manager/database.h>
#include <sw/manager/package_data.h>
#include <sw/manager/settings.h>
#include <sw/manager/storage.h>
#include <sw/support/exceptions.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string_regex.hpp>
#include <boost/dll.hpp>
#include <boost/regex.hpp>
#include <primitives/emitter.h>
#include <primitives/executor.h>
#include <primitives/file_monitor.h>
#include <primitives/lock.h>
#include <primitives/pack.h>
#include <primitives/sw/cl.h>
#include <primitives/sw/settings_program_name.h>
#include <primitives/sw/main.h>
#include <primitives/thread.h>
#include <primitives/win32helpers.h>

#include <iostream>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "main");

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <Objbase.h>
#include <Shlobj.h>

#include <WinReg.hpp>
#endif

#if _MSC_VER
#if defined(SW_USE_TBBMALLOC)
#include "tbb/include/tbb/tbbmalloc_proxy.h"
#elif defined(SW_USE_TCMALLOC)
//"libtcmalloc_minimal.lib"
//#pragma comment(linker, "/include:__tcmalloc")
#else
#endif
#endif

using namespace sw;

bool bConsoleMode = true;
bool bUseSystemPause = false;

/*
// check args here to see if we want gui or not!

// 1. if 'uri' arg - console depends
// 2. if no args, no sw.cpp, *.sw files in cwd - gui
*/

#pragma push_macro("main")
#undef main
int main(int argc, char **argv);
#pragma pop_macro("main")

int sw_main(const Strings &args);
void stop();
void setup_log(const std::string &log_level, bool simple = true);
void self_upgrade();
void self_upgrade_copy(const path &dst);

#ifdef _WIN32
/*int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
    const std::wstring s = GetCommandLine();
    bConsoleMode = s.find(L"uri sw:") == -1;
    if (bConsoleMode)
    {
        SetupConsole();
    }
    else
    {
        CoInitialize(0);
    }

#pragma push_macro("main")
#undef main
    return main(__argc, __argv);
#pragma pop_macro("main")
}*/
#endif

extern bool gForceServerQuery;
static ::cl::opt<bool, true> force_server_query1("s", ::cl::desc("Force server check"), ::cl::location(gForceServerQuery));
static ::cl::alias force_server_query2("server", ::cl::desc("Alias for -s"), ::cl::aliasopt(force_server_query1));

static ::cl::opt<path> working_directory("d", ::cl::desc("Working directory"));
extern bool gVerbose;
static ::cl::opt<bool, true> verbose_opt("verbose", ::cl::desc("Verbose output"), ::cl::location(gVerbose));
static ::cl::alias verbose_opt2("v", ::cl::desc("Alias for -verbose"), ::cl::aliasopt(verbose_opt));
static ::cl::opt<bool> trace("trace", ::cl::desc("Trace output"));
extern int gNumberOfJobs;
static ::cl::opt<int, true> jobs("j", ::cl::desc("Number of jobs"), ::cl::location(gNumberOfJobs));

static ::cl::opt<int> sleep_seconds("sleep", ::cl::desc("Sleep on startup"), ::cl::Hidden);

static ::cl::opt<bool> cl_self_upgrade("self-upgrade", ::cl::desc("Upgrade client"));
static ::cl::opt<path> cl_self_upgrade_copy("internal-self-upgrade-copy", ::cl::desc("Upgrade client: copy file"), ::cl::ReallyHidden);

extern std::map<sw::PackagePath, sw::Version> gUserSelectedPackages;
static ::cl::list<String> cl_activate("activate", ::cl::desc("Activate specific packages"));

extern ::cl::opt<bool> useFileMonitor;

static ::cl::opt<path> storage_dir_override("storage-dir");

#define SUBCOMMAND(n, d) ::cl::SubCommand subcommand_##n(#n, d);
#include "command/commands.inl"
#undef SUBCOMMAND

extern path gIdeFastPath;
static ::cl::opt<path, true> build_ide_fast_path("ide-fast-path", ::cl::sub(subcommand_build), ::cl::Hidden, ::cl::location(gIdeFastPath));
extern path gIdeCopyToDir;
static ::cl::opt<path, true> build_ide_copy_to_dir("ide-copy-to-dir", ::cl::sub(subcommand_build), ::cl::Hidden, ::cl::location(gIdeCopyToDir));
// TODO: https://github.com/tomtom-international/cpp-dependencies
static ::cl::list<bool> build_graph("g", ::cl::desc("Print .dot graph of build targets"), ::cl::sub(subcommand_build));

static ::cl::list<path> internal_sign_file("internal-sign-file", ::cl::value_desc("<file> <private.key>"), ::cl::desc("Sign file with private key"), ::cl::ReallyHidden, ::cl::multi_val(2));
static ::cl::list<path> internal_verify_file("internal-verify-file", ::cl::value_desc("<file> <sigfile> <public.key>"), ::cl::desc("Verify signature with public key"), ::cl::ReallyHidden, ::cl::multi_val(3));

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

static ::cl::opt<bool> curl_verbose("curl-verbose");
static ::cl::opt<bool> ignore_ssl_checks("ignore-ssl-checks");

extern ::cl::list<String> build_arg;
extern ::cl::list<String> build_arg_test;

#include <sw/core/c.hpp>

sw_driver_t sw_create_driver(void);

std::unique_ptr<sw::SwContext> createSwContext()
{
    // load proxy settings early
    httpSettings.verbose = curl_verbose;
    httpSettings.ignore_ssl_checks = ignore_ssl_checks;
    httpSettings.proxy = Settings::get_local_settings().proxy;

    auto swctx = std::make_unique<sw::SwContext>(storage_dir_override.empty() ? sw::Settings::get_user_settings().storage_dir : storage_dir_override);
    swctx->registerDriver(std::make_unique<sw::driver::cpp::Driver>(*swctx));
    //swctx->registerDriver(std::make_unique<sw::CDriver>(sw_create_driver));
    return swctx;
}

int setup_main(const Strings &args)
{
    // some initial stuff

    if (sleep_seconds > 0)
        std::this_thread::sleep_for(std::chrono::seconds(sleep_seconds));

    // try to do as less as possible before log init

    if (!working_directory.empty())
    {
        if (fs::is_regular_file(working_directory))
            fs::current_path(working_directory.parent_path());
        else
            fs::current_path(working_directory);

#ifdef _WIN32
        sw_append_symbol_path(fs::current_path());
#endif
    }

    if (trace)
        setup_log("TRACE");// , false); // add modules for trace logger
    else if (gVerbose)
        setup_log("DEBUG");
    else
        setup_log("INFO");

    // after log initialized

    if (!cl_self_upgrade_copy.empty())
    {
        self_upgrade_copy(cl_self_upgrade_copy);
        return 0;
    }

    if (cl_self_upgrade)
    {
        self_upgrade();
        return 0;
    }

    if (!internal_sign_file.empty())
    {
        SW_UNIMPLEMENTED;
        //ds_sign_file(internal_sign_file[0], internal_sign_file[1]);
        return 0;
    }

    if (!internal_verify_file.empty())
    {
        SW_UNIMPLEMENTED;
        //ds_verify_file(internal_verify_file[0], internal_verify_file[1], internal_verify_file[2]);
        return 0;
    }

    // before storages
    // Create QSBR context for the main thread.
    //auto context = createConcurrentContext();
    //getConcurrentContext(&context);

    SCOPE_EXIT
    {
        // Destroy the QSBR context for the main thread.
        //destroyConcurrentContext(context);
    };

    // after everything
    std::unique_ptr<Executor> e;
    if (gIdeFastPath.empty())
    {
        e = std::make_unique<Executor>(select_number_of_threads(jobs));
        getExecutor(e.get());
    }

    /*boost::asio::io_context io_service;
    boost::asio::signal_set signals(io_service, SIGINT);
    signals.async_wait([&e]()
    {
        e.stop();
    });
    io_service.run();*/

    // actual execution
    return sw_main(args);
}

int parse_main(int argc, char **argv)
{
    //args::ValueFlag<int> configuration(parser, "configuration", "Configuration to build", { 'c' });

    String overview = "SW: Software Network Client\n"
        "\n"
        "  SW is a Universal Package Manager and Build System\n";

    std::vector<std::string> args0(argv + 1, argv + argc);
    Strings args;
    args.push_back(argv[0]);
    for (auto &a : args0)
    {
        std::vector<std::string> t;
        //boost::replace_all(a, "%5F", "_");
        boost::split_regex(t, a, boost::regex("%20"));
        args.insert(args.end(), t.begin(), t.end());
    }

    if (args.size() > 1 && args[1] == sw::builder::getInternalCallBuiltinFunctionName())
    {
        // name of subcommand must outlive it (StringRef is used)
        auto n = sw::builder::getInternalCallBuiltinFunctionName();
        ::cl::SubCommand subcommand_icbf(n, "");
        ::cl::opt<String> icbf_arg(::cl::Positional, ::cl::sub(subcommand_icbf)); // module name
        ::cl::list<String> icbf_args(::cl::ConsumeAfter, ::cl::sub(subcommand_icbf)); // rest

        ::cl::ParseCommandLineOptions(args);

        Strings args;
        args.push_back(argv[0]);
        args.push_back(sw::builder::getInternalCallBuiltinFunctionName());
        args.push_back(icbf_arg);
        args.insert(args.end(), icbf_args.begin(), icbf_args.end());
        return jumppad_call(args);
    }

    //useFileMonitor = false;

    //
    ::cl::ParseCommandLineOptions(args, overview);

    // post setup args

    if (build_arg.empty())
        build_arg.push_back(".");
    if (build_arg_test.empty())
        build_arg_test.push_back(".");

    for (sw::PackageId p : cl_activate)
        gUserSelectedPackages[p.ppath] = p.version;

    return setup_main(args);
}

int main(int argc, char **argv)
{
    int r = 0;
    String error;
    bool supress = false;
    try
    {
        r = parse_main(argc, argv);
    }
    catch (SupressOutputException &)
    {
        supress = true;
    }
    catch (const std::exception &e)
    {
        error = e.what();
    }

    stop();

    if (!error.empty() || supress)
    {
        if (!supress)
        {
            LOG_ERROR(logger, error);
#ifdef _WIN32
            //if (IsDebuggerPresent())
                //system("pause");
#else
            //std::cout << "Press any key to continue..." << std::endl;
            //getchar();
#endif
        }
        r = 1;

        if (!bConsoleMode)
        {
#ifdef _WIN32
            if (bUseSystemPause)
            {
                system("pause");
            }
            else
                message_box(sw::getProgramName(), error);
#endif
        }
    }

    LOG_FLUSH();

    return r;
}

//
//static ::cl::list<path> build_arg0(::cl::Positional, ::cl::desc("Files or directoris to build"));

// ide commands
static ::cl::opt<String> target_build("target", ::cl::desc("Target to build")/*, ::cl::sub(subcommand_ide)*/);
static ::cl::opt<String> ide_rebuild("rebuild", ::cl::desc("Rebuild target"), ::cl::sub(subcommand_ide));
static ::cl::opt<String> ide_clean("clean", ::cl::desc("Clean target"), ::cl::sub(subcommand_ide));

//static ::cl::list<String> override_package("override-remote-package", ::cl::value_desc("prefix sdir"), ::cl::desc("Provide a local copy of remote package"), ::cl::multi_val(2));
static ::cl::opt<String> override_package("override-remote-package", ::cl::value_desc("prefix"), ::cl::desc("Provide a local copy of remote package(s)"));
static ::cl::alias override_package2("override", ::cl::desc("Alias for -override-remote-package"), ::cl::aliasopt(override_package));
static ::cl::opt<bool> list_overridden_packages("list-overridden-remote-packages", ::cl::desc("List overridden packages"));
static ::cl::opt<String> delete_overridden_package("delete-overridden-remote-package", ::cl::value_desc("package"), ::cl::desc("Delete overridden package from index"));
static ::cl::opt<path> delete_overridden_package_dir("delete-overridden-remote-package-dir", ::cl::value_desc("sdir"), ::cl::desc("Delete overridden dir packages"));
static ::cl::alias delete_overridden_package_dir2("delete-override", ::cl::desc("Alias for -delete-overridden-remote-package-dir"), ::cl::aliasopt(delete_overridden_package_dir));

// uri commands
extern bool gRunAppInContainer;
static ::cl::opt<bool, true> run_app_in_container("in-container", ::cl::desc("Print file with build graph"), ::cl::location(gRunAppInContainer), ::cl::sub(subcommand_uri));

bool gUseLockFile;
static ::cl::opt<bool, true> use_lock_file("l", ::cl::desc("Use lock file"), ::cl::location(gUseLockFile));// , cl::init(true));

//static ::cl::list<String> builtin_function(sw::builder::getInternalCallBuiltinFunctionName(), ::cl::desc("Call built-in function"), ::cl::Hidden);

void override_package_perform(sw::SwContext &swctx);

int sw_main(const Strings &args)
{
    if (list_overridden_packages)
    {
        auto swctx = createSwContext();
        // sort
        std::set<sw::LocalPackage> pkgs;
        for (auto &p : swctx->getLocalStorage().getOverriddenPackagesStorage().getPackages())
            pkgs.emplace(p);
        for (auto &p : pkgs)
            std::cout << p.toString() << " " << *p.getOverriddenDir() << "\n";
        return 0;
    }

    if (!override_package.empty())
    {
        auto swctx = createSwContext();
        override_package_perform(*swctx);
        return 0;
    }

    if (!delete_overridden_package.empty())
    {
        auto swctx = createSwContext();
        sw::PackageId pkg{ delete_overridden_package };
        LOG_INFO(logger, "Delete override for " + pkg.toString());
        swctx->getLocalStorage().getOverriddenPackagesStorage().deletePackage(pkg);
        return 0;
    }

    if (!delete_overridden_package_dir.empty())
    {
        LOG_INFO(logger, "Delete override for sdir " + delete_overridden_package_dir.u8string());

        auto d = primitives::filesystem::canonical(delete_overridden_package_dir);

        auto swctx = createSwContext();
        std::set<sw::LocalPackage> pkgs;
        for (auto &p : swctx->getLocalStorage().getOverriddenPackagesStorage().getPackages())
        {
            if (*p.getOverriddenDir() == d)
                pkgs.emplace(p);
        }
        for (auto &p : pkgs)
            std::cout << "Deleting " << p.toString() << "\n";

        swctx->getLocalStorage().getOverriddenPackagesStorage().deletePackageDir(d);
        return 0;
    }

    if (gUseLockFile && fs::exists(fs::current_path() / "sw.lock"))
    {
        SW_UNIMPLEMENTED;
        //getPackageStore().loadLockFile(fs::current_path() / "sw.lock");
    }

    /*if (!build_arg0.empty())
    {
        sw::build(Files{ build_arg0.begin(), build_arg0.end() });
        return 0;
    }*/

    if (0);
#define SUBCOMMAND(n, d) else if (subcommand_##n) { cli_##n(); return 0; }
#include "command/commands.inl"
#undef SUBCOMMAND

    LOG_WARN(logger, "No command was issued");

    return 0;
}

void stop()
{
    if (gUseLockFile)
    {
        SW_UNIMPLEMENTED;
        //getPackageStore().saveLockFile(fs::current_path() / "sw.lock");
    }
}

static ::cl::opt<bool> write_log_to_file("log-to-file");

void setup_log(const std::string &log_level, bool simple)
{
    LoggerSettings log_settings;
    log_settings.log_level = log_level;
    if (write_log_to_file && bConsoleMode)
        log_settings.log_file = (get_root_directory() / "sw").string();
    log_settings.simple_logger = simple;
    log_settings.print_trace = true;
    initLogger(log_settings);

    // first trace message
    LOG_TRACE(logger, "----------------------------------------");
    LOG_TRACE(logger, "Starting sw...");
}

void override_package_perform(sw::SwContext &swctx)
{
    auto &i = swctx.addInput(fs::current_path());
    auto ts = swctx.getHostSettings();
    ts["driver"]["dry-run"] = "true";
    i.addSettings(ts);
    swctx.load();

    // one prepare step will find sources
    // maybe add explicit enum value
    //swctx.prepareStep();

    auto gn = swctx.getLocalStorage().getOverriddenPackagesStorage().getPackagesDatabase().getMaxGroupNumber() + 1;
    for (auto &[pkg, desc] : getPackages(swctx))
    {
        sw::PackagePath prefix = override_package;
        sw::PackageId pkg2{ prefix / pkg.ppath, pkg.version };
        auto dir = fs::absolute(".");
        LOG_INFO(logger, "Overriding " + pkg2.toString() + " to " + dir.u8string());
        // fix deps' prefix
        sw::UnresolvedPackages deps;
        for (auto &d : desc->getData().dependencies)
        {
            if (d.ppath.isAbsolute())
                deps.insert(d);
            else
                deps.insert({ prefix / d.ppath, d.range });
        }
        LocalPackage lp(swctx.getLocalStorage(), pkg2);
        PackageData d;
        d.sdir = dir;
        d.dependencies = deps;
        d.group_number = gn;
        d.prefix = (int)prefix.size();
        swctx.getLocalStorage().getOverriddenPackagesStorage().install(lp, d);
    }
}

SUBCOMMAND_DECL(mirror)
{
    enum storage_file_type
    {
        SourceArchive,
        SpecificationFirstFile,
        About,
        BuildArchive, // binary archive?
    };
}

SUBCOMMAND_DECL(ide)
{
    SW_UNIMPLEMENTED;

    /*auto swctx = createSwContext();
    if (!target_build.empty())
    {
        try_single_process_job(fs::current_path() / SW_BINARY_DIR / "ide", [&swctx]()
        {
            auto s = sw::load(swctx, working_directory);
            auto &b = *((sw::Build*)s.get());
            b.ide = true;
            auto pkg = sw::extractPackageIdFromString(target_build);
            b.TargetsToBuild[pkg] = b.children[pkg];
            s->execute();
        });
    }
    else
    {
        single_process_job(fs::current_path() / SW_BINARY_DIR / "ide", [&swctx]()
        {
            auto s = sw::load(swctx, working_directory);
            auto &b = *((sw::Build*)s.get());
            b.ide = true;
            s->execute();
        });
    }*/
}

SUBCOMMAND_DECL(configure)
{
    SW_UNIMPLEMENTED;

    // generate execution plan
}

SUBCOMMAND_DECL(pack)
{
    // http://www.king-foo.com/2011/11/creating-debianubuntu-deb-packages/
    SW_UNIMPLEMENTED;
}

String getBuildTime();
String getGitRev();

EXPORT_FROM_EXECUTABLE
std::string getVersionString()
{
    std::string s;
    s += ::sw::getProgramName();
    s += " version ";
    s += PACKAGE_VERSION;
    s += "\n";
    s += getGitRev();
    s += "assembled on " + getBuildTime();
    return s;
}

EXPORT_FROM_EXECUTABLE
std::string getProgramName()
{
    return PACKAGE_NAME_CLEAN;
}

#if _MSC_VER
#if defined(SW_USE_JEMALLOC)
#define JEMALLOC_NO_PRIVATE_NAMESPACE
#include <jemalloc-5.1.0/include/jemalloc/jemalloc.h>
//#include <jemalloc-5.1.0/src/jemalloc_cpp.cpp>
#endif
#endif
