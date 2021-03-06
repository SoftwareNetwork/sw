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
#if PRIMITIVES_FS_USE_UTF8_PATH_STRINGS
        files_map[(const char8_t *)f.c_str()] = (const char8_t *)t.get<std::string>().c_str();
#else
        files_map[f.c_str()] = t.get<std::string>().c_str();
#endif
    for (auto &v : j["dependencies"])
        dependencies.emplace(UnresolvedPackageName{ v.get<std::string>() });
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
    return PackageId{ {prefix / id.getName().getPath(), id.getName().getVersion()},{} };
}

void PackageData::applyPrefix(const PackagePath &prefix)
{
    id = getPackageId(prefix);

    // also fix deps
    decltype(dependencies) deps2;
    for (auto &d : dependencies)
    {
        if (d.getPath().isAbsolute())
            deps2.insert(UnresolvedPackageName{ d.getPath(), d.getRange() });
        else
            deps2.insert(UnresolvedPackageName{ prefix / d.getPath(), d.getRange() });
    }
    dependencies = deps2;
}

void PackageData::applyVersion()
{
    source->apply([this](auto &&s) { return id.getName().getVersion().format(s); });
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
