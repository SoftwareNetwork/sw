// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <map>

#include "enums.h"
#include "remote.h"
#include "version.h"
#include "yaml.h"

#include <sw/support/filesystem.h>
#include <sw/support/http.h>

#include <primitives/sw/settings.h>

namespace sw
{

struct SW_MANAGER_API Settings
{
    // connection
    Remotes remotes{ get_default_remotes() };
    ProxySettings proxy;

    path storage_dir;

    // do not check for new cppan version
    bool disable_update_checks = false;
    bool can_update_packages_db = true;
    bool verify_all = false;

public:
    Settings();

    void load(const path &p, const SettingsType type);
    void load(const yaml &root, const SettingsType type);
    void save(const path &p) const;

    bool checkForUpdates() const;

private:
    void load_main(const yaml &root, const SettingsType type);

public:
    static Settings &get(SettingsType type);
    static Settings &get_system_settings();
    static Settings &get_user_settings();
    static Settings &get_local_settings();
    static void clear_local_settings();
};

}
