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

#include <map>
#include <set>

#include "checks.h"
#include "cppan_string.h"
#include "filesystem.h"
#include "package.h"
#include "project.h"
#include "project_path.h"
#include "yaml.h"

#define CONFIG_ROOT "/etc/cppan/"

struct Config
{
    // projects settings
    Checks checks; // move to proj?

public:
    Config();
    Config(const path &p);

    void load(yaml root);
    void load(const path &p);
    void reload(const path &p);

    void load_current_config();

    void process(const path &p = path()) const;
    void post_download() const;

    void clear_vars_cache() const;

    auto &getProjects() { return projects; }
    auto &getProjects() const { return projects; }
    Project &getDefaultProject();
    const Project &getDefaultProject() const;
    Project &getProject(const String &p) const;

    void setPackage(const Package &pkg);

    // split current conf into several with only one project in it
    std::vector<Config> split() const;

    Packages getFileDependencies() const; // from file

private:
    Projects projects;
    path dir; // cwd

    void addDefaultProject();

public:
    bool defaults_allowed = true;
    bool allow_relative_project_names = false;
    bool allow_local_dependencies = false;

    // we create this project for the first time (downloaded, locally created etc.)
    bool created = false;

    // current package
    Package pkg;
};
