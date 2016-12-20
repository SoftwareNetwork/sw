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
