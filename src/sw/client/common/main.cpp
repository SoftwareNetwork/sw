// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#include "main.h"

#include "commands.h"
#include "self_upgrade.h"

#include <sw/builder/jumppad.h>
#include <sw/driver/driver.h>
#include <sw/manager/settings.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string_regex.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/dll.hpp>
#include <boost/regex.hpp>

#include <thread>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "main");

static void print_command_line(const Strings &args)
{
    String cmdline;
    for (auto &a : args)
        cmdline += a + " ";
    LOG_TRACE(logger, "command line:\n" + cmdline);

    auto append_file_unique = [](const auto &fn, String cmd)
    {
        if (!fs::exists(fn))
        {
            write_file(fn, cmd + "\n");
            return;
        }
        boost::trim(cmd);
        auto lines = read_lines(fn);
        Strings lines_out;
        String s;
        for (auto &l : lines)
        {
            if (std::find(lines_out.begin(), lines_out.end(), l) == lines_out.end() && l != cmd)
            {
                lines_out.push_back(l);
                s += l + "\n";
            }
        }
        s += cmd + "\n";
        write_file(fn, s);
    };

    if (sw::Settings::get_user_settings().record_commands)
    {
        auto hfn = ".sw_history";
        append_file_unique(get_home_directory() / hfn, cmdline);
        if (sw::Settings::get_user_settings().record_commands_in_current_dir)
        {
            try
            {
                // do not work on some commands (uri)
                append_file_unique(path(".sw") / hfn, cmdline);
            }
            catch (std::exception &) {}
        }
    }
}

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

StartupData::StartupData(int argc, char **argv)
    : argc(argc), argv(argv)
{
    overview = "SW: Software Network Client\n"
        "\n"
        "  SW is a Universal Package Manager and Build System\n"
        "\n"
        "  Documentation: " SW_DOC_URL "\n"
        ;
}

StartupData::~StartupData()
{
}

Options &StartupData::getOptions()
{
    if (options)
        return *options;
    throw SW_RUNTIME_ERROR("Options was not created");
}

ClOptions &StartupData::getClOptions()
{
    if (cloptions)
        return *cloptions;
    throw SW_RUNTIME_ERROR("ClOptions was not created");
}

int StartupData::run()
{
    // try to do as less as possible before log init
    // TODO: allow console for GUI case?
    setConsoleColorProcessing();

    prepareArgs();

    try
    {
        if (args.size() > 1 && args[1] == sw::builder::getInternalCallBuiltinFunctionName())
            return builtinCall();
    }
    catch (std::exception &e)
    {
        std::cerr << e.what();
        return 1;
    }

    parseArgs();

    createOptions();
    setWorkingDir();
    initLogger();

    try
    {
        if (!version.empty())
            LOG_TRACE(logger, "version:\n" + version);
        print_command_line(args); // after logger; also for builtin call?
        if (after_create_options && after_create_options(*this))
            return exit(0);

        if (!getOptions().self_upgrade_copy.empty())
        {
            self_upgrade_copy(getOptions().self_upgrade_copy);
            return true;
        }
        if (getOptions().self_upgrade)
        {
            setHttpSettings();
            self_upgrade(program_short_name);
            return true;
        }

        setup();
        if (after_setup && after_setup(*this))
            return exit(0);
        if (exit_code)
            return *exit_code;

        sw_main();
        exit_code = 0;
    }
    catch (const std::exception &e)
    {
        exit_code = 1;
        LOG_ERROR(logger, e.what());
    }

    LOG_FLUSH();

    if (!exit_code)
        throw SW_LOGIC_ERROR("Exit code was not set");
    return *exit_code;
}

void StartupData::prepareArgs()
{
    std::vector<std::string> args0(argv + 1, argv + argc);
    args.push_back(argv[0]);
    for (auto &a : args0)
    {
        std::vector<std::string> t;
        //boost::replace_all(a, "%5F", "_");
        boost::split_regex(t, a, boost::regex("%20"));
        args.insert(args.end(), t.begin(), t.end());
    }
}

int StartupData::builtinCall()
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
    exit_code = sw::jumppad_call(args);
    return *exit_code;
}

void StartupData::parseArgs()
{
    // create and register cl options
    cloptions = std::make_unique<ClOptions>();

    String s;
    llvm::raw_string_ostream errs(s);
    if (!::cl::ParseCommandLineOptions(args, overview, &errs))
    {
        // try alias
        if (args.size() > 1)
        {
            auto alias_args = SwClientContext::getAliasArguments(args[1]);
            if (!alias_args.empty())
            {
                Strings args2;
                args2.push_back(args[0]);
                args2.insert(args2.end(), alias_args.begin(), alias_args.end());
                args2.insert(args2.end(), args.begin() + 2, args.end()); // add rest of args

                // reset cl options
                cloptions.reset(); // reset first!!!
                cloptions = std::make_unique<ClOptions>();

                // reset stream
                errs.flush();
                s.clear();
                if (::cl::ParseCommandLineOptions(args2, overview, &errs))
                    return;
            }
        }

        errs.flush();
        boost::trim(s);
        // no need to throw SW_RUNTIME_ERROR with file and line info
        throw std::runtime_error(s);
    }
}

void StartupData::createOptions()
{
    // create main options!
    options = std::make_unique<Options>(getClOptions());
}

void StartupData::setHttpSettings()
{
    ::setHttpSettings(getOptions());
}

void StartupData::initLogger()
{
    if (getOptions().trace)
        setupLogger("TRACE", getOptions());// , false); // add modules for trace logger
    else if (getOptions().verbose)
        setupLogger("DEBUG", getOptions());
    else
        setupLogger("INFO", getOptions());
}

void StartupData::setWorkingDir()
{
    if (getOptions().working_directory.empty())
        return;

    getOptions().working_directory = primitives::filesystem::canonical(getOptions().working_directory);
    if (fs::is_regular_file(getOptions().working_directory))
        fs::current_path(getOptions().working_directory.parent_path());
    else
        fs::current_path(getOptions().working_directory);

#ifdef _WIN32
    //sw_append_symbol_path(fs::current_path());
#endif
}

void StartupData::setup()
{
    if (getClOptions().parse_configure_ac.getNumOccurrences())
    {
        if (getClOptions().parse_configure_ac.empty())
            getClOptions().parse_configure_ac = "configure.ac";
        sw::driver::cpp::Driver::processConfigureAc(getClOptions().parse_configure_ac);
        exit_code = 0;
        return;
    }

    if (!getOptions().internal_sign_file.empty())
    {
        SW_UNIMPLEMENTED;
        //ds_sign_file(internal_sign_file[0], internal_sign_file[1]);
        exit_code = 0;
        return;
    }

    if (!getOptions().internal_verify_file.empty())
    {
        SW_UNIMPLEMENTED;
        //ds_verify_file(internal_verify_file[0], internal_verify_file[1], internal_verify_file[2]);
        exit_code = 0;
        return;
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

    if (!getOptions().options_build.ide_fast_path.empty() && fs::exists(getOptions().options_build.ide_fast_path))
    {
        auto files = read_lines(getOptions().options_build.ide_fast_path);
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
            path fmtime = getOptions().options_build.ide_fast_path;
            fmtime += ".t";
            if (fs::exists(fmtime) && mtime == std::stoull(read_file(fmtime)))
            {
                exit_code = 0;
                return;
            }
            write_file(fmtime, std::to_string(mtime));
        }
    }
}

void StartupData::sw_main()
{
    SwClientContext swctx(getOptions());

    // graceful exit handler
    boost::asio::io_context io_context;
    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&swctx](
        const std::error_code &error,
        int signal_number)
    {
        if (error)
            return;
        if (swctx.hasContext())
            swctx.getContext().stop();
    });
    std::thread t([&io_context] { try { io_context.run(); } catch (...) {} });
    SCOPE_EXIT
    {
        io_context.stop();
        t.join();
    };

    // for cli we set default input to '.' dir
    if (swctx.getInputs().empty() && getOptions().input_settings_pairs.empty())
        swctx.getInputs().push_back(".");
    //

    if (getOptions().list_predefined_targets)
    {
        LOG_INFO(logger, swctx.listPredefinedTargets());
        exit_code = 0;
        return;
    }

    if (getOptions().list_programs)
    {
        LOG_INFO(logger, swctx.listPrograms());
        exit_code = 0;
        return;
    }

    if (0);
#define SUBCOMMAND(n) else if (getClOptions().subcommand_##n) { swctx.command_##n(); return; }
#include <sw/client/common/commands.inl>
#undef SUBCOMMAND

    LOG_WARN(logger, "No command was issued");
}

int StartupData::exit(int r)
{
    exit_code = r;
    return r;
}
