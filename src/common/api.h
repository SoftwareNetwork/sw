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
#include "enums.h"

class ProjectPath;
struct Remote;
struct Version;

struct Api
{
    void add_project(const Remote &r, ProjectPath p, ProjectType t);
    void remove_project(const Remote &r, ProjectPath p);
    void add_version(const Remote &r, ProjectPath p, const String &cppan);
    void add_version(const Remote &r, ProjectPath p, const Version &vnew);
    void add_version(const Remote &r, ProjectPath p, const Version &vnew, const String &vold);
    void update_version(const Remote &r, ProjectPath p, const Version &v);
    void remove_version(const Remote &r, ProjectPath p, const Version &v);
    void get_notifications(const Remote &r, int n = 10);
    void clear_notifications(const Remote &r);
};
