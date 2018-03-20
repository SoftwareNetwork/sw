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

#include "settings.h"

#include "access_table.h"
#include "config.h"
#include "database.h"
#include "directories.h"
#include "exceptions.h"
#include "hash.h"
#include "program.h"
#include "stamp.h"

#include <boost/algorithm/string.hpp>
#include <boost/nowide/fstream.hpp>

#include <primitives/hasher.h>
#include <primitives/templates.h>

#include <primitives/log.h>
//DECLARE_STATIC_LOGGER(logger, "settings");

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

    auto get_build_dir = [](const path &p, SettingsType type, const auto &dirs)
    {
        switch (type)
        {
        case SettingsType::Local:
            return current_thread_path();
        case SettingsType::User:
        case SettingsType::System:
            return dirs.storage_dir_tmp / "build";
        default:
            return p;
        }
    };

    Directories dirs;
    dirs.storage_dir_type = storage_dir_type;
    auto sd = get_storage_dir(storage_dir_type);
    dirs.set_storage_dir(sd);
    dirs.build_dir_type = build_dir_type;
    dirs.set_build_dir(get_build_dir(build_dir, build_dir_type, dirs));
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
        YAML_EXTRACT_VAR(kv.second, prm->url, "url", String);
        YAML_EXTRACT_VAR(kv.second, prm->data_dir, "data_dir", String);
        YAML_EXTRACT_VAR(kv.second, prm->user, "user", String);
        YAML_EXTRACT_VAR(kv.second, prm->token, "token", String);
        if (!o)
            remotes.push_back(*prm);
    });

    YAML_EXTRACT_AUTO(disable_update_checks);
    YAML_EXTRACT_AUTO(max_download_threads);
    YAML_EXTRACT_AUTO(debug_generated_cmake_configs);
    YAML_EXTRACT(storage_dir, String);
    YAML_EXTRACT(build_dir, String);
    YAML_EXTRACT(cppan_dir, String);
    YAML_EXTRACT(output_dir, String);

/*#ifdef _WIN32
    // correctly convert to utf-8
#define CONVERT_DIR(x) x = to_wstring(x.string())
    CONVERT_DIR(storage_dir);
    CONVERT_DIR(build_dir);
    CONVERT_DIR(cppan_dir);
    CONVERT_DIR(output_dir);
#endif*/

    auto &p = root["proxy"];
    if (p.IsDefined())
    {
        if (!p.IsMap())
            throw std::runtime_error("'proxy' should be a map");
        YAML_EXTRACT_VAR(p, proxy.host, "host", String);
        YAML_EXTRACT_VAR(p, proxy.user, "user", String);
    }

    storage_dir_type = packages_dir_type_from_string(get_scalar<String>(root, "storage_dir_type", "user"), "storage_dir_type");
    if (root["storage_dir"].IsDefined())
        storage_dir_type = SettingsType::None;
    build_dir_type = packages_dir_type_from_string(get_scalar<String>(root, "build_dir_type", "system"), "build_dir_type");
    if (root["build_dir"].IsDefined())
        build_dir_type = SettingsType::None;

    // read these first from local settings
    // and they'll be overriden in bs (if they exist there)
    YAML_EXTRACT_AUTO(use_cache);
    YAML_EXTRACT_AUTO(show_ide_projects);
    YAML_EXTRACT_AUTO(add_run_cppan_target);
    YAML_EXTRACT_AUTO(cmake_verbose);
    YAML_EXTRACT_AUTO(build_system_verbose);
    YAML_EXTRACT_AUTO(verify_all);
    YAML_EXTRACT_AUTO(copy_all_libraries_to_output);
    YAML_EXTRACT_AUTO(copy_import_libs);
    YAML_EXTRACT_AUTO(rc_enabled);
    YAML_EXTRACT_AUTO(short_local_names);
    YAML_EXTRACT_AUTO(full_path_executables);
    YAML_EXTRACT_AUTO(var_check_jobs);
    YAML_EXTRACT_AUTO(install_prefix);
    YAML_EXTRACT_AUTO(build_warning_level);
    YAML_EXTRACT_AUTO(meta_target_suffix);

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
            {
                if (root["builds"][root["current_build"].template as<String>()].IsDefined())
                    current_build = root["builds"][root["current_build"].template as<String>()];
                else
                {
                    // on empty config name we build the first configuration
                    LOG_WARN(logger, "No such build config '" + root["current_build"].template as<String>() +
                        "' in builds directive. Trying to build the first configuration.");
                    current_build = root["builds"].begin()->second;
                }
            }
        }
        else if (root["build"].IsDefined())
            current_build = root["build"];

        load_build(current_build);
    }

    // read project settings (deps etc.)
    if (type == SettingsType::Local && load_project)
    {
        Project p;
        p.allow_relative_project_names = true;
        p.allow_local_dependencies = true;
        p.load(root);
        dependencies = p.dependencies;
    }
}

void Settings::load_build(const yaml &root)
{
    if (root.IsNull())
        return;

    // extract
    YAML_EXTRACT_AUTO(c_compiler);
    YAML_EXTRACT_AUTO(cxx_compiler);
    YAML_EXTRACT_AUTO(compiler);
    YAML_EXTRACT_AUTO(c_compiler_flags);
    if (c_compiler_flags.empty())
        YAML_EXTRACT_VAR(root, c_compiler_flags, "c_flags", String);
    YAML_EXTRACT_AUTO(cxx_compiler_flags);
    if (cxx_compiler_flags.empty())
        YAML_EXTRACT_VAR(root, cxx_compiler_flags, "cxx_flags", String);
    YAML_EXTRACT_AUTO(compiler_flags);
    YAML_EXTRACT_AUTO(link_flags);
    YAML_EXTRACT_AUTO(link_libraries);
    YAML_EXTRACT_AUTO(configuration);
    YAML_EXTRACT_AUTO(generator);
    YAML_EXTRACT_AUTO(system_version);
    YAML_EXTRACT_AUTO(toolset);
    YAML_EXTRACT_AUTO(use_shared_libs);
    YAML_EXTRACT_VAR(root, use_shared_libs, "build_shared_libs", bool);
    YAML_EXTRACT_AUTO(silent);
    YAML_EXTRACT_AUTO(use_cache);
    YAML_EXTRACT_AUTO(show_ide_projects);
    YAML_EXTRACT_AUTO(add_run_cppan_target);
    YAML_EXTRACT_AUTO(cmake_verbose);
    YAML_EXTRACT_AUTO(build_system_verbose);
    YAML_EXTRACT_AUTO(verify_all);
    YAML_EXTRACT_AUTO(copy_all_libraries_to_output);
    YAML_EXTRACT_AUTO(copy_import_libs);
    YAML_EXTRACT_AUTO(rc_enabled);
    YAML_EXTRACT_AUTO(short_local_names);
    YAML_EXTRACT_AUTO(full_path_executables);
    YAML_EXTRACT_AUTO(var_check_jobs);
    YAML_EXTRACT_AUTO(install_prefix);
    YAML_EXTRACT_AUTO(build_warning_level);
    YAML_EXTRACT_AUTO(meta_target_suffix);

    for (int i = 0; i < CMakeConfigurationType::Max; i++)
    {
        auto t = configuration_types[i];
        boost::to_lower(t);

        YAML_EXTRACT_VAR(root, c_compiler_flags_conf[i], "c_compiler_flags_" + t, String);
        if (c_compiler_flags_conf[i].empty())
            YAML_EXTRACT_VAR(root, c_compiler_flags_conf[i], "c_flags_" + t, String);

        YAML_EXTRACT_VAR(root, cxx_compiler_flags_conf[i], "cxx_compiler_flags_" + t, String);
        if (cxx_compiler_flags_conf[i].empty())
            YAML_EXTRACT_VAR(root, cxx_compiler_flags_conf[i], "cxx_flags_" + t, String);

        YAML_EXTRACT_VAR(root, compiler_flags_conf[i], "compiler_flags_" + t, String);

        YAML_EXTRACT_VAR(root, link_flags_conf[i], "link_flags_" + t, String);
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
    h |= system_version;
    h |= toolset;
    h |= use_shared_libs;
    h |= configuration;
    h |= default_configuration;

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

    auto fn = fs::temp_directory_path() / fs::unique_path();
    download_file(remotes[0].url + stamp_file, fn);
    auto stamp_remote = boost::trim_copy(read_file(fn));
    fs::remove(fn);
    boost::replace_all(stamp_remote, "\"", "");
    uint64_t s1 = std::stoull(cppan_stamp);
    uint64_t s2 = std::stoull(stamp_remote);
    if (!(s1 != 0 && s2 != 0 && s2 > s1))
        return false;

    LOG_INFO(logger, "New version of the CPPAN client is available!");
    LOG_INFO(logger, "Feel free to upgrade it from website (https://cppan.org/) or simply run:");
    LOG_INFO(logger, "cppan --self-upgrade");
#ifdef _WIN32
    LOG_INFO(logger, "(or the same command but from administrator)");
#else
    LOG_INFO(logger, "or");
    LOG_INFO(logger, "sudo cppan --self-upgrade");
#endif
    LOG_INFO(logger, "");
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
        std::exception_ptr eptr;
        RUN_ONCE
        {
            try
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
            }
            catch (...)
            {
                eptr = std::current_exception();
            }
        };

        if (eptr)
            std::rethrow_exception(eptr);
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
    default:
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
    boost::nowide::ofstream o(p.string());
    if (!o)
        throw std::runtime_error("Cannot open file: " + p.string());
    yaml root;
    root["remotes"][DEFAULT_REMOTE_NAME]["url"] = remotes[0].url;
    root["storage_dir"] = storage_dir.string();
    o << dump_yaml_config(root);
}

void cleanConfig(const String &c)
{
    if (c.empty())
        return;

    auto h = hash_config(c);

    auto remove_pair = [&](const auto &dir)
    {
        fs::remove_all(dir / c);
        fs::remove_all(dir / h);
    };

    remove_pair(directories.storage_dir_bin);
    remove_pair(directories.storage_dir_lib);
    remove_pair(directories.storage_dir_exp);
#ifdef _WIN32
    remove_pair(directories.storage_dir_lnk);
#endif

    // for cfg we also remove xxx.cmake files (found with xxx.c.cmake files)
    remove_pair(directories.storage_dir_cfg);
    for (auto &f : boost::make_iterator_range(fs::directory_iterator(directories.storage_dir_cfg), {}))
    {
        if (!fs::is_regular_file(f) || f.path().extension() != ".cmake")
            continue;
        auto parts = split_string(f.path().string(), ".");
        if (parts.size() == 2)
        {
            if (parts[0] == c || parts[0] == h)
                fs::remove(f);
            continue;
        }
        if (parts.size() == 3)
        {
            if (parts[1] == c || parts[1] == h)
            {
                fs::remove(parts[0] + ".cmake");
                fs::remove(parts[1] + ".cmake");
                fs::remove(f);
            }
            continue;
        }
    }

    // obj
    auto &sdb = getServiceDatabase();
    for (auto &p : sdb.getInstalledPackages())
    {
        auto d = p.getDirObj() / "build";
        if (!fs::exists(d))
            continue;
        for (auto &f : boost::make_iterator_range(fs::directory_iterator(d), {}))
        {
            if (!fs::is_directory(f))
                continue;
            if (f.path().filename() == c || f.path().filename() == h)
                fs::remove_all(f);
        }
    }

    // config hashes (in sdb)
    sdb.removeConfigHashes(c);
    sdb.removeConfigHashes(h);
}

void cleanConfigs(const Strings &configs)
{
    for (auto &c : configs)
        cleanConfig(c);
}
