// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "package_data.h"

#include "source.h"

#include <nlohmann/json.hpp>

namespace sw
{

PackageId detail::PackageData::getPackageId(const PackagePath &prefix) const
{
    return {prefix / ppath, version};
}

void detail::PackageData::applyPrefix(const PackagePath &prefix)
{
    ppath = prefix / ppath;

    // also fix deps
    decltype(dependencies) deps2;
    for (auto &[p, r] : dependencies)
    {
        if (p.isAbsolute())
            deps2.insert(UnresolvedPackage{p,r});
        else
            deps2.insert(UnresolvedPackage{ prefix / p,r });
    }
    dependencies = deps2;
}

void detail::PackageData::applyVersion()
{
    source->applyVersion(version);
}

PackageDescription::PackageDescription(const std::string &s)
    : base(s)
{
}

JsonPackageDescription::JsonPackageDescription(const std::string &s)
    : PackageDescription(s)
{
}

detail::PackageData JsonPackageDescription::getData() const
{
    auto j = nlohmann::json::parse(*this);
    detail::PackageData d;
    d.source = Source::load(j["source"]);
    d.version = j["version"].get<std::string>();
    d.ppath = j["path"].get<std::string>();
    for (auto &v : j["files"])
        d.files_map[fs::u8path(v["from"].get<std::string>())] = fs::u8path(v["to"].get<std::string>());
    for (auto &v : j["dependencies"])
        d.dependencies.emplace(v["path"].get<std::string>(), v["range"].get<std::string>());
    return d;
}

YamlPackageDescription::YamlPackageDescription(const std::string &s)
    : PackageDescription(s)
{
}

detail::PackageData YamlPackageDescription::getData() const
{
    //const auto &s = *this;
    detail::PackageData d;
    throw SW_RUNTIME_ERROR("Not implemented");
}

}
