/*
 * Copyright (C) 2016-2017, Egor Pugin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <iostream>
#include <thread>
#include <string>

#include <boost/algorithm/string.hpp>

#include <access_table.h>
#include <api.h>
#include <config.h>
#include <database.h>
#include <filesystem.h>
#include <http.h>
#include <logger.h>
#include <pack.h>
#include <printers/cmake.h>
#include <program.h>
#include <settings.h>
#include <shell_link.h>
#include <verifier.h>

#include "build.h"
#include "fix_imports.h"
#include "options.h"
#include "autotools.h"

enum class ApiResult
{
    Handled,
    NotHandled,
    Error,
};

ApiResult api_call(const String &cmd, const Strings &args);
void check_spec_file();
void default_run();
void init(const Strings &args, const String &log_level);
void init_service_db();
void load_current_config();
void self_upgrade();

int main(int argc, char *argv[])
try
{
    Strings args;
    for (auto i = 0; i < argc; i++)
        args.push_back(argv[i]);

    String log_level = "info";

    // set correct working directory to look for config file
    std::unique_ptr<ScopedCurrentPath> cp;

    // do manual checks of critical arguments
    {
        Strings args_copy = args;
        for (size_t i = 1; i < args.size(); i++)
        {
            // working dir
            if (args[i] == "-d"s || args[i] == "--dir"s)
            {
                if (i + 1 < args.size())
                    cp = std::make_unique<ScopedCurrentPath>(args[i + 1]);
                else
                    throw std::runtime_error("Missing necessary argument for "s + args[i] + " option");
                args_copy.erase(args_copy.begin() + i, args_copy.begin() + i + 2);
            }

            // verbosity
            if (args[i] == "-v"s || args[i] == "--verbose"s)
            {
                log_level = "debug";
                args_copy.erase(args_copy.begin() + i, args_copy.begin() + i + 1);
            }
            if (args[i] == "--trace"s)
            {
                log_level = "trace";
                args_copy.erase(args_copy.begin() + i, args_copy.begin() + i + 1);
            }

            // additional build args
            if (args[i] == "--"s)
            {
                auto &s = Settings::get_user_settings();
                s.additional_build_args.assign(args_copy.begin() + i + 1, args_copy.end());
                args_copy.erase(args_copy.begin() + i, args_copy.end());
            }
        }
        args = args_copy;
    }

    // check if single arg is dir name
    if (args.size() == 2)
    {
        auto cmd = args[1];

        // file/url arg
        if (!isUrl(cmd) && fs::is_directory(cmd))
        {
            cp = std::make_unique<ScopedCurrentPath>(cmd);
            args.erase(args.end() - 1);
        }
    }

    // main cppan client init routine
    init(args, log_level);

    // default run
    if (args.size() == 1)
    {
        default_run();
        return 0;
    }

    if (args.size() > 1)
    {
        auto self_upgrade = [&args]()
        {
            // self upgrade via copy
            std::this_thread::sleep_for(std::chrono::seconds(1));
            fs::copy_file(args[0], args[2], fs::copy_option::overwrite_if_exists);
        };

        // command selector, always exit inside this if()
        if (args[1][0] != '-')
        {
            String cmd = args[1];

            // internal
            if (cmd.find("internal-") == 0)
            {
                if (cmd == "internal-fix-imports")
                {
                    if (args.size() != 6)
                    {
                        std::cout << "invalid number of arguments\n";
                        std::cout << "usage: cppan internal-fix-imports target aliases.file old.file new.file\n";
                        return 1;
                    }
                    fix_imports(args[2], args[3], args[4], args[5]);
                    return 0;
                }

                if (cmd == "internal-parallel-vars-check")
                {
                    if (args.size() < 6)
                    {
                        std::cout << "invalid number of arguments: " << args.size() << "\n";
                        std::cout << "usage: cppan internal-parallel-vars-check vars_dir vars_file checks_file generator toolset toolchain\n";
                        return 1;
                    }

                    size_t a = 2;

#define ASSIGN_ARG(x) if (a < args.size()) o.x = trim_double_quotes(args[a++])

                    ParallelCheckOptions o;
                    ASSIGN_ARG(dir);
                    ASSIGN_ARG(vars_file);
                    ASSIGN_ARG(checks_file);
                    ASSIGN_ARG(generator);
                    ASSIGN_ARG(toolset);
                    ASSIGN_ARG(toolchain);

#undef ASSIGN_ARG

                    CMakePrinter c;
                    c.parallel_vars_check(o);
                    return 0;
                }

                if (cmd == "internal-create-link-to-solution")
                {
#ifndef _WIN32
                    return 0;
#endif
                    if (args.size() != 4)
                    {
                        std::cout << "invalid number of arguments: " << args.size() << "\n";
                        std::cout << "usage: cppan internal-create-link-to-solution solution.sln link.lnk\n";
                        return 1;
                    }
                    if (!create_link(args[2], args[3], "Link to CPPAN Solution"))
                        return 1;
                    return 0;
                }

                if (args[1] == "internal-self-upgrade-copy")
                {
                    self_upgrade();
                    return 0;
                }

                return 0;
            }

            // normal options
            if (cmd == "parse-configure-ac")
            {
                if (args.size() != 3)
                {
                    std::cout << "invalid number of arguments\n";
                    std::cout << "usage: cppan parse-configure-ac configure.ac\n";
                    return 1;
                }
                process_configure_ac(args[2]);
                return 0;
            }

            if (cmd == "list")
            {
                auto &db = getPackagesDatabase();
                db.listPackages(args.size() > 2 ? args[2] : "");
                return 0;
            }

            // api
            switch (api_call(cmd, args))
            {
            case ApiResult::Handled:
                return 0;
            case ApiResult::Error:
                return 1;
            }

            // file/url arg
            if (isUrl(cmd))
                return build(cmd);
            if (fs::exists(cmd))
            {
                if (fs::is_directory(cmd))
                {
                    ScopedCurrentPath cp(cmd);
                    default_run();
                    return 0;
                }
                if (fs::is_regular_file(cmd))
                    return build(cmd);
            }

            // maybe we entered a package?
            try
            {
                build_package(cmd);
                return 0;
            }
            catch (const std::exception &)
            {
            }

            std::cout << "unknown command\n";
            return 1;
    }
#ifdef _WIN32
        else if (args[1] == "--self-upgrade-copy") // remove this very very later (at 0.3.0 - 0.5.0)
        {
            self_upgrade();
            return 0;
        }
#endif
    }

    // pay attention to the priority of arguments

    ProgramOptions options;
    bool r = options.parseArgs(args);

    httpSettings.verbose = options["curl-verbose"].as<bool>();
    httpSettings.ignore_ssl_checks = options["ignore-ssl-checks"].as<bool>();
    httpSettings.proxy = Settings::get_local_settings().proxy;

    // always first
    if (!r || options().count("help"))
    {
        std::cout << options.printHelp() << "\n";
        return !r;
    }
    if (options["version"].as<bool>())
    {
        std::cout << get_program_version_string("cppan") << "\n";
        return 0;
    }

    // self-upgrade?
    if (options()["self-upgrade"].as<bool>())
    {
        self_upgrade();
        return 0;
    }

    if (options["clear-cache"].as<bool>())
    {
        CMakePrinter c;
        // TODO: provide better way of opening passed storage in args[2]
        c.clear_cache();
        return 0;
    }
    if (options["clear-vars-cache"].as<bool>())
    {
        Config c;
        // TODO: provide better way of opening passed storage in args[2]
        c.clear_vars_cache();
        return 0;
    }
    if (options().count(CLEAN_PACKAGES))
    {
        auto fs = CleanTarget::getStrings();
        int flags = 0;
        auto opts = options[CLEAN_PACKAGES].as<Strings>();
        for (auto &o : opts)
        {
            auto i = fs.find(o);
            if (i != fs.end())
                flags |= i->second;
        }
        if (flags == 0)
            flags = CleanTarget::All;
        for (auto &o : opts)
        {
            auto i = fs.find(o);
            if (i == fs.end())
                cleanPackages(o, flags);
        }
        return 0;
    }
    if (options().count(CLEAN_CONFIGS))
    {
        cleanConfigs(options[CLEAN_CONFIGS].as<Strings>());
        return 0;
    }
    if (options().count("beautify"))
    {
        path p = options["beautify"].as<String>();
        auto y = load_yaml_config(p);
        dump_yaml_config(p, y);
        return 0;
    }

    Settings::get_user_settings().force_server_query = options()[SERVER_QUERY].as<bool>();

    if (options().count("verify"))
    {
        verify(options["verify"].as<String>());
        LOG_INFO(logger, "Verified...  Ok. Packages are the same.");
        return 0;
    }

    if (options().count("build"))
    {
        return build(options["build"].as<String>(), options["config"].as<String>());
    }
    if (options().count("build-only"))
    {
        return build_only(options["build-only"].as<String>(), options["config"].as<String>());
    }
    if (options().count(BUILD_PACKAGES))
    {
        auto pkgs = options[BUILD_PACKAGES].as<Strings>();
        for (auto &pkg : pkgs)
        {
            auto r = build_package(pkg, options["settings"].as<String>(), options["config"].as<String>());
            if (r)
                return r;
        }
        return 0;
    }

    if (options()["prepare-archive"].as<bool>())
    {
        Config c;
        c.load_current_config();
        Projects &projects = c.getProjects();
        for (auto &p : projects)
        {
            auto &project = p.second;
            project.findSources();
            String archive_name = make_archive_name(project.pkg.ppath.toString());
            if (!project.writeArchive(fs::absolute(archive_name)))
                throw std::runtime_error("Archive write failed");
        }
        return 0;
    }

    default_run();

    return 0;
}
catch (const std::exception &e)
{
    std::cerr << e.what() << "\n";
    return 1;
}
catch (...)
{
    std::cerr << "Unhandled unknown exception" << "\n";
    return 1;
}

void check_spec_file()
{
    // no config - cannot do anything more
    if (!fs::exists(CPPAN_FILENAME))
        throw std::runtime_error("No spec file found");
}

void default_run()
{
    check_spec_file();

    Config c;
    auto &deps = Settings::get_local_settings().dependencies;
    if (deps.empty())
    {
        c.load_current_config();

        // if we have several projects, gather deps in a new config
        const auto &projects = c.getProjects();
        if (projects.size() > 1)
        {
            Config c2;
            for (auto &p : projects)
            {
                c2.getDefaultProject().dependencies.insert(
                    p.second.dependencies.begin(), p.second.dependencies.end());
            }
            c = c2;
        }
    }
    else
        c.getDefaultProject().dependencies = deps;
    c.process();
}

void init(const Strings &args, const String &log_level)
{
    // initial sequence
    initLogger(log_level, "", true);

    // initialize CPPAN structures (settings), do not remove
    auto &us = Settings::get_user_settings();

    // disable update checks for internal commands
    if (args.size() > 1 && args[1].find("internal-") == 0)
        us.disable_update_checks = true;

    load_current_config();
    init_service_db();
}

void init_service_db()
{
    // initialize internal db
    auto &sdb = getServiceDatabase();
    sdb.performStartupActions();
}

void load_current_config()
{
    try
    {
        // load local settings for storage dir
        Config().load_current_config();
    }
    catch (...)
    {
        // ignore everything
    }
}

void self_upgrade()
{
#ifdef _WIN32
    String client = "/client/cppan-master-Windows-client.zip";
#elif __APPLE__
    String client = "/client/cppan-master-macOS-client.zip";
#else
    String client = "/client/.service/cppan-master-Linux-client.zip";
#endif

    auto &s = Settings::get_user_settings();

    DownloadData dd;
    dd.url = s.remotes[0].url + client + ".md5";
    dd.fn = fs::temp_directory_path() / fs::unique_path();
    std::cout << "Downloading checksum file" << "\n";
    download_file(dd);
    auto md5 = boost::algorithm::trim_copy(read_file(dd.fn));

    dd.url = s.remotes[0].url + client;
    dd.fn = fs::temp_directory_path() / fs::unique_path();
    String dl_md5;
    dd.md5.hash = &dl_md5;
    std::cout << "Downloading the latest client" << "\n";
    download_file(dd);
    if (md5 != dl_md5)
        throw std::runtime_error("Downloaded bad file (md5 check failed)");

    std::cout << "Unpacking" << "\n";
    auto tmp_dir = fs::temp_directory_path() / "cppan.bak";
    unpack_file(dd.fn, tmp_dir);

    // self update
    auto program = get_program();
#ifdef _WIN32
    auto exe = (tmp_dir / "cppan.exe").wstring();
    auto arg0 = L"\"" + exe + L"\"";
    auto dst = L"\"" + program.wstring() + L"\"";
    std::cout << "Replacing client" << "\n";
    if (_wexecl(exe.c_str(), arg0.c_str(), L"internal-self-upgrade-copy", dst.c_str(), 0) == -1)
    {
        throw std::runtime_error("errno = "s + std::to_string(errno) + "\n" +
            "Cannot do a self upgrade. Replace this file with newer CPPAN client manually.");
    }
#else
    auto cppan = tmp_dir / "cppan";
    fs::permissions(cppan, fs::owner_all | fs::group_exe | fs::others_exe);
    fs::remove(program);
    fs::copy_file(cppan, program);
    fs::remove(cppan);
#endif
}

ApiResult api_call(const String &cmd, const Strings &args)
{
    auto us = Settings::get_user_settings();

    auto get_remote = [&us](const String &remote)
    {
        auto i = std::find_if(us.remotes.begin(), us.remotes.end(),
            [&remote](auto &v) { return v.name == remote; });
        return i;
    };
    auto find_remote = [&get_remote, &us](const String &remote)
    {
        auto i = get_remote(remote);
        if (i == us.remotes.end())
            throw std::runtime_error("unknown remote: " + remote);
        return *i;
    };
    auto has_remote = [&get_remote, &us](const String &remote)
    {
        return get_remote(remote) != us.remotes.end();
    };

    if (cmd == "add" || cmd == "create")
    {
        if (args.size() < 3)
        {
            std::cout << "invalid number of arguments\n";
            std::cout << "usage: cppan add project|version [remote] name ...\n";
            return ApiResult::Error;
        }

        size_t arg = 2;
        String what = args[arg++];
        if (what == "project" || what == "package")
        {
            auto proj_usage = []
            {
                std::cout << "invalid number of arguments\n";
                std::cout << "usage: cppan add project [remote] name [type]\n";
            };

            if (args.size() < arg + 1)
            {
                proj_usage();
                return ApiResult::Error;
            }

            String remote = DEFAULT_REMOTE_NAME;
            ProjectPath p(args[arg++]);
            if (has_remote(remote) && p.is_relative() && p.size() == 1)
            {
                remote = args[arg - 1];

                if (args.size() < arg + 1)
                {
                    proj_usage();
                    return ApiResult::Error;
                }

                p = ProjectPath(args[arg++]);
            }

            // type
            ProjectType type = ProjectType::Library;
            if (args.size() > arg)
            {
                String t = args[arg++];
                if (t == "l" || t == "lib" || t == "library")
                    type = ProjectType::Library;
                else if (t == "e" || t == "exe" || t == "executable")
                    type = ProjectType::Executable;
                else if (t == "r" || t == "root" || t == "root_project")
                    type = ProjectType::RootProject;
                else if (t == "d" || t == "dir" || t == "directory")
                    type = ProjectType::Directory;
            }

            Api().add_project(find_remote(remote), p, type);
            return ApiResult::Handled;
        }

        if (what == "version")
        {
            auto proj_usage = []
            {
                std::cout << "invalid number of arguments\n";
                std::cout << "usage: cppan add version [remote] name cppan.yml\n";
            };

            if (args.size() < arg + 1)
            {
                proj_usage();
                return ApiResult::Error;
            }

            String remote = DEFAULT_REMOTE_NAME;
            ProjectPath p(args[arg++]);
            if (has_remote(remote) && p.is_relative() && p.size() == 1)
            {
                remote = args[arg - 1];

                if (args.size() < arg + 1)
                {
                    proj_usage();
                    return ApiResult::Error;
                }

                p = ProjectPath(args[arg++]);
            }

            if (args.size() < arg + 1)
            {
                proj_usage();
                return ApiResult::Error;
            }

            Api().add_version(find_remote(remote), p, read_file(args[arg++]));
            return ApiResult::Handled;
        }

        return ApiResult::Handled;
    }

    if (cmd == "remove")
    {
        if (args.size() < 3)
        {
            std::cout << "invalid number of arguments\n";
            std::cout << "usage: cppan remove project|version [remote] name ...\n";
            return ApiResult::Error;
        }

        size_t arg = 2;
        String what = args[arg++];
        if (what == "project" || what == "package")
        {
            auto proj_usage = []
            {
                std::cout << "invalid number of arguments\n";
                std::cout << "usage: cppan remove project [remote] name\n";
            };

            if (args.size() < arg + 1)
            {
                proj_usage();
                return ApiResult::Error;
            }

            String remote = DEFAULT_REMOTE_NAME;
            ProjectPath p(args[arg++]);
            if (has_remote(remote) && p.is_relative() && p.size() == 1)
            {
                remote = args[arg - 1];

                if (args.size() < arg + 1)
                {
                    proj_usage();
                    return ApiResult::Error;
                }

                p = ProjectPath(args[arg++]);
            }

            Api().remove_project(find_remote(remote), p);
            return ApiResult::Handled;
        }

        if (what == "version")
        {
            auto proj_usage = []
            {
                std::cout << "invalid number of arguments\n";
                std::cout << "usage: cppan remove version [remote] name version\n";
            };

            if (args.size() < arg + 1)
            {
                proj_usage();
                return ApiResult::Error;
            }

            String remote = DEFAULT_REMOTE_NAME;
            ProjectPath p(args[arg++]);
            if (has_remote(remote) && p.is_relative() && p.size() == 1)
            {
                remote = args[arg - 1];

                if (args.size() < arg + 1)
                {
                    proj_usage();
                    return ApiResult::Error;
                }

                p = ProjectPath(args[arg++]);
            }

            if (args.size() < arg + 1)
            {
                proj_usage();
                return ApiResult::Error;
            }

            Api().remove_version(find_remote(remote), p, String(args[arg++]));
            return ApiResult::Handled;
        }

        return ApiResult::Handled;
    }

    if (cmd == "notifications")
    {
        auto proj_usage = []
        {
            std::cout << "invalid number of arguments\n";
            std::cout << "usage: cppan notifications [origin] [clear] [N]\n";
        };

        size_t arg = 2;

        String remote = DEFAULT_REMOTE_NAME;
        if (args.size() < arg + 1)
        {
            Api().get_notifications(find_remote(remote));
            return ApiResult::Handled;
        }

        String arg2 = args[arg++];
        if (args.size() < arg + 1)
        {
            if (arg2 == "clear")
            {
                Api().clear_notifications(find_remote(remote));
                return ApiResult::Handled;
            }

            int n = 10;
            try
            {
                n = std::stoi(arg2);
            }
            catch (const std::exception&)
            {
                Api().get_notifications(find_remote(arg2));
                return ApiResult::Handled;
            }

            Api().get_notifications(find_remote(remote), n);
            return ApiResult::Handled;
        }

        String arg3 = args[arg++];
        if (args.size() < arg + 1)
        {
            if (arg3 == "clear")
            {
                Api().clear_notifications(find_remote(arg2));
                return ApiResult::Handled;
            }

            Api().get_notifications(find_remote(arg2), std::stoi(arg3));
            return ApiResult::Handled;
        }

        return ApiResult::Error;
    }

    return ApiResult::NotHandled;
}
