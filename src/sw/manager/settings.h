// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "yaml.h"

#include <sw/support/enums.h>
#include <sw/support/filesystem.h>

#include <primitives/http.h>
#include <primitives/sw/settings.h>

#include <map>

namespace sw
{

struct Remote;

struct SW_MANAGER_API Settings
{
    // connection
    ProxySettings proxy;

    path storage_dir;

    // do not check for new cppan version
    bool disable_update_checks = false;
    bool can_update_packages_db = true;
    //bool verify_all = false;
    bool record_commands = false;
    bool record_commands_in_current_dir = false;

    // not from file (local settings?)

    // db
    bool gForceServerQuery = false;
    bool gForceServerDatabaseUpdate = false;

    // command
    bool save_failed_commands = false;
    bool save_all_commands = false;
    bool save_executed_commands = false;

    bool explain_outdated = false;
    bool explain_outdated_full = false;
    bool gExplainOutdatedToTrace = false;

    String save_command_format;

public:
    Settings();
    ~Settings();

    void load(const path &p, const SettingsType type);
    void load(const yaml &root, const SettingsType type);
    void save(const path &p) const;

    bool checkForUpdates() const;

    const std::vector<std::shared_ptr<Remote>> &getRemotes(bool allow_network) const;
    void setDefaultRemote(const String &r) { default_remote = r; }

private:
    yaml root;
    String default_remote;
    mutable std::vector<std::shared_ptr<Remote>> remotes;

    void load_main(const yaml &root, const SettingsType type);

public:
    static Settings &get(SettingsType type);
    static Settings &get_system_settings();
    static Settings &get_user_settings();
    static Settings &get_local_settings();
    static void clear_local_settings();
};

}
