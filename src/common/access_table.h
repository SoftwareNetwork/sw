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

#include "cppan_string.h"
#include "filesystem.h"

class AccessTable
{
public:
    AccessTable(const path &cfg_dir);
    ~AccessTable();

    bool updates_disabled() const;
    bool must_update_contents(const path &p) const;
    void update_contents(const path &p, const String &s) const;
    void write_if_older(const path &p, const String &s) const;
    void clear() const;
    void remove(const path &p) const;

    static void do_not_update_files(bool v);

private:
    path root_dir;

    bool isUnderRoot(path p) const;
};
