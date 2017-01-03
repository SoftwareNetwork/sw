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

#pragma once

#include "enums.h"
#include "filesystem.h"

struct Directories
{
    path storage_dir;
#define ADD_DIR(x) path storage_dir_##x
    ADD_DIR(bin);
    ADD_DIR(cfg);
    ADD_DIR(etc);
    ADD_DIR(exp);
    ADD_DIR(lib);
#ifdef _WIN32
    ADD_DIR(lnk);
#endif
    ADD_DIR(obj);
    ADD_DIR(src);
    ADD_DIR(tmp);
    ADD_DIR(usr);
#undef ADD_DIR
    path build_dir;

    SettingsType storage_dir_type;
    SettingsType build_dir_type;

    bool empty() const { return storage_dir.empty(); }
    void update(const Directories &dirs, SettingsType type);

    void set_storage_dir(const path &p);
    void set_build_dir(const path &p);

    path get_include_dir() const;
    path get_local_dir() const;
    path get_static_files_dir() const;

private:
    SettingsType type{ SettingsType::Max };
};

extern Directories directories;

const Directories &get_user_directories();
