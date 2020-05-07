/*
 * SW - Build System and Package Manager
 * Copyright (C) 2016-2018 Egor Pugin
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

#include "settings.h"

#include "database.h"
#include "remote.h"
#include "stamp.h"
#include "storage.h"

#include <sw/support/exceptions.h>
#include <sw/support/hash.h>

#include <boost/algorithm/string.hpp>
#include <primitives/hasher.h>
#include <primitives/templates.h>

#include <fstream>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "settings");

#define CONFIG_ROOT "/etc/sw/"

String default_remote;

namespace sw
{

Settings::Settings()
{
    storage_dir = get_root_directory() / "storage";
}

Settings::~Settings()
{
}

void Settings::load(const path &p, const SettingsType type)
{
    root = load_yaml_config(p);
    load(root, type);
}

void Settings::load(const yaml &root, const SettingsType type)
{
    load_main(root, type);
}

void Settings::load_main(const yaml &root, const SettingsType type)
{
    YAML_EXTRACT_AUTO(disable_update_checks);
    YAML_EXTRACT_AUTO(record_commands);
    YAML_EXTRACT_AUTO(record_commands_in_current_dir);
    YAML_EXTRACT(storage_dir, String);

    auto &p = root["proxy"];
    if (p.IsDefined())
    {
        if (!p.IsMap())
            throw SW_RUNTIME_ERROR("'proxy' should be a map");
        YAML_EXTRACT_VAR(p, proxy.host, "host", String);
        YAML_EXTRACT_VAR(p, proxy.user, "user", String);
    }
}

const std::vector<std::shared_ptr<Remote>> &Settings::getRemotes() const
{
    static std::mutex m;
    std::unique_lock lk(m);
    if (!remotes.empty())
        return remotes;

    remotes = get_default_remotes();

    get_map_and_iterate(root, "remotes", [this](auto &kv)
    {
        Remote *pr;

        auto n = kv.first.template as<String>();
        if (n == DEFAULT_REMOTE_NAME) // origin
        {
            pr = remotes[0].get();
        }
        else
        {
            String url;
            YAML_EXTRACT_VAR(kv.second, url, "url", String);
            auto r = std::make_shared<Remote>(n, url);
            remotes.push_back(r);
            pr = r.get();
        }

        YAML_EXTRACT_VAR(kv.second, pr->secure, "secure", bool);
        //YAML_EXTRACT_VAR(kv.second, prm.data_dir, "data_dir", String);
        get_map_and_iterate(kv.second, "publishers", [pr](auto &kv)
        {
            Remote::Publisher p;
            YAML_EXTRACT_VAR(kv.second, p.name, "name", String);
            YAML_EXTRACT_VAR(kv.second, p.token, "token", String);
            pr->publishers[p.name] = p;
        });
    });

    if (!default_remote.empty())
    {
        auto i = std::find_if(remotes.begin(), remotes.end(), [](const auto &r)
        {
            return r->name == default_remote;
        });
        if (i == remotes.end())
            throw SW_RUNTIME_ERROR("Remote not found: " + default_remote);
        std::swap(*i, *remotes.begin());
    }

    return remotes;
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
    download_file(remotes[0]->url + stamp_file, fn);
    auto stamp_remote = boost::trim_copy(read_file(fn));
    fs::remove(fn);
    boost::replace_all(stamp_remote, "\"", "");
    uint64_t s1 = cppan_stamp.empty() ? 0 : std::stoull(cppan_stamp);
    uint64_t s2 = std::stoull(stamp_remote);
    if (!(s1 != 0 && s2 != 0 && s2 > s1))
        return false;

    LOG_INFO(logger, "New version of the SW client is available!");
    LOG_INFO(logger, "Feel free to upgrade it from the website (https://software-network.org/) or simply run:");
    LOG_INFO(logger, "sw --self-upgrade");
#ifdef _WIN32
    LOG_INFO(logger, "(or the same command but from administrator)");
#else
    LOG_INFO(logger, "or");
    LOG_INFO(logger, "sudo sw --self-upgrade");
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
                auto &ss = get(SettingsType::System);
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
        root["remotes"][r->name]["url"] = r->url;
        if (!r->secure)
            root["remotes"][r->name]["secure"] = r->secure;
        for (auto &[n, p] : r->publishers)
        {
            root["remotes"][r->name]["publishers"][p.name]["name"] = p.name;
            root["remotes"][r->name]["publishers"][p.name]["token"] = p.token;
        }
    }
    root["storage_dir"] = storage_dir.string();

    root["record_commands"] = record_commands;
    root["record_commands_in_current_dir"] = record_commands_in_current_dir;

    std::ofstream o(p);
    if (!o)
        throw SW_RUNTIME_ERROR("Cannot open file: " + p.string());
    o << dump_yaml_config(root);
}

}
