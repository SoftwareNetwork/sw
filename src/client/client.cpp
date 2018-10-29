// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <command.h>
#include <database.h>
#include <directories.h>
#include <exceptions.h>
#include <file.h>
#include <file_storage.h>
#include <resolver.h>
#include <settings.h>

#include <sw/builder/build.h>
#include <sw/builder/driver.h>
#include <sw/driver/cpp/driver.h>
#include <sw/driver/cppan/driver.h>
#include <jumppad.h>

//#include <args.hxx>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string_regex.hpp>
#include <boost/dll.hpp>
//#include <boost/nowide/args.hpp>
#include <boost/regex.hpp>
#include <primitives/executor.h>
#include <primitives/lock.h>
#include <primitives/sw/settings.h>
#include <primitives/sw/main.h>
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

using namespace sw;

bool bConsoleMode = true;
bool bUseSystemPause = false;

namespace sw::driver::cpp { SW_REGISTER_PACKAGE_DRIVER(CppDriver); }
namespace sw::driver::cppan { SW_REGISTER_PACKAGE_DRIVER(CppanDriver); }

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
void setup_log(const std::string &log_level);

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
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
}
#endif

static cl::opt<path> working_directory("d", cl::desc("Working directory"));
static cl::opt<bool> verbose("verbose", cl::desc("Verbose output"));
static cl::opt<bool> trace("trace", cl::desc("Trace output"));
static cl::opt<int> jobs("j", cl::desc("Number of jobs"), cl::init(-1));

int setup_main(const Strings &args)
{
    // some initial stuff

    if (!working_directory.empty())
        fs::current_path(working_directory);

    if (jobs > 0)
        getExecutor(jobs);

#ifdef NDEBUG
    setup_log("INFO");
#else
    setup_log("DEBUG");
#endif

    if (verbose)
        setup_log("DEBUG");
    if (trace)
        setup_log("TRACE");

    getServiceDatabase();

    // actual execution
    return sw_main(args);
}

int parse_main(int argc, char **argv)
{
    //args::ValueFlag<int> configuration(parser, "configuration", "Configuration to build", { 'c' });

    String overview = "SW: Software Network Client\n\n"
        "  SW is a Universal Package Manager and Build System...\n";
    if (auto &driver = getDrivers(); !driver.empty())
    {
        overview += "\n  Available drivers:\n";
        for (auto &d : driver)
            overview += "    - " + d->getName() + "\n";
    }

    const std::vector<std::string> args0(argv + 1, argv + argc);
    Strings args;
    args.push_back(argv[0]);
    for (auto &a : args0)
    {
        std::vector<std::string> t;
        boost::split_regex(t, a, boost::regex("%20"));
        args.insert(args.end(), t.begin(), t.end());
    }

    if (args.size() > 1 && args[1] == "internal-call-builtin-function")
    {
        return jumppad_call(args);
    }

    //
    cl::ParseCommandLineOptions(args, overview);

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
            if (IsDebuggerPresent())
                system("pause");
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
                message_box(error);
#endif
        }
    }

    LOG_FLUSH();

    return r;
}

#define SUBCOMMAND_DECL(n) void cli_##n()
#define SUBCOMMAND(n, d) SUBCOMMAND_DECL(n);
#include <commands.inl>
#undef SUBCOMMAND

#define SUBCOMMAND(n, d) cl::SubCommand subcommand_##n(#n, d);
#include <commands.inl>
#undef SUBCOMMAND

//
//static cl::list<path> build_arg0(cl::Positional, cl::desc("Files or directoris to build"));

// build commands
// must be opt<String>!
static cl::opt<String> build_arg(cl::Positional, cl::desc("File or directory to build"), cl::init("."), cl::sub(subcommand_build));

// ide commands
static cl::opt<String> target_build("target", cl::desc("Target to build")/*, cl::sub(subcommand_ide)*/);
static cl::opt<String> ide_rebuild("rebuild", cl::desc("Rebuild target"), cl::sub(subcommand_ide));
static cl::opt<String> ide_clean("clean", cl::desc("Clean target"), cl::sub(subcommand_ide));

static cl::list<String> override_package("override-remote-package", cl::value_desc("prefix sdir"), cl::desc("Provide a local copy of remote package"), cl::multi_val(2));
static cl::opt<String> delete_overridden_package("delete-overridden-remote-package", cl::value_desc("package"), cl::desc("Delete overridden package from index"));
static cl::opt<path> delete_overridden_package_dir("delete-overridden-remote-package-dir", cl::value_desc("sdir"), cl::desc("Delete overridden dir packages"));

//static cl::list<String> builtin_function("internal-call-builtin-function", cl::desc("Call built-in function"), cl::Hidden);

int sw_main(const Strings &args)
{
    if (!override_package.empty())
    {
        auto s = sw::load(override_package[1]);
        for (auto &[pkg, _] : s->getPackages())
        {
            sw::PackageId pkg2{ sw::PackagePath(override_package[0]) / pkg.ppath, pkg.version };
            auto dir = fs::absolute(override_package[1]);
            LOG_INFO(logger, "Overriding " + pkg2.toString() + " to " + dir.u8string());
            getServiceDatabase().overridePackage(pkg2, dir);
        }
        return 0;
    }

    if (!delete_overridden_package.empty())
    {
        sw::PackageId pkg{ delete_overridden_package };
        LOG_INFO(logger, "Delete override for " + pkg.toString());
        getServiceDatabase().deleteOverriddenPackage(pkg);
        return 0;
    }

    if (!delete_overridden_package_dir.empty())
    {
        LOG_INFO(logger, "Delete override for sdir " + delete_overridden_package_dir.u8string());
        getServiceDatabase().deleteOverriddenPackageDir(delete_overridden_package_dir);
        return 0;
    }

    /*if (!build_arg0.empty())
    {
        sw::build(Files{ build_arg0.begin(), build_arg0.end() });
        return 0;
    }*/

    if (0);
#define SUBCOMMAND(n, d) else if (subcommand_##n) cli_##n();
#include <commands.inl>
#undef SUBCOMMAND

    return 0;
}

void stop()
{
    getExecutor().join();
    getFileStorages().clear();
}

static cl::opt<bool> write_log_to_file("log-to-file");

void setup_log(const std::string &log_level)
{
    LoggerSettings log_settings;
    log_settings.log_level = log_level;
    if (write_log_to_file && bConsoleMode)
        log_settings.log_file = (get_root_directory() / "sw").string();
    log_settings.simple_logger = true;
    log_settings.print_trace = true;
    initLogger(log_settings);

    // first trace message
    LOG_TRACE(logger, "----------------------------------------");
    LOG_TRACE(logger, "Starting sw...");
}

#define SUBCOMMAND_DECL_URI(c) SUBCOMMAND_DECL(uri_ ## c)

SUBCOMMAND_DECL(build)
{
    sw::build(build_arg);
}

static cl::list<String> uri_args(cl::Positional, cl::desc("sw uri arguments"), cl::sub(subcommand_uri));
//static cl::opt<String> uri_sdir("sw:sdir", cl::desc("Open source dir in file browser"), cl::sub(subcommand_uri));

SUBCOMMAND_DECL(uri)
{
    if (uri_args.empty())
        return;
    if (uri_args.size() == 1)
        return;

    try
    {
        auto p = extractFromStringPackageId(uri_args[1]);
        auto &sdb = getServiceDatabase();

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
                    auto r = SHOpenFolderAndSelectItems(pidl, 0, 0, 0);
                    if (FAILED(r))
                    {
                        message_box("Error in SHOpenFolderAndSelectItems");
                    }
                    ILFree(pidl);
                }
            }
            else
            {
                message_box("Package '" + p.target_name + "' not installed");
            }
#endif
        }

        if (uri_args[0] == "sw:install")
        {
#ifdef _WIN32
            if (!sdb.isPackageInstalled(p))
            {
                SetupConsole();
                bUseSystemPause = true;
                Resolver r;
                r.resolve_dependencies({ {p.ppath, p.version} });
            }
            else
            {
                message_box("Package '" + p.target_name + "' is already installed");
            }
#endif
        }

        if (uri_args[0] == "sw:remove")
        {
            sdb.removeInstalledPackage(p);
            error_code ec;
            fs::remove_all(p.getDir(), ec);
        }

        if (uri_args[0] == "sw:build")
        {
#ifdef _WIN32
            SetupConsole();
            bUseSystemPause = true;
#endif
            auto d = getUserDirectories().storage_dir_tmp / "build";// / fs::unique_path();
            fs::create_directories(d);
            ScopedCurrentPath scp(d, CurrentPathScope::All);
            sw::build(p);
        }

        if (uri_args[0] == "sw:run")
        {
#ifdef _WIN32
            SetupConsole();
            bUseSystemPause = true;
#endif
            auto d = getUserDirectories().storage_dir_tmp / "build";// / fs::unique_path();
            fs::create_directories(d);
            ScopedCurrentPath scp(d, CurrentPathScope::All);
            sw::run(p);
        }
    }
    catch (std::exception &e)
    {
#ifdef _WIN32
        message_box(e.what());
#endif
    }
    catch (...)
    {
#ifdef _WIN32
        message_box("Unknown exception");
#endif
    }
}

#include <solution.h>

SUBCOMMAND_DECL(ide)
{
    if (!target_build.empty())
    {
        try_single_process_job(fs::current_path(), []()
        {
            auto s = sw::load("sw.cpp");
            auto &b = *((sw::Build*)s.get());
            b.ide = true;
            auto pkg = sw::extractFromStringPackageId(target_build);
            b.TargetsToBuild[pkg] = b.children[pkg];
            s->execute();
        });
    }
    else
    {
        single_process_job(fs::current_path(), []()
        {
            auto s = sw::load("sw.cpp");
            auto &b = *((sw::Build*)s.get());
            b.ide = true;
            s->execute();
        });
    }
}

SUBCOMMAND_DECL(init)
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
