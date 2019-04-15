// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "package.h"

/*

namespace sw
{

struct Remote;

struct ExtendedPackageData : Package
{
    db::PackageVersionId id = 0;
    String hash;
    PackageVersionGroupNumber group_number = 0;
    PackageVersionGroupNumber group_number_from_lock_file = 0;
    int prefix = 2;
    const Remote *remote = nullptr;
    bool local_override = false;
    bool from_lock_file = false;
};

} // namespace sw

namespace std
{

template<> struct hash<::sw::ExtendedPackageData>
{
    size_t operator()(const ::sw::ExtendedPackageData &p) const
    {
        return std::hash<::sw::PackageId>()(p);
    }
};

} // namespace std

namespace sw
{

struct DownloadDependency1 : ExtendedPackageData
{
    // own data (private)
    VersionRange range;
    bool installed = false; // manually installed
};

struct DownloadDependency : DownloadDependency1
{
    using IdDependencies = std::unordered_map<db::PackageVersionId, DownloadDependency>;
    using DbDependencies = std::unordered_map<String, DownloadDependency1>;
    using IdDependenciesSet = std::unordered_set<db::PackageVersionId>;
    using Dependencies = std::unordered_set<ExtendedPackageData>;

    // this prevents us having deps on two different versions of some package
    // TODO: reconsider
    DbDependencies db_dependencies;

public:
    void setDependencyIds(const IdDependenciesSet &ids)
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
                throw SW_RUNTIME_ERROR("cannot find dep by id");
            auto dep = i->second;
            //dep.createNames();
            dependencies.insert(dep);
        }
        dependencies.erase(*this); // erase self
    }

private:
    IdDependenciesSet id_dependencies;
    Dependencies dependencies;
};

using IdDependencies = DownloadDependency::IdDependencies;

}

namespace std
{

template<> struct hash<::sw::DownloadDependency>
{
    size_t operator()(const ::sw::DownloadDependency &p) const
    {
        return std::hash<::sw::ExtendedPackageData>()(p);
    }
};

} // namespace std

*/
