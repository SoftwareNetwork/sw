// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "build.h"

//#include <resolver.h>
#include <sw/builder/file.h>
#include <sw/builder/sw_context.h>
#include <sw/driver/command.h>
#include <sw/driver/jumppad.h>
#include <sw/driver/solution_build.h>
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

int sw_main(sw::SwContext &swctx, const Strings &args);
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
bool gVerbose;
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
#include "commands.inl"
#undef SUBCOMMAND

static ::cl::list<String> build_arg_test(::cl::Positional, ::cl::desc("File or directory to use to generate projects"), ::cl::sub(subcommand_test));
static ::cl::list<String> build_arg(::cl::Positional, ::cl::desc("Files or directories to build (paths to config)"), ::cl::sub(subcommand_build));
extern path gIdeFastPath;
static ::cl::opt<path, true> build_ide_fast_path("ide-fast-path", ::cl::sub(subcommand_build), ::cl::Hidden, ::cl::location(gIdeFastPath));
extern path gIdeCopyToDir;
static ::cl::opt<path, true> build_ide_copy_to_dir("ide-copy-to-dir", ::cl::sub(subcommand_build), ::cl::Hidden, ::cl::location(gIdeCopyToDir));
// TODO: https://github.com/tomtom-international/cpp-dependencies
static ::cl::list<bool> build_graph("g", ::cl::desc("Print .dot graph of build targets"), ::cl::sub(subcommand_build));

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

int setup_main(const Strings &args)
{
    // some initial stuff

    if (sleep_seconds > 0)
        std::this_thread::sleep_for(std::chrono::seconds(sleep_seconds));

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

    // init
    sw::SwContext swctx(sw::Settings::get_user_settings().storage_dir);

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
    return sw_main(swctx, args);
}

int parse_main(int argc, char **argv)
{
    //args::ValueFlag<int> configuration(parser, "configuration", "Configuration to build", { 'c' });

    String overview = "SW: Software Network Client\n"
        "\n"
        "  SW is a Universal Package Manager and Build System...\n";
    {
        //overview += "\n  Available drivers:\n";
        //for (auto &d : driver)
            //overview += "    - " + d->getName() + "\n";
        overview += "\n  Available frontends:\n";
        for (const auto &n : Solution::getAvailableFrontendNames())
            overview += "    - " + n + "\n";
    }

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

    if (args.size() > 1 && args[1] == sw::driver::getInternalCallBuiltinFunctionName())
    {
        // name of subcommand must outlive it (StringRef is used)
        auto n = sw::driver::getInternalCallBuiltinFunctionName();
        ::cl::SubCommand subcommand_icbf(n, "");
        ::cl::opt<String> icbf_arg(::cl::Positional, ::cl::sub(subcommand_icbf)); // module name
        ::cl::list<String> icbf_args(::cl::ConsumeAfter, ::cl::sub(subcommand_icbf)); // rest

        ::cl::ParseCommandLineOptions(args);

        Strings args;
        args.push_back(argv[0]);
        args.push_back(sw::driver::getInternalCallBuiltinFunctionName());
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

#define SUBCOMMAND_DECL(n) void cli_##n(sw::SwContext &swctx)
#define SUBCOMMAND(n, d) SUBCOMMAND_DECL(n);
#include "commands.inl"
#undef SUBCOMMAND

//
//static ::cl::list<path> build_arg0(::cl::Positional, ::cl::desc("Files or directoris to build"));

// build commands
// must be opt<String>!
static ::cl::opt<String> build_arg_generate(::cl::Positional, ::cl::desc("File or directory to use to generate projects"), ::cl::init("."), ::cl::sub(subcommand_generate));
static ::cl::opt<String> build_arg_update(::cl::Positional, ::cl::desc("Update lock"), ::cl::init("."), ::cl::sub(subcommand_update));
static ::cl::opt<String> list_arg(::cl::Positional, ::cl::desc("Package regex to list"), ::cl::init("."), ::cl::sub(subcommand_list));
static ::cl::opt<String> install_arg(::cl::Positional, ::cl::desc("Packages to add"), ::cl::sub(subcommand_install));
static ::cl::list<String> install_args(::cl::ConsumeAfter, ::cl::desc("Packages to add"), ::cl::sub(subcommand_install));

// upload
static ::cl::opt<String> upload_remote(::cl::Positional, ::cl::desc("Remote name"), ::cl::sub(subcommand_upload));
static ::cl::opt<String> upload_prefix(::cl::Positional, ::cl::desc("Prefix path"), ::cl::sub(subcommand_upload), ::cl::Required);
static ::cl::opt<path> upload_path_to_root(::cl::Positional, ::cl::desc("Path to root"), ::cl::sub(subcommand_upload));
static ::cl::opt<bool> build_before_upload("build", ::cl::desc("Build before upload"), ::cl::sub(subcommand_upload));

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

//static ::cl::list<String> builtin_function(sw::driver::getInternalCallBuiltinFunctionName(), ::cl::desc("Call built-in function"), ::cl::Hidden);

void override_package_perform(sw::SwContext &swctx);

int sw_main(sw::SwContext &swctx, const Strings &args)
{
    if (list_overridden_packages)
    {
        // sort
        std::set<sw::LocalPackage> pkgs;
        for (auto &p : swctx.getLocalStorage().getOverriddenPackagesStorage().getPackages())
            pkgs.emplace(p);
        for (auto &p : pkgs)
            std::cout << p.toString() << " " << *p.getOverriddenDir() << "\n";
        return 0;
    }

    if (!override_package.empty())
    {
        override_package_perform(swctx);
        return 0;
    }

    if (!delete_overridden_package.empty())
    {
        sw::PackageId pkg{ delete_overridden_package };
        LOG_INFO(logger, "Delete override for " + pkg.toString());
        swctx.getLocalStorage().getOverriddenPackagesStorage().deletePackage(pkg);
        return 0;
    }

    if (!delete_overridden_package_dir.empty())
    {
        LOG_INFO(logger, "Delete override for sdir " + delete_overridden_package_dir.u8string());

        auto d = fs::canonical(delete_overridden_package_dir);

        std::set<sw::LocalPackage> pkgs;
        for (auto &p : swctx.getLocalStorage().getOverriddenPackagesStorage().getPackages())
        {
            if (*p.getOverriddenDir() == d)
                pkgs.emplace(p);
        }
        for (auto &p : pkgs)
            std::cout << "Deleting " << p.toString() << "\n";

        swctx.getLocalStorage().getOverriddenPackagesStorage().deletePackageDir(delete_overridden_package_dir);
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
#define SUBCOMMAND(n, d) else if (subcommand_##n) { cli_##n(swctx); return 0; }
#include "commands.inl"
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

#define SUBCOMMAND_DECL_URI(c) SUBCOMMAND_DECL(uri_ ## c)

static ::cl::opt<String> build_source_dir("S", ::cl::desc("Explicitly specify a source directory."), ::cl::sub(subcommand_build), ::cl::init("."));
static ::cl::opt<String> build_binary_dir("B", ::cl::desc("Explicitly specify a build directory."), ::cl::sub(subcommand_build), ::cl::init(SW_BINARY_DIR));

static ::cl::opt<bool> build_fetch("fetch", ::cl::desc("Fetch sources, then build"), ::cl::sub(subcommand_build));
static ::cl::opt<bool> build_after_fetch("build", ::cl::desc("Build after fetch"), ::cl::sub(subcommand_fetch));

SUBCOMMAND_DECL(build)
{
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

    sw::build(swctx, build_arg);
}

static ::cl::list<String> remove_arg(::cl::Positional, ::cl::desc("package to remove"), ::cl::sub(subcommand_remove));

SUBCOMMAND_DECL(remove)
{
    for (auto &a : remove_arg)
    {
        sw::LocalPackage p(swctx.getLocalStorage(), a);
        //sdb.removeInstalledPackage(p); // TODO: remove from db
        fs::remove_all(p.getDir());
    }
}

static ::cl::opt<String> create_type(::cl::Positional, ::cl::desc("<type>"), ::cl::sub(subcommand_create), ::cl::Required);
static ::cl::opt<String> create_proj_name(::cl::Positional, ::cl::desc("<project name>"), ::cl::sub(subcommand_create));

static ::cl::opt<String> create_template("template", ::cl::desc("Template project to create"), ::cl::sub(subcommand_create), ::cl::init("exe"));
static ::cl::alias create_template2("t", ::cl::desc("Alias for -template"), ::cl::aliasopt(create_template));
static ::cl::opt<String> create_language("language", ::cl::desc("Template project language to create"), ::cl::sub(subcommand_create), ::cl::init("cpp"));
static ::cl::alias create_language2("l", ::cl::desc("Alias for -language"), ::cl::aliasopt(create_language));
static ::cl::opt<bool> create_clear_dir("clear", ::cl::desc("Clear current directory"), ::cl::sub(subcommand_create));
static ::cl::opt<bool> create_clear_dir_y("y", ::cl::desc("Answer yes"), ::cl::sub(subcommand_create));
static ::cl::opt<bool> create_build("b", ::cl::desc("Build instead of generate"), ::cl::sub(subcommand_create));
static ::cl::alias create_clear_dir2("c", ::cl::desc("Alias for -clear"), ::cl::aliasopt(create_clear_dir));
static ::cl::opt<bool> create_overwrite_files("overwrite", ::cl::desc("Clear current directory"), ::cl::sub(subcommand_create));
static ::cl::alias create_overwrite_files2("ow", ::cl::desc("Alias for -overwrite"), ::cl::aliasopt(create_overwrite_files));
static ::cl::alias create_overwrite_files3("o", ::cl::desc("Alias for -overwrite"), ::cl::aliasopt(create_overwrite_files));

SUBCOMMAND_DECL(create)
{
    if (create_type == "project")
    {
        if (create_clear_dir)
        {
            String s;
            if (!create_clear_dir_y)
            {
                std::cout << "Going to clear current directory. Are you sure? [Yes/No]\n";
                std::cin >> s;
            }
            if (create_clear_dir_y || boost::iequals(s, "yes") || boost::iequals(s, "Y"))
            {
                for (auto &p : fs::directory_iterator("."))
                    fs::remove_all(p);
            }
            else
            {
                if (fs::directory_iterator(".") != fs::directory_iterator())
                    return;
            }
        }

        if (!create_overwrite_files && fs::directory_iterator(".") != fs::directory_iterator())
            throw SW_RUNTIME_ERROR("directory is not empty");

        String name = fs::current_path().filename().u8string();
        if (!create_proj_name.empty())
            name = create_proj_name;

        // TODO: add separate extended template with configure
        // common sw.cpp
        primitives::CppEmitter ctx;
        ctx.beginFunction("void build(Solution &s)");
        ctx.addLine("// Uncomment to make a project. Also replace s.addTarget(). with p.addTarget() below.");
        ctx.addLine("// auto &p = s.addProject(\"myproject\");");
        ctx.addLine("// p += Git(\"enter your url here\", \"enter tag here\", \"or branch here\");");
        ctx.addLine();
        ctx.addLine("auto &t = s.addTarget<Executable>(\"" + name + "\");");
        ctx.addLine("t += cpp17;");

        String s;
        if (create_language == "cpp")
        {
            if (create_template == "sw")
            {
                s = R"(#include <primitives/sw/main.h>
#include <primitives/sw/settings.h>
#include <primitives/sw/cl.h>

#include <iostream>

int main(int argc, char *argv[])
{
    ::cl::ParseCommandLineOptions(argc, argv);

    std::cout << "Hello, World!\n";
    return 0;
}
)";
            }
            else
            {
                s = R"(#include <iostream>

int main(int argc, char *argv[])
{
    std::cout << "Hello, World!\n";
    return 0;
}
)";
            }
            write_file("src/main.cpp", s);

            ctx.addLine("t += \"src/main.cpp\";");
            if (create_template == "sw")
                ctx.addLine("t += \"pub.egorpugin.primitives.sw.main-master\"_dep;");
            ctx.endFunction();
            write_file("sw.cpp", ctx.getText());

            if (create_build)
                cli_build(swctx);
            else
                cli_generate(swctx);
        }
        else if (create_language == "c")
        {
            s = R"(#include <stdio.h>

int main(int argc, char *argv[])
{
    printf("Hello, World!\n");
    return 0;
}
)";
            write_file("src/main.c", s);

            ctx.addLine("t += \"src/main.c\";");
            ctx.endFunction();
            write_file("sw.cpp", ctx.getText());

            if (create_build)
                cli_build(swctx);
            else
                cli_generate(swctx);
        }
        else
            throw SW_RUNTIME_ERROR("unknown language");
    }
    else if (create_type == "config")
    {
        primitives::CppEmitter ctx;
        ctx.beginFunction("void build(Solution &s)");
        ctx.addLine("// Uncomment to make a project. Also replace s.addTarget(). with p.addTarget() below.");
        ctx.addLine("// auto &p = s.addProject(\"myproject\", \"master\");");
        ctx.addLine("// p += Git(\"https://github.com/account/project\");");
        ctx.addLine();
        ctx.addLine("auto &t = s.addTarget<Executable>(\"project\");");
        ctx.addLine("t += cpp17;");
        ctx.addLine("//t += \"src/main.cpp\";");
        ctx.addLine("//t += \"pub.egorpugin.primitives.sw.main-master\"_dep;");
        ctx.endFunction();
        write_file("sw.cpp", ctx.getText());
    }
    else
        throw SW_RUNTIME_ERROR("Unknown create type");
}

static ::cl::list<String> uri_args(::cl::Positional, ::cl::desc("sw uri arguments"), ::cl::sub(subcommand_uri));
//static ::cl::opt<String> uri_sdir("sw:sdir", ::cl::desc("Open source dir in file browser"), ::cl::sub(subcommand_uri));

SUBCOMMAND_DECL(uri)
{
    if (uri_args.empty())
        return;
    if (uri_args.size() == 1)
        return;

    try
    {
        auto id = extractPackageIdFromString(uri_args[1]);
        auto &sdb = swctx.getLocalStorage();
        LocalPackage p(sdb, id);

        if (uri_args[0] == "sw:sdir" || uri_args[0] == "sw:bdir")
        {
#ifdef _WIN32
            if (sdb.isPackageInstalled(p))
            {
                auto pidl = uri_args[0] == "sw:sdir" ?
                                    ILCreateFromPath(p.getDirSrc2().wstring().c_str()) :
                                    ILCreateFromPath(p.getDirObj().wstring().c_str())
                                    ;
                if (pidl)
                {
                    CoInitialize(0);
                    // ShellExecute does not work here for some scenarios
                    auto r = SHOpenFolderAndSelectItems(pidl, 0, 0, 0);
                    if (FAILED(r))
                    {
                        message_box(sw::getProgramName(), "Error in SHOpenFolderAndSelectItems");
                    }
                    ILFree(pidl);
                }
                else
                {
                    message_box(sw::getProgramName(), "Error in ILCreateFromPath");
                }
            }
            else
            {
                message_box(sw::getProgramName(), "Package '" + p.toString() + "' not installed");
            }
#endif
            return;
        }

        if (uri_args[0] == "sw:open_build_script")
        {
#ifdef _WIN32
            if (sdb.isPackageInstalled(p))
            {
                auto f = (p.getDirSrc2() / "sw.cpp").wstring();

                CoInitialize(0);
                auto r = ShellExecute(0, L"open", f.c_str(), 0, 0, 0);
                if (r <= (HINSTANCE)HINSTANCE_ERROR)
                {
                    message_box(sw::getProgramName(), "Error in ShellExecute");
                }
            }
            else
            {
                message_box(sw::getProgramName(), "Package '" + p.toString() + "' not installed");
            }
#endif
            return;
        }

        if (uri_args[0] == "sw:install")
        {
#ifdef _WIN32
            if (!sdb.isPackageInstalled(p))
            {
                SetupConsole();
                bUseSystemPause = true;
                auto m = swctx.resolveAndInstall(UnresolvedPackages{ UnresolvedPackage{p.ppath, p.version} });
            }
            else
            {
                message_box(sw::getProgramName(), "Package '" + p.toString() + "' is already installed");
            }
#endif
            return;
        }

        if (uri_args[0] == "sw:remove")
        {
            SW_UNIMPLEMENTED;

            //sdb.removeInstalledPackage(p); // TODO: remove from db
            error_code ec;
            fs::remove_all(p.getDir(), ec);
            return;
        }

        if (uri_args[0] == "sw:build")
        {
#ifdef _WIN32
            SetupConsole();
            bUseSystemPause = true;
#endif
            auto d = swctx.getLocalStorage().storage_dir_tmp / "build";// / fs::unique_path();
            fs::create_directories(d);
            ScopedCurrentPath scp(d, CurrentPathScope::All);
            sw::build(swctx, p.toString());
            return;
        }

        if (uri_args[0] == "sw:run")
        {
#ifdef _WIN32
            SetupConsole();
            bUseSystemPause = true;
#endif
            auto d = swctx.getLocalStorage().storage_dir_tmp / "build";// / fs::unique_path();
            fs::create_directories(d);
            ScopedCurrentPath scp(d, CurrentPathScope::All);
            sw::run(swctx, p);
            return;
        }

        if (uri_args[0] == "sw:upload")
        {
            if (uri_args.size() != 4)
                return;

            auto rs = swctx.getRemoteStorages();
            if (rs.empty())
                throw SW_RUNTIME_ERROR("No remote storages found");

            Package pkg(*rs.front(), uri_args[1]);
            Version new_version(uri_args[2]);

            String url = "https://raw.githubusercontent.com/SoftwareNetwork/specifications/master/";
            url += normalize_path(pkg.getHashPath() / "sw.cpp");
            auto fn = get_temp_filename("uploads") / "sw.cpp";
            auto spec_data = download_file(url);
            boost::replace_all(spec_data, pkg.version.toString(), new_version.toString());
            write_file(fn, spec_data);

            // before scp
            SCOPE_EXIT
            {
                // free files
                // TODO:
                /*for (auto &[n,s] : sw::getFileStorages())
                    s.clear();
                sw::getFileStorages().clear();*/

                fs::remove_all(fn.parent_path());
            };

            // run secure as below?
            ScopedCurrentPath scp(fn.parent_path());
            upload_prefix = pkg.ppath.slice(0, std::stoi(uri_args[3]));
            cli_upload(swctx);

            /*primitives::Command c;
            c.program = "sw";
            c.working_directory = fn.parent_path();
            c.args.push_back("upload");
            c.args.push_back(pkg.ppath.slice(0, std::stoi(uri_args[3])));
            c.out.inherit = true;
            c.err.inherit = true;
            //c.execute();*/

            return;
        }

        throw SW_RUNTIME_ERROR("Unknown command: " + uri_args[0]);
    }
    catch (std::exception &e)
    {
#ifdef _WIN32
        message_box(sw::getProgramName(), e.what());
#endif
    }
    catch (...)
    {
#ifdef _WIN32
        message_box(sw::getProgramName(), "Unknown exception");
#endif
    }
}

void override_package_perform(sw::SwContext &swctx)
{
    auto s = sw::load(swctx, ".");
    auto &b = *s.get();
    b.prepareStep();

    auto gn = swctx.getLocalStorage().getOverriddenPackagesStorage().getPackagesDatabase().getMaxGroupNumber() + 1;
    for (auto &[pkg, desc] : b.solutions[0].getPackages())
    {
        sw::PackagePath prefix = override_package;
        sw::PackageId pkg2{ prefix / pkg.ppath, pkg.version };
        auto dir = fs::absolute(".");
        //auto dir = fs::absolute(override_package[1]);
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
        LocalPackage lp(swctx.getLocalStorage().getOverriddenPackagesStorage(), pkg2);
        PackageData d;
        d.sdir = dir;
        d.dependencies = deps;
        d.group_number = gn;
        d.prefix = (int)prefix.size();
        lp.setData(d);
        swctx.getLocalStorage().getOverriddenPackagesStorage().install(lp);
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
    //useFileMonitor = false;

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
    }
}

extern String gGenerator;
::cl::opt<String, true> cl_generator("G", ::cl::desc("Generator"), ::cl::location(gGenerator), ::cl::sub(subcommand_generate));
::cl::alias generator2("g", ::cl::desc("Alias for -G"), ::cl::aliasopt(cl_generator));
extern bool gPrintDependencies;
static ::cl::opt<bool, true> print_dependencies("print-dependencies", ::cl::location(gPrintDependencies), ::cl::sub(subcommand_generate));
// ad = all deps?
::cl::alias print_dependencies4("ad", ::cl::desc("Alias for -print-dependencies"), ::cl::aliasopt(print_dependencies));
::cl::alias print_dependencies2("d", ::cl::desc("Alias for -print-dependencies"), ::cl::aliasopt(print_dependencies));
::cl::alias print_dependencies3("deps", ::cl::desc("Alias for -print-dependencies"), ::cl::aliasopt(print_dependencies));
extern bool gPrintOverriddenDependencies;
static ::cl::opt<bool, true> print_overridden_dependencies("print-overridden-dependencies", ::cl::location(gPrintOverriddenDependencies), ::cl::sub(subcommand_generate));
// o = od?
::cl::alias print_overridden_dependencies4("o", ::cl::desc("Alias for -print-overridden-dependencies"), ::cl::aliasopt(print_overridden_dependencies));
::cl::alias print_overridden_dependencies2("od", ::cl::desc("Alias for -print-overridden-dependencies"), ::cl::aliasopt(print_overridden_dependencies));
::cl::alias print_overridden_dependencies3("odeps", ::cl::desc("Alias for -print-overridden-dependencies"), ::cl::aliasopt(print_overridden_dependencies));
extern bool gOutputNoConfigSubdir;
static ::cl::opt<bool, true> output_no_config_subdir("output-no-config-subdir", ::cl::location(gOutputNoConfigSubdir), ::cl::sub(subcommand_generate));

// generated solution dir instead of .sw/...
//static ::cl::opt<String> generate_binary_dir("B", ::cl::desc("Explicitly specify a build directory."), ::cl::sub(subcommand_build), ::cl::init(SW_BINARY_DIR));

SUBCOMMAND_DECL(generate)
{
    if (gGenerator.empty())
    {
#ifdef _WIN32
        gGenerator = "vs";
#endif
    }
    ((Strings&)build_arg).clear();
    build_arg.push_back(build_arg_generate.getValue());
    cli_build(swctx);
}

SUBCOMMAND_DECL(setup)
{
    elevate();

#ifdef _WIN32
    auto prog = boost::dll::program_location().wstring();

    // set common environment variable
    //winreg::RegKey env(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment");
    //env.SetStringValue(L"SW_TOOL", boost::dll::program_location().wstring());

    // set up protocol handler
    {
        const std::wstring id = L"sw";

        winreg::RegKey url(HKEY_CLASSES_ROOT, id);
        url.SetStringValue(L"URL Protocol", L"");

        winreg::RegKey icon(HKEY_CLASSES_ROOT, id + L"\\DefaultIcon");
        icon.SetStringValue(L"", prog);

        winreg::RegKey open(HKEY_CLASSES_ROOT, id + L"\\shell\\open\\command");
        open.SetStringValue(L"", prog + L" uri %1");
    }

    // register .sw extension
    {
        const std::wstring id = L"sw.1";

        winreg::RegKey ext(HKEY_CLASSES_ROOT, L".sw");
        ext.SetStringValue(L"", id);

        winreg::RegKey icon(HKEY_CLASSES_ROOT, id + L"\\DefaultIcon");
        icon.SetStringValue(L"", prog);

        winreg::RegKey p(HKEY_CLASSES_ROOT, id + L"\\shell\\open\\command");
        p.SetStringValue(L"", prog + L" build %1");
    }
#endif
}

sw::Remote *find_remote(sw::Settings &s, const String &name)
{
    sw::Remote *current_remote = nullptr;
    for (auto &r : s.remotes)
    {
        if (r.name == name)
        {
            current_remote = &r;
            break;
        }
    }
    if (!current_remote)
        throw SW_RUNTIME_ERROR("Remote not found: " + name);
    return current_remote;
}

static ::cl::opt<String> remote_subcommand(::cl::Positional, ::cl::desc("remote subcomand"), ::cl::sub(subcommand_remote), ::cl::Required);
static ::cl::list<String> remote_rest(::cl::desc("other remote args"), ::cl::sub(subcommand_remote), ::cl::ConsumeAfter);

SUBCOMMAND_DECL(remote)
{
    // subcommands: add, alter, rename, remove

    // sw remote add origin url:port
    // sw remote remove origin
    // sw remote rename origin origin2
    // sw remote alter origin add token TOKEN

    if (remote_subcommand == "alter" || remote_subcommand == "change")
    {
        int i = 0;
        if (remote_rest.size() > i + 1)
        {
            auto token = remote_rest[i];
            auto &us = Settings::get_user_settings();
            auto r = find_remote(us, remote_rest[i]);

            i++;
            if (remote_rest.size() > i + 1)
            {
                if (remote_rest[i] == "add")
                {
                    i++;
                    if (remote_rest.size() > i + 1)
                    {
                        if (remote_rest[i] == "token")
                        {
                            i++;
                            if (remote_rest.size() >= i + 2) // publisher + token
                            {
                                sw::Remote::Publisher p;
                                p.name = remote_rest[i];
                                p.token = remote_rest[i+1];
                                r->publishers[p.name] = p;
                                us.save(get_config_filename());
                            }
                            else
                                throw SW_RUNTIME_ERROR("missing publisher or token");
                        }
                        else
                            throw SW_RUNTIME_ERROR("unknown add object: " + remote_rest[i]);
                    }
                    else
                        throw SW_RUNTIME_ERROR("missing add object");
                }
                else
                    throw SW_RUNTIME_ERROR("unknown alter command: " + remote_rest[i]);
            }
            else
                throw SW_RUNTIME_ERROR("missing alter command");
        }
        else
            throw SW_RUNTIME_ERROR("missing remote name");
        return;
    }
}

SUBCOMMAND_DECL(list)
{
    auto rs = swctx.getRemoteStorages();
    if (rs.empty())
        throw SW_RUNTIME_ERROR("No remote storages found");

    static_cast<StorageWithPackagesDatabase&>(*rs.front()).getPackagesDatabase().listPackages(list_arg);
}

SUBCOMMAND_DECL(pack)
{
    // http://www.king-foo.com/2011/11/creating-debianubuntu-deb-packages/
}

extern bool gWithTesting;

SUBCOMMAND_DECL(test)
{
    gWithTesting = true;
    (Strings&)build_arg = (Strings&)build_arg_test;
    cli_build(swctx);
}

SUBCOMMAND_DECL(install)
{
    SW_UNIMPLEMENTED;

    sw::UnresolvedPackages pkgs;
    install_args.push_back(install_arg);
    for (auto &p : install_args)
        pkgs.insert(extractFromString(p));
    auto m = swctx.resolveAndInstall(pkgs);
    for (auto &[p1, d] : m)
    {
        //for (auto &p2 : install_args)
            //if (p1 == p2)
                //d.installed = true;
    }
}

extern ::cl::opt<bool> dry_run;
SUBCOMMAND_DECL(update)
{
    SW_UNIMPLEMENTED;

    dry_run = true;
    ((Strings&)build_arg).clear();
    build_arg.push_back(build_arg_update.getValue());
    cli_build(swctx);
}

SUBCOMMAND_DECL(fetch)
{
    sw::FetchOptions opts;
    //opts.name_prefix = upload_prefix;
    opts.dry_run = !build_after_fetch;
    opts.root_dir = fs::current_path() / SW_BINARY_DIR;
    opts.ignore_existing_dirs = true;
    opts.existing_dirs_age = std::chrono::hours(1);
    //opts.apply_version_to_source = true;
    auto s = sw::fetch_and_load(swctx, ".", opts);
    if (build_after_fetch)
        s->execute();
}

SUBCOMMAND_DECL(upload)
{
    // select remote first
    auto &us = Settings::get_user_settings();
    auto current_remote = &*us.remotes.begin();
    if (!upload_remote.empty())
        current_remote = find_remote(us, upload_remote);

    sw::FetchOptions opts;
    //opts.name_prefix = upload_prefix;
    opts.dry_run = !build_before_upload;
    opts.root_dir = fs::current_path() / SW_BINARY_DIR;
    opts.ignore_existing_dirs = true;
    opts.existing_dirs_age = std::chrono::hours(8);
    //opts.apply_version_to_source = true;
    if (!upload_path_to_root.empty())
        opts.source_dir = fs::relative(fs::current_path(), fs::current_path() / upload_path_to_root);
    auto s = sw::fetch_and_load(swctx, build_arg_update.getValue(), opts);
    if (build_before_upload)
    {
        s->execute();

        // after execution such solution has resolved deps and deps of the deps
        // we must not add them
        throw SW_RUNTIME_ERROR("not uploaded: not implemented");
    }

    sw::Api api(*current_remote);
    api.addVersion(upload_prefix, s->solutions[0].getPackages(), sw::read_config(build_arg_update.getValue()).value());
}

EXPORT_FROM_EXECUTABLE
std::string getVersionString()
{
    std::string s;
    s += ::sw::getProgramName();
    s += " version ";
    s += PACKAGE_VERSION;
    s += "\n";
    s += "assembled " __DATE__ " " __TIME__;
    return s;
}

EXPORT_FROM_EXECUTABLE
std::string getProgramName()
{
    return PACKAGE_NAME_CLEAN;
}

void self_upgrade()
{
#ifdef _WIN32
    path client = "/client/sw-master-windows-client.zip";
#elif __APPLE__
    path client = "/client/sw-master-macos-client.tar.gz";
#else
    path client = "/client/sw-master-linux-client.tar.gz";
#endif

    auto &s = Settings::get_user_settings();

    std::cout << "Downloading checksum file" << "\n";
    auto md5sum = boost::algorithm::trim_copy(download_file(s.remotes[0].url + client.u8string() + ".md5"));

    auto fn = fs::temp_directory_path() / (unique_path() += client.extension());
    std::cout << "Downloading the latest client" << "\n";
    download_file(s.remotes[0].url + client.u8string(), fn, 50_MB);
    if (md5sum != md5(fn))
        throw std::runtime_error("Downloaded bad file (md5 check failed)");

    std::cout << "Unpacking" << "\n";
    auto tmp_dir = fs::temp_directory_path() / "sw.bak";
    unpack_file(fn, tmp_dir);
    fs::remove(fn);

    // self update
    auto program = path(boost::dll::program_location().wstring());
#ifdef _WIN32
    auto exe = (tmp_dir / "sw.exe").wstring();
    auto arg0 = L"\"" + exe + L"\"";
    auto dst = L"\"" + program.wstring() + L"\"";
    std::cout << "Replacing client" << "\n";
    auto cmd_line = arg0 + L" -internal-self-upgrade-copy " + dst;
    STARTUPINFO si = { 0 };
    PROCESS_INFORMATION pi = { 0 };
    if (!CreateProcess(exe.c_str(), &cmd_line[0], 0, 0, 0, 0, 0, 0, &si, &pi))
    {
        throw std::runtime_error("errno = "s + std::to_string(errno) + "\n" +
            "Cannot do a self upgrade. Replace this file with newer SW client manually.");
    }
#else
    auto cppan = tmp_dir / "sw";
    fs::permissions(cppan, fs::perms::owner_all | fs::perms::group_exec | fs::perms::others_exec);
    fs::remove(program);
    fs::copy_file(cppan, program);
    fs::remove(cppan);
#endif
}

void self_upgrade_copy(const path &dst)
{
    int n = 3;
    while (n--)
    {
        std::cout << "Waiting old program to exit...\n";
        std::this_thread::sleep_for(std::chrono::seconds(2));
        try
        {
            fs::copy_file(boost::dll::program_location().wstring(), dst, fs::copy_options::overwrite_existing);
            break;
        }
        catch (std::exception &e)
        {
            std::cerr << "Cannot replace program with new executable: " << e.what() << "\n";
            if (n == 0)
                throw;
            std::cerr << "Retrying... (" << n << ")\n";
        }
    }
    std::cout << "Success!\n";
}

#if _MSC_VER
#if defined(SW_USE_JEMALLOC)
#define JEMALLOC_NO_PRIVATE_NAMESPACE
#include <jemalloc-5.1.0/include/jemalloc/jemalloc.h>
//#include <jemalloc-5.1.0/src/jemalloc_cpp.cpp>
#endif
#endif
