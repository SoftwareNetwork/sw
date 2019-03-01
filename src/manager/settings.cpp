// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "settings.h"

#include "database.h"
#include "directories.h"
#include "exceptions.h"
#include "hash.h"
#include "stamp.h"

#include <boost/algorithm/string.hpp>

#include <primitives/hasher.h>
#include <primitives/templates.h>
#include <primitives/sw/cl.h>

#include <fstream>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "settings");

#define CPPAN_LOCAL_BUILD_PREFIX "sw-build-"
#define CONFIG_ROOT "/etc/sw/"

static cl::opt<String> default_remote("r", cl::desc("Select default remote"));

namespace sw
{

Directories &getDirectoriesUnsafe();

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
        /*case SettingsType::Local:
            return cppan_dir / STORAGE_DIR;*/
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

    getDirectoriesUnsafe().update(dirs, type);
}

void Settings::load_main(const yaml &root, const SettingsType type)
{
    auto packages_dir_type_from_string = [](const String &s, const String &key) {
        if (s == "local")
            return SettingsType::Local;
        if (s == "user")
            return SettingsType::User;
        if (s == "system")
            return SettingsType::System;
        throw SW_RUNTIME_ERROR("Unknown '" + key + "'. Should be one of [local, user, system]");
    };

    get_map_and_iterate(root, "remotes", [this](auto &kv) {
        auto n = kv.first.template as<String>();
        bool o = n == DEFAULT_REMOTE_NAME; // origin
        Remote rm;
        Remote *prm = o ? &remotes[0] : &rm;
        prm->name = n;
        String provider;
        YAML_EXTRACT_VAR(kv.second, prm->url, "url", String);
        YAML_EXTRACT_VAR(kv.second, prm->secure, "secure", bool);
        //YAML_EXTRACT_VAR(kv.second, prm->data_dir, "data_dir", String);
        YAML_EXTRACT_VAR(kv.second, provider, "provider", String);
        if (!provider.empty())
        {
            /*if (provider == "sw")
                prm->default_source = &Remote::sw_source_provider;*/
        }
        get_map_and_iterate(kv.second, "publishers", [&prm](auto &kv) {
            Remote::Publisher p;
            YAML_EXTRACT_VAR(kv.second, p.name, "name", String);
            YAML_EXTRACT_VAR(kv.second, p.token, "token", String);
            prm->publishers[p.name] = p;
        });
        if (!o)
            remotes.push_back(*prm);
    });

    if (!default_remote.empty())
    {
        auto i = std::find_if(remotes.begin(), remotes.end(), [](const auto &r)
        {
            return r.name == default_remote;
        });
        if (i == remotes.end())
            throw SW_RUNTIME_ERROR("Remote not found: " + default_remote);
        std::swap(*i, *remotes.begin());
    }

    YAML_EXTRACT_AUTO(disable_update_checks);
    YAML_EXTRACT(storage_dir, String);
    YAML_EXTRACT(build_dir, String);
    YAML_EXTRACT(output_dir, String);

    auto &p = root["proxy"];
    if (p.IsDefined())
    {
        if (!p.IsMap())
            throw SW_RUNTIME_ERROR("'proxy' should be a map");
        YAML_EXTRACT_VAR(p, proxy.host, "host", String);
        YAML_EXTRACT_VAR(p, proxy.user, "user", String);
    }

    storage_dir_type = packages_dir_type_from_string(get_scalar<String>(root, "storage_dir_type", "user"), "storage_dir_type");
    /*if (root["storage_dir"].IsDefined())
        storage_dir_type = SettingsType::None;*/
    build_dir_type = packages_dir_type_from_string(get_scalar<String>(root, "build_dir_type", "system"), "build_dir_type");
    /*if (root["build_dir"].IsDefined())
        build_dir_type = SettingsType::None;*/
}

bool Settings::is_custom_build_dir() const
{
    return build_dir_type == SettingsType::Local/* || build_dir_type == SettingsType::None*/;
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

    auto fn = fs::temp_directory_path() / unique_path();
    download_file(remotes[0].url + stamp_file, fn);
    auto stamp_remote = boost::trim_copy(read_file(fn));
    fs::remove(fn);
    boost::replace_all(stamp_remote, "\"", "");
    uint64_t s1 = cppan_stamp.empty() ? 0 : std::stoull(cppan_stamp);
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
        RUN_ONCE
        {
            s = get(SettingsType::System);

            auto fn = get_config_filename();
            if (!fs::exists(fn))
            {
                error_code ec;
                fs::create_directories(fn.parent_path(), ec);
                if (ec)
                    throw SW_RUNTIME_ERROR(ec.message());
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
                break;
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
    yaml root;
    for (auto &r : remotes)
    {
        root["remotes"][r.name]["url"] = r.url;
        if (!r.secure)
            root["remotes"][r.name]["secure"] = r.secure;
        for (auto &[n, p] : r.publishers)
        {
            root["remotes"][r.name]["publishers"][p.name]["name"] = p.name;
            root["remotes"][r.name]["publishers"][p.name]["token"] = p.token;
        }
    }
    root["storage_dir"] = storage_dir.string();

    std::ofstream o(p);
    if (!o)
        throw SW_RUNTIME_ERROR("Cannot open file: " + p.string());
    o << dump_yaml_config(root);
}

}
