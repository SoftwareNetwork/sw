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

#include <sw/builder/jumppad.h>
#include <sw/core/input.h>
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

#include <iostream>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "main");

#ifdef _WIN32
#include <primitives/win32helpers.h>
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

#if _MSC_VER
#if defined(SW_USE_JEMALLOC)
#define JEMALLOC_NO_PRIVATE_NAMESPACE
#include <jemalloc-5.1.0/include/jemalloc/jemalloc.h>
//#include <jemalloc-5.1.0/src/jemalloc_cpp.cpp>
#endif
#endif

//#include <mimalloc.h>

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
static ::cl::opt<int> jobs("j", ::cl::desc("Number of jobs"));

static ::cl::opt<bool> cl_self_upgrade("self-upgrade", ::cl::desc("Upgrade client"));
static ::cl::opt<path> cl_self_upgrade_copy("internal-self-upgrade-copy", ::cl::desc("Upgrade client: copy file"), ::cl::ReallyHidden);

extern std::map<sw::PackagePath, sw::Version> gUserSelectedPackages;
static ::cl::list<String> cl_activate("activate", ::cl::desc("Activate specific packages"));

extern ::cl::opt<path> build_ide_fast_path;

#define SUBCOMMAND(n) extern ::cl::SubCommand subcommand_##n;
#include "command/commands.inl"
#undef SUBCOMMAND

// TODO: https://github.com/tomtom-international/cpp-dependencies
static ::cl::list<bool> build_graph("g", ::cl::desc("Print .dot graph of build targets"), ::cl::sub(subcommand_build));

static ::cl::list<path> internal_sign_file("internal-sign-file", ::cl::value_desc("<file> <private.key>"), ::cl::desc("Sign file with private key"), ::cl::ReallyHidden, ::cl::multi_val(2));
static ::cl::list<path> internal_verify_file("internal-verify-file", ::cl::value_desc("<file> <sigfile> <public.key>"), ::cl::desc("Verify signature with public key"), ::cl::ReallyHidden, ::cl::multi_val(3));

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

//
#include <sw/core/c.hpp>

sw_driver_t sw_create_driver(void);

//static ::cl::list<String> drivers("load-driver", ::cl::desc("Load more drivers"), ::cl::CommaSeparated);

int setup_main(const Strings &args)
{
    // some initial stuff
    // try to do as less as possible before log init

    if (!working_directory.empty())
    {
        working_directory = primitives::filesystem::canonical(working_directory);
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

    if (!build_ide_fast_path.empty() && fs::exists(build_ide_fast_path))
    {
        auto files = read_lines(build_ide_fast_path);
        uint64_t mtime = 0;
        bool missing = false;
        for (auto &f : files)
        {
            if (!fs::exists(f))
            {
                missing = true;
                break;
            }
            auto lwt = fs::last_write_time(f);
            mtime ^= file_time_type2time_t(lwt);
        }
        if (!missing)
        {
            path fmtime = build_ide_fast_path;
            fmtime += ".t";
            if (fs::exists(fmtime) && mtime == std::stoull(read_file(fmtime)))
                return 0;
            write_file(fmtime, std::to_string(mtime));
        }
    }

    // after everything
    std::unique_ptr<Executor> e;
    {
        e = std::make_unique<Executor>(select_number_of_threads(jobs));
        getExecutor(e.get());
    }

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

    //
    ::cl::ParseCommandLineOptions(args, overview);

    // post setup args

    for (sw::PackageId p : cl_activate)
        gUserSelectedPackages[p.getPath()] = p.getVersion();

    return setup_main(args);
}

int main(int argc, char **argv)
{
    //mi_version();

#ifdef _WIN32
    CoInitializeEx(0, 0); // vs find helper
#endif

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
extern bool gRunAppInContainer;
static ::cl::opt<bool, true> run_app_in_container("in-container", ::cl::desc("Print file with build graph"), ::cl::location(gRunAppInContainer), ::cl::sub(subcommand_uri));

bool gUseLockFile;
static ::cl::opt<bool, true> use_lock_file("l", ::cl::desc("Use lock file"), ::cl::location(gUseLockFile));// , cl::init(true));

//static ::cl::list<String> builtin_function(sw::builder::getInternalCallBuiltinFunctionName(), ::cl::desc("Call built-in function"), ::cl::Hidden);

int sw_main(const Strings &args)
{
    if (gUseLockFile && fs::exists(fs::current_path() / "sw.lock"))
    {
        SW_UNIMPLEMENTED;
        //getPackageStore().loadLockFile(fs::current_path() / "sw.lock");
    }

    if (0);
#define SUBCOMMAND(n) else if (subcommand_##n) { cli_##n(); return 0; }
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

/*SUBCOMMAND_DECL(mirror)
{
    enum storage_file_type
    {
        SourceArchive,
        SpecificationFirstFile,
        About,
        BuildArchive, // binary archive?
    };
}*/

/*SUBCOMMAND_DECL(pack)
{
    // http://www.king-foo.com/2011/11/creating-debianubuntu-deb-packages/
    SW_UNIMPLEMENTED;
}*/

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
