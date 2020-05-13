// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "package_data.h"

#include <nlohmann/json.hpp>

namespace sw
{

namespace detail
{

PackageData::PackageData(const PackageId &id)
    : id(id)
{
}

PackageData::PackageData(const String &json)
    : PackageData(nlohmann::json::parse(json))
{
}

PackageData::PackageData(const nlohmann::json &j)
    : PackageData(PackageId{ j["path"].get<std::string>(), j["version"].get<std::string>() })
{
    source = Source::load(j["source"]);
    for (auto &v : j["files"])
        files_map[fs::u8path(v["from"].get<std::string>())] = fs::u8path(v["to"].get<std::string>());
    for (auto &v : j["dependencies"])
        dependencies.emplace(v["path"].get<std::string>(), v["range"].get<std::string>());
}

String detail::PackageData::toJson() const
{
    SW_UNIMPLEMENTED;
}

PackageId PackageData::getPackageId(const PackagePath &prefix) const
{
    if (prefix.empty())
        return id;
    return { prefix / id.getPath(), id.getVersion() };
}

void PackageData::applyPrefix(const PackagePath &prefix)
{
    id = getPackageId(prefix);

    // also fix deps
    decltype(dependencies) deps2;
    for (auto &[p, r] : dependencies)
    {
        if (p.isAbsolute())
            deps2.insert(UnresolvedPackage{ p, r });
        else
            deps2.insert(UnresolvedPackage{ prefix / p, r });
    }
    dependencies = deps2;
}

void PackageData::applyVersion()
{
    source->applyVersion(id.getVersion());
}

void PackageData::addFile(const path &root, const path &from, const path &to)
{
    auto rd = normalize_path(root);
    auto sz = rd.size();
    if (rd.back() != '\\' && rd.back() != '/')
        sz++;
    auto s = normalize_path(from);
    if (s.find(rd) != 0)
        throw SW_RUNTIME_ERROR("bad file path: " + s);
    files_map[s.substr(sz)] = normalize_path(to);
}

} // namespace detail

} // namespace sw
