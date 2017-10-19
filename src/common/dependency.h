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
#include "package.h"

#include <set>

struct Remote;

struct DownloadDependency : public Package
{
    using IdDependencies = std::unordered_map<ProjectVersionId, DownloadDependency>;
    using DbDependencies = std::unordered_map<String, DownloadDependency>;
    using Dependencies = std::unordered_map<Package, DownloadDependency>;

    // extended data
    ProjectVersionId id = 0;
    String hash;

    // own data (private)
    const Remote *remote = nullptr;
    DbDependencies db_dependencies;

public:
    void setDependencyIds(const std::unordered_set<ProjectVersionId> &ids)
    {
        id_dependencies = ids;
    }

    Dependencies getDependencies() const
    {
        return dependencies;
    }

    void prepareDependencies(const IdDependencies &dd)
    {
        for (auto d : id_dependencies)
        {
            auto i = dd.find(d);
            if (i == dd.end())
                throw std::runtime_error("cannot find dep by id");
            auto dep = i->second;
            dep.createNames();
            dependencies[dep] = dep;
        }
        dependencies.erase(*this); // erase self
    }

private:
    std::unordered_set<ProjectVersionId> id_dependencies;
    Dependencies dependencies;
};

using IdDependencies = DownloadDependency::IdDependencies;
