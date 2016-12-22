/*
 * Copyright (c) 2016, Egor Pugin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *     3. Neither the name of the copyright holder nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "settings.h"

#include "access_table.h"
#include "config.h"
#include "database.h"
#include "directories.h"
#include "hash.h"
#include "hasher.h"
#include "log.h"
#include "program.h"
#include "templates.h"
#include "stamp.h"

#include <boost/algorithm/string.hpp>

#include "logger.h"
DECLARE_STATIC_LOGGER(logger, "settings");

void BuildSettings::set_build_dirs(const String &name)
{
    filename = name;
    filename_without_ext = name;

    source_directory = directories.build_dir;
    if (directories.build_dir_type == SettingsType::Local ||
        directories.build_dir_type == SettingsType::None)
    {
        source_directory /= (CPPAN_LOCAL_BUILD_PREFIX + filename);
    }
    else
    {
        source_directory_hash = sha256_short(name);
        source_directory /= source_directory_hash;
    }
    binary_directory = source_directory / "build";
}

void BuildSettings::append_build_dirs(const path &p)
{
    source_directory /= p;
    binary_directory = source_directory / "build";
}

Settings::Settings()
{
    build_dir = temp_directory_path() / "build";
    storage_dir = get_root_directory() / STORAGE_DIR;
}

void Settings::load(const path &p, const SettingsType type)
{
    auto root = load_yaml_config(p);
    load(root, type);
}

void Settings::load(const yaml &root, const SettingsType type)
{
    load_main(root, type);

    auto get_storage_dir = [this](SettingsType type)
    {
        switch (type)
        {
        case SettingsType::Local:
            return cppan_dir / STORAGE_DIR;
        case SettingsType::User:
            return Settings::get_user_settings().storage_dir;
        case SettingsType::System:
            return Settings::get_system_settings().storage_dir;
        default:
        {
            auto d = fs::absolute(storage_dir);
            fs::create_directories(d);
            return fs::canonical(d);
        }
        }
    };

    auto get_build_dir = [this](const path &p, SettingsType type)
    {
        switch (type)
        {
        case SettingsType::Local:
            return fs::current_path();
        case SettingsType::User:
            return directories.storage_dir_tmp;
        case SettingsType::System:
            return temp_directory_path() / "build";
        default:
            return p;
        }
    };

    Directories dirs;
    dirs.storage_dir_type = storage_dir_type;
    auto sd = get_storage_dir(storage_dir_type);
    dirs.set_storage_dir(sd);
    dirs.build_dir_type = build_dir_type;
    dirs.set_build_dir(get_build_dir(build_dir, build_dir_type));
    directories.update(dirs, type);
}

void Settings::load_main(const yaml &root, const SettingsType type)
{
    auto packages_dir_type_from_string = [](const String &s, const String &key)
    {
        if (s == "local")
            return SettingsType::Local;
        if (s == "user")
            return SettingsType::User;
        if (s == "system")
            return SettingsType::System;
        throw std::runtime_error("Unknown '" + key + "'. Should be one of [local, user, system]");
    };

    get_map_and_iterate(root, "remotes", [this](auto &kv)
    {
        auto n = kv.first.template as<String>();
        bool o = n == DEFAULT_REMOTE_NAME; // origin
        Remote rm;
        Remote *prm = o ? &remotes[0] : &rm;
        prm->name = n;
        EXTRACT_VAR(kv.second, prm->url, "url", String);
        EXTRACT_VAR(kv.second, prm->data_dir, "data_dir", String);
        EXTRACT_VAR(kv.second, prm->user, "user", String);
        EXTRACT_VAR(kv.second, prm->token, "token", String);
        if (!o)
            remotes.push_back(*prm);
    });

    EXTRACT_AUTO(disable_update_checks);
    EXTRACT(storage_dir, String);
    EXTRACT(build_dir, String);
    EXTRACT(cppan_dir, String);

    auto &p = root["proxy"];
    if (p.IsDefined())
    {
        if (!p.IsMap())
            throw std::runtime_error("'proxy' should be a map");
        EXTRACT_VAR(p, proxy.host, "host", String);
        EXTRACT_VAR(p, proxy.user, "user", String);
    }

    storage_dir_type = packages_dir_type_from_string(get_scalar<String>(root, "storage_dir_type", "user"), "storage_dir_type");
    if (root["storage_dir"].IsDefined())
        storage_dir_type = SettingsType::None;
    build_dir_type = packages_dir_type_from_string(get_scalar<String>(root, "build_dir_type", "system"), "build_dir_type");
    if (root["build_dir"].IsDefined())
        build_dir_type = SettingsType::None;

    // read these first from local settings
    // and they'll be overriden in bs (if they exist there)
    EXTRACT_AUTO(use_cache);
    EXTRACT_AUTO(show_ide_projects);
    EXTRACT_AUTO(add_run_cppan_target);
    EXTRACT_AUTO(cmake_verbose);
    EXTRACT_AUTO(build_system_verbose);
    EXTRACT_AUTO(verify_all);
    EXTRACT_AUTO(var_check_jobs);
    EXTRACT_VAR(root, build_warning_level, "build_warning_level", int);

    // read build settings
    if (type == SettingsType::Local)
    {
        // at first, we load bs from current root
        load_build(root);

        // then override settings with specific (or default) config
        yaml current_build;
        if (root["builds"].IsDefined())
        {
            // yaml will not keep sorting of keys in map
            // so we can take 'first' build in document
            if (root["current_build"].IsDefined())
                current_build = root["builds"][root["current_build"].template as<String>()];
        }
        else if (root["build"].IsDefined())
            current_build = root["build"];

        load_build(current_build);
    }

    // read project settings (deps etc.)
    if (type == SettingsType::Local)
    {
        Project p;
        p.load(root);
        dependencies = p.dependencies;
    }
}

void Settings::load_build(const yaml &root)
{
    if (root.IsNull())
        return;

    // extract
    EXTRACT_AUTO(c_compiler);
    EXTRACT_AUTO(cxx_compiler);
    EXTRACT_AUTO(compiler);
    EXTRACT_AUTO(c_compiler_flags);
    if (c_compiler_flags.empty())
        EXTRACT_VAR(root, c_compiler_flags, "c_flags", String);
    EXTRACT_AUTO(cxx_compiler_flags);
    if (cxx_compiler_flags.empty())
        EXTRACT_VAR(root, cxx_compiler_flags, "cxx_flags", String);
    EXTRACT_AUTO(compiler_flags);
    EXTRACT_AUTO(link_flags);
    EXTRACT_AUTO(link_libraries);
    EXTRACT_AUTO(configuration);
    EXTRACT_AUTO(generator);
    EXTRACT_AUTO(toolset);
    EXTRACT_AUTO(use_shared_libs);
    EXTRACT_AUTO(silent);
    EXTRACT_AUTO(use_cache);
    EXTRACT_AUTO(show_ide_projects);
    EXTRACT_AUTO(add_run_cppan_target);
    EXTRACT_AUTO(cmake_verbose);
    EXTRACT_AUTO(build_system_verbose);
    EXTRACT_AUTO(verify_all);
    EXTRACT_AUTO(var_check_jobs);
    EXTRACT_VAR(root, build_warning_level, "build_warning_level", int);

    for (int i = 0; i < CMakeConfigurationType::Max; i++)
    {
        auto t = configuration_types[i];
        boost::to_lower(t);
        EXTRACT_VAR(root, c_compiler_flags_conf[i], "c_compiler_flags_" + t, String);
        EXTRACT_VAR(root, cxx_compiler_flags_conf[i], "cxx_compiler_flags_" + t, String);
        EXTRACT_VAR(root, compiler_flags_conf[i], "compiler_flags_" + t, String);
        EXTRACT_VAR(root, link_flags_conf[i], "link_flags_" + t, String);
    }

    cmake_options = get_sequence<String>(root["cmake_options"]);
    get_string_map(root, "env", env);

    // process
    if (c_compiler.empty())
        c_compiler = cxx_compiler;
    if (c_compiler.empty())
        c_compiler = compiler;
    if (cxx_compiler.empty())
        cxx_compiler = compiler;

    if (!compiler_flags.empty())
    {
        c_compiler_flags += " " + compiler_flags;
        cxx_compiler_flags += " " + compiler_flags;
    }
    for (int i = 0; i < CMakeConfigurationType::Max; i++)
    {
        if (!compiler_flags_conf[i].empty())
        {
            c_compiler_flags_conf[i] += " " + compiler_flags_conf[i];
            cxx_compiler_flags_conf[i] += " " + compiler_flags_conf[i];
        }
    }
}

bool Settings::is_custom_build_dir() const
{
    return build_dir_type == SettingsType::Local || build_dir_type == SettingsType::None;
}

String Settings::get_hash() const
{
    Hasher h;
    h |= c_compiler;
    h |= cxx_compiler;
    h |= compiler;
    h |= c_compiler_flags;
    for (int i = 0; i < CMakeConfigurationType::Max; i++)
        h |= c_compiler_flags_conf[i];
    h |= cxx_compiler_flags;
    for (int i = 0; i < CMakeConfigurationType::Max; i++)
        h |= cxx_compiler_flags_conf[i];
    h |= compiler_flags;
    for (int i = 0; i < CMakeConfigurationType::Max; i++)
        h |= compiler_flags_conf[i];
    h |= link_flags;
    for (int i = 0; i < CMakeConfigurationType::Max; i++)
        h |= link_flags_conf[i];
    h |= link_libraries;
    h |= generator;
    h |= toolset;
    h |= use_shared_libs;

    // besides we track all valuable ENV vars
    // to be sure that we'll load correct config
    auto add_env = [&h](const char *var)
    {
        auto e = getenv(var);
        if (!e)
            return;
        h |= String(e);
    };

    add_env("PATH");
    add_env("Path");
    add_env("FPATH");
    add_env("CPATH");

    // windows, msvc
    add_env("VSCOMNTOOLS");
    add_env("VS71COMNTOOLS");
    add_env("VS80COMNTOOLS");
    add_env("VS90COMNTOOLS");
    add_env("VS100COMNTOOLS");
    add_env("VS110COMNTOOLS");
    add_env("VS120COMNTOOLS");
    add_env("VS130COMNTOOLS");
    add_env("VS140COMNTOOLS");
    add_env("VS141COMNTOOLS"); // 2017?
    add_env("VS150COMNTOOLS");
    add_env("VS151COMNTOOLS");
    add_env("VS160COMNTOOLS"); // for the future

    add_env("INCLUDE");
    add_env("LIB");

    // gcc
    add_env("COMPILER_PATH");
    add_env("LIBRARY_PATH");
    add_env("C_INCLUDE_PATH");
    add_env("CPLUS_INCLUDE_PATH");
    add_env("OBJC_INCLUDE_PATH");
    //add_env("LD_LIBRARY_PATH"); // do we need these?
    //add_env("DYLD_LIBRARY_PATH");

    add_env("CC");
    add_env("CFLAGS");
    add_env("CXXFLAGS");
    add_env("CPPFLAGS");

    return h.hash;
}

bool Settings::checkForUpdates() const
{
    if (disable_update_checks)
        return false;

#ifdef _WIN32
    String stamp_file = "/client/.service/win32.stamp";
#elif __APPLE__
    String stamp_file = "/client/.service/macos.stamp";
#else
    String stamp_file = "/client/.service/linux.stamp";
#endif

    DownloadData dd;
    dd.url = remotes[0].url + stamp_file;
    dd.fn = fs::temp_directory_path() / fs::unique_path();
    download_file(dd);
    auto stamp_remote = boost::trim_copy(read_file(dd.fn));
    boost::replace_all(stamp_remote, "\"", "");
    uint64_t s1 = std::stoull(cppan_stamp);
    uint64_t s2 = std::stoull(stamp_remote);
    if (!(s1 != 0 && s2 != 0 && s2 > s1))
        return false;

    std::cout << "New version of the CPPAN client is available!" << "\n";
    std::cout << "Feel free to upgrade it from website or simply run:" << "\n";
    std::cout << "cppan --self-upgrade" << "\n";
#ifdef _WIN32
    std::cout << "(or the same command but from administrator)" << "\n";
#else
    std::cout << "or" << "\n";
    std::cout << "sudo cppan --self-upgrade" << "\n";
#endif
    std::cout << "\n";
    return true;
}

Settings &Settings::get(SettingsType type)
{
    static Settings settings[toIndex(SettingsType::Max) + 1];

    auto i = toIndex(type);
    auto &s = settings[i];
    switch (type)
    {
    case SettingsType::Local:
    {
        RUN_ONCE
        {
            s = get(SettingsType::User);
        };
    }
        break;
    case SettingsType::User:
    {
        RUN_ONCE
        {
            s = get(SettingsType::System);

            auto fn = get_config_filename();
            if (!fs::exists(fn))
            {
                boost::system::error_code ec;
                fs::create_directories(fn.parent_path(), ec);
                if (ec)
                    throw std::runtime_error(ec.message());
                auto ss = get(SettingsType::System);
                ss.save(fn);
            }
            s.load(fn, SettingsType::User);
        };
    }
        break;
    case SettingsType::System:
    {
        RUN_ONCE
        {
            auto fn = CONFIG_ROOT "default";
            if (!fs::exists(fn))
                return;
            s.load(fn, SettingsType::System);
        };
    }
        break;
    }
    return s;
}

Settings &Settings::get_system_settings()
{
    return get(SettingsType::System);
}

Settings &Settings::get_user_settings()
{
    return get(SettingsType::User);
}

Settings &Settings::get_local_settings()
{
    return get(SettingsType::Local);
}

void Settings::clear_local_settings()
{
    get_local_settings() = get_user_settings();
}

void Settings::save(const path &p) const
{
    std::ofstream o(p.string());
    if (!o)
        throw std::runtime_error("Cannot open file: " + p.string());
    yaml root;
    root["remotes"][DEFAULT_REMOTE_NAME]["url"] = remotes[0].url;
    root["storage_dir"] = storage_dir.string();
    o << dump_yaml_config(root);
}
