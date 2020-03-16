/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
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

#include <sw/builder/jumppad.h>
#include <sw/client/common/commands.h>
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
#include <primitives/git_rev.h>

#include <iostream>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "main");

#ifdef _WIN32
#include <primitives/win32helpers.h>
#include <combaseapi.h>
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

void setup_log(const std::string &log_level, const Options &, bool simple = true);
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

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

static bool setConsoleColorProcessing()
{
    bool r = false;
#ifdef _WIN32
    DWORD mode;
    // Try enabling ANSI escape sequence support on Windows 10 terminals.
    auto console_ = GetStdHandle(STD_OUTPUT_HANDLE);
    if (GetConsoleMode(console_, &mode))
        r |= !!SetConsoleMode(console_, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    console_ = GetStdHandle(STD_ERROR_HANDLE);
    if (GetConsoleMode(console_, &mode))
        r &= !!SetConsoleMode(console_, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
    return r;
}

int sw_main(const Strings &args, Options &options)
{
    SwClientContext swctx(options);

    if (options.list_predefined_targets)
    {
        LOG_INFO(logger, swctx.listPredefinedTargets());
        return 0;
    }

    if (options.list_programs)
    {
        LOG_INFO(logger, swctx.listPrograms());
        return 0;
    }

    if (0);
#define SUBCOMMAND(n) else if (subcommand_##n) { swctx.command_##n(); return 0; }
#include <sw/client/common/commands.inl>
#undef SUBCOMMAND

    LOG_WARN(logger, "No command was issued");

    return 0;
}

int setup_main(const Strings &args, Options &options)
{
    // some initial stuff
    // try to do as less as possible before log init

    setConsoleColorProcessing();

    if (!options.working_directory.empty())
    {
        options.working_directory = primitives::filesystem::canonical(options.working_directory);
        if (fs::is_regular_file(options.working_directory))
            fs::current_path(options.working_directory.parent_path());
        else
            fs::current_path(options.working_directory);

#ifdef _WIN32
        sw_append_symbol_path(fs::current_path());
#endif
    }

    if (options.trace)
        setup_log("TRACE", options);// , false); // add modules for trace logger
    else if (gVerbose)
        setup_log("DEBUG", options);
    else
        setup_log("INFO", options);

    {
        String cmdline;
        for (auto &a : args)
            cmdline += a + " ";
        LOG_TRACE(logger, "command line:\n" + cmdline);

        if (Settings::get_user_settings().record_commands)
        {
            auto hfn = ".sw_history";
            append_file(get_home_directory() / hfn, cmdline + "\n");
            if (Settings::get_user_settings().record_commands_in_current_dir)
            {
                try
                {
                    // do not work on some commands (uri)
                    append_file(path(".sw") / hfn, cmdline + "\n");
                }
                catch (std::exception &) {}
            }
        }
    }

    // after log initialized

    if (!options.self_upgrade_copy.empty())
    {
        self_upgrade_copy(options.self_upgrade_copy);
        return 0;
    }

    if (options.self_upgrade)
    {
        self_upgrade();
        return 0;
    }

    if (cl_parse_configure_ac.getNumOccurrences())
    {
        if (cl_parse_configure_ac.empty())
            cl_parse_configure_ac = "configure.ac";
        void process_configure_ac2(const path &p);
        process_configure_ac2(cl_parse_configure_ac);
        return 0;
    }

    if (!options.internal_sign_file.empty())
    {
        SW_UNIMPLEMENTED;
        //ds_sign_file(internal_sign_file[0], internal_sign_file[1]);
        return 0;
    }

    if (!options.internal_verify_file.empty())
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

    if (!options.options_build.ide_fast_path.empty() && fs::exists(options.options_build.ide_fast_path))
    {
        auto files = read_lines(options.options_build.ide_fast_path);
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
            path fmtime = options.options_build.ide_fast_path;
            fmtime += ".t";
            if (fs::exists(fmtime) && mtime == std::stoull(read_file(fmtime)))
                return 0;
            write_file(fmtime, std::to_string(mtime));
        }
    }

    // actual execution
    return sw_main(args, options);
}

int parse_main(int argc, char **argv)
{
    //args::ValueFlag<int> configuration(parser, "configuration", "Configuration to build", { 'c' });

    String overview = "SW: Software Network Client\n"
        "\n"
        "  SW is a Universal Package Manager and Build System\n"
        "\n"
        "  Documentation: " SW_DOC_URL "\n"
        ;

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

    // create main options!
    Options options;
    // set http settings very early
    // needed for self-upgrade feature
    setHttpSettings(options);
    return setup_main(args, options);
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

void setup_log(const std::string &log_level, const Options &options, bool simple)
{
    LoggerSettings log_settings;
    log_settings.log_level = log_level;
    if (options.write_log_to_file && bConsoleMode)
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

EXPORT_FROM_EXECUTABLE
std::string getVersionString()
{
    std::string s;
    s += ::sw::getProgramName();
    s += " version ";
    s += PACKAGE_VERSION;
    s += "\n";
    s += primitives::git_rev::getGitRevision();
    s += "\n";
    s += primitives::git_rev::getBuildTime();
    return s;
}

EXPORT_FROM_EXECUTABLE
std::string getProgramName()
{
    return PACKAGE_NAME_CLEAN;
}
