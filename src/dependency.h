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

#include "common.h"
#include "project_path.h"

struct Dependency
{
    using Dependencies = std::map<String, Dependency>;

    ProjectPath package;
    Version version;
    ProjectFlags flags;

    path getPackageDir(path base) const
    {
        return base / package.toString() / version.toString();
    }
};

using Dependencies = Dependency::Dependencies;

struct DownloadDependency : public Dependency
{
    using DownloadDependencies = std::map<int, DownloadDependency>;

    String md5;

private:
    std::set<int> dependencies;

public:
    DownloadDependencies *map_ptr = nullptr;

public:
    void setDependencyIds(const std::set<int> &ids) { dependencies = ids; }

    Dependencies getDirectDependencies() const
    {
        Dependencies deps;
        for (auto d : dependencies)
        {
            auto &dep = (*map_ptr)[d];
            deps[dep.package.toString()] = dep;
        }
        deps.erase(package.toString()); // erase self
        return deps;
    }

    Dependencies getIndirectDependencies(const Dependencies &known_deps = Dependencies()) const
    {
        Dependencies deps = known_deps;
        for (auto d : dependencies)
        {
            auto &dep = (*map_ptr)[d];
            auto p = deps.insert({ dep.package.toString(), dep });
            if (p.second)
            {
                auto id = dep.getIndirectDependencies(deps);
                deps.insert(id.begin(), id.end());
            }
        }
        if (known_deps.empty())
        {
            // first call in a chain

            // erase direct deps
            for (auto &d : getDirectDependencies())
                deps.erase(d.first);

            // erase self
            deps.erase(package.toString());
        }
        return deps;
    }

    // custom package dir can be used to apply project-wide patches
    // that won't hit any system storage
    /*PackagesDirType package_dir_type{ PackagesDirType::None };
    path package_dir;
    std::vector<path> patches;
    //

    PackagesDirType get_package_dir_type(PackagesDirType default_type)
    {
    if (package_dir_type != PackagesDirType::None)
    return package_dir_type;
    return default_type;
    }*/
};

using DownloadDependencies = DownloadDependency::DownloadDependencies;
