// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>

#include "package_data.h"

#include <nlohmann/json.hpp>
#include <primitives/yaml.h> // for Source::load

namespace sw
{

namespace detail
{

PackageData::PackageData(const PackageId &id, const PackageId &driver_id)
    : id(id), driver_id(driver_id)
{
}

PackageData::PackageData(const String &json)
    : PackageData(nlohmann::json::parse(json))
{
}

PackageData::PackageData(nlohmann::json j)
    : PackageData(PackageId{ j["package"].get<std::string>()}, PackageId{ j["driver"].get<std::string>() })
{
    source = Source::load(j["source"]);
    if (!source)
        throw SW_RUNTIME_ERROR("bad source");
    for (auto &[f,t] : j["files"].items())
        files_map[fs::u8path(f)] = fs::u8path(t.get<std::string>());
    for (auto &v : j["dependencies"])
        dependencies.emplace(v.get<std::string>());
    for (auto &v : j["signatures"])
    {
        Signature s;
        s.fingerprint = v["fingerprint"].get<std::string>();
        s.signature = v["signature"].get<std::string>();
        signatures.push_back(s);
    }
}

nlohmann::json detail::PackageData::toJson() const
{
    nlohmann::json j;
    j["package"] = id.toString();
    j["driver"] = driver_id.toString();
    source->save(j["source"]);
    for (auto &[f, t] : files_map)
        j["files"][to_string(normalize_path(f))] = to_string(normalize_path(t));
    for (auto &d : dependencies)
        j["dependencies"].push_back(d.toString());
    for (auto &s : signatures)
    {
        nlohmann::json js;
        js["fingerprint"] = s.fingerprint;
        js["signature"] = s.signature;
        j["signatures"].push_back(js);
    }
    return j;
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
    auto rd = to_printable_string(normalize_path(root));
    auto sz = rd.size();
    if (rd.back() != '\\' && rd.back() != '/')
        sz++;
    auto s = to_printable_string(normalize_path(from));
    if (s.find(rd) != 0)
        throw SW_RUNTIME_ERROR("bad file path: " + to_printable_string(s));
    files_map[s.substr(sz)] = normalize_path(to);
}

} // namespace detail

} // namespace sw
