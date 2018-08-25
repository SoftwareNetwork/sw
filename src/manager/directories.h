// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "enums.h"
#include "filesystem.h"

#include <primitives/sw/settings.h>

namespace sw
{

String getDataDir();
String getDataDirPrivate(const String &base = {});

struct Directories
{
    path storage_dir;
#define ADD_DIR(x) path storage_dir_##x
    ADD_DIR(bin);
    ADD_DIR(cfg);
    ADD_DIR(etc);
    //ADD_DIR(exp);
    ADD_DIR(lib);
#ifdef _WIN32
    ADD_DIR(lnk);
#endif
    ADD_DIR(obj);
    ADD_DIR(pkg);
    //ADD_DIR(src);
    ADD_DIR(tmp);
    //ADD_DIR(usr);
#undef ADD_DIR
    path build_dir;

    SettingsType storage_dir_type;
    SettingsType build_dir_type;

    bool empty() const { return storage_dir.empty(); }
    void update(const Directories &dirs, SettingsType type);

    void set_storage_dir(const path &p);
    void set_build_dir(const path &p);

    /*path get_include_dir() const;
    path get_local_dir() const;*/
    path get_static_files_dir() const;

private:
    SettingsType type{ SettingsType::Max };
};

SW_MANAGER_API
const Directories &getDirectories();

SW_MANAGER_API
const Directories &getUserDirectories();

}
