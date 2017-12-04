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

#include "dependency.h"
#include "package_store.h"

class Resolver
{
public:
    using Dependencies = std::unordered_map<Package, DownloadDependency>;

public:
    PackagesMap resolved_packages;

    void resolve_dependencies(const Packages &deps);
    void resolve_and_download(const Package &p, const path &fn);
    void assign_dependencies(const Package &p, const Packages &deps); // why such name?

private:
    Dependencies download_dependencies_;
    const Remote *current_remote = nullptr;
    bool query_local_db = true;

    void read_configs();
    void download_and_unpack();
    void post_download();
    void prepare_config(PackageStore::PackageConfigs::value_type &cc);
    void read_config(const ExtendedPackageData &d);

    void resolve(const Packages &deps, std::function<void()> resolve_action);
    void download(const ExtendedPackageData &d, const path &fn);
};

void resolve_and_download(const Package &p, const path &fn);
std::tuple<Package, PackagesSet> resolve_dependency(const String &d);
PackagesMap resolve_dependencies(const Packages &deps);
