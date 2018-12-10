// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <sw/builder/driver.h>

#include <package_data.h>

#include <nlohmann/json.hpp>

namespace sw
{

PackageDescription::PackageDescription(const std::string &s)
    : base(s)
{
}

JsonPackageDescription::JsonPackageDescription(const std::string &s)
    : PackageDescription(s)
{
}

YamlPackageDescription::YamlPackageDescription(const std::string &s)
    : PackageDescription(s)
{
}

Drivers &getDrivers()
{
    static Drivers drivers;
    return drivers;
}

bool Driver::hasConfig(const path &dir) const
{
    return fs::exists(dir / getConfigFilename());
}

PackageScriptPtr Driver::fetch_and_load(const path &file_or_dir, bool parallel) const
{
    fetch(file_or_dir, parallel);
    return load(file_or_dir);
}

bool Driver::execute(const path &file_or_dir) const
{
    if (auto s = load(file_or_dir); s)
        return s->execute();
    return false;
}

PackageData JsonPackageDescription::getData() const
{
    auto j = nlohmann::json::parse(*this);
    PackageData d;
    d.source = load_source(j["source"]);
    d.version = j["version"].get<std::string>();
    d.ppath = j["path"].get<std::string>();
    for (auto &v : j["files"])
        d.files_map[fs::u8path(v["from"].get<std::string>())] = fs::u8path(v["to"].get<std::string>());
    for (auto &v : j["dependencies"])
        d.dependencies.emplace(v["path"].get<std::string>(), v["range"].get<std::string>());
    return d;
}

PackageData YamlPackageDescription::getData() const
{
    //const auto &s = *this;
    PackageData d;
    throw SW_RUNTIME_EXCEPTION("Not implemented");
}

} // namespace sw
