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

#include <unordered_map>
#include <unordered_set>

struct Remote;

struct ExtendedPackageData : Package
{
    ProjectVersionId id = 0;
    String hash;
    const Remote *remote = nullptr;
};

struct DownloadDependency : ExtendedPackageData
{
    using IdDependencies = std::unordered_map<ProjectVersionId, DownloadDependency>;
    using DbDependencies = std::unordered_map<String, ExtendedPackageData>;
    using Dependencies = std::unordered_map<Package, ExtendedPackageData>;

    // own data (private)
    DbDependencies db_dependencies;
    Dependencies dependencies;

    void setDependencyIds(const std::unordered_set<ProjectVersionId> &ids);
    void prepareDependencies(const IdDependencies &dd);

private:
    std::unordered_set<ProjectVersionId> id_dependencies;
};

using IdDependencies = DownloadDependency::IdDependencies;
