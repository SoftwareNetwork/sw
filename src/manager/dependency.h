// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "package.h"

namespace sw
{

struct Remote;

struct ExtendedPackageData : Package
{
    db::PackageVersionId id = 0;
    String hash;
    PackageVersionGroupNumber group_number = 0;
    int prefix = 2;
    const Remote *remote = nullptr;
};

} // namespace sw

namespace std
{

template<> struct hash<sw::ExtendedPackageData>
{
    size_t operator()(const sw::ExtendedPackageData &p) const
    {
        return std::hash<sw::PackageId>()(p);
    }
};

} // namespace std

namespace sw
{

struct DownloadDependency : public ExtendedPackageData
{
    using IdDependencies = std::unordered_map<db::PackageVersionId, DownloadDependency>;
    using DbDependencies = std::unordered_map<String, ExtendedPackageData>;
    using Dependencies = std::unordered_map<ExtendedPackageData, ExtendedPackageData>;

    // own data (private)
    VersionRange range;
    DbDependencies db_dependencies;

public:
    void setDependencyIds(const std::unordered_set<db::PackageVersionId> &ids)
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
    std::unordered_set<db::PackageVersionId> id_dependencies;
    Dependencies dependencies;
};

using IdDependencies = DownloadDependency::IdDependencies;

}
