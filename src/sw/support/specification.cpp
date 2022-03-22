// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2019-2020  Egor Pugin <egor.pugin@gmail.com>

#include "specification.h"

#include "hash.h"

#include <boost/algorithm/string.hpp>
#include <nlohmann/json.hpp>

namespace sw
{

static void prepare_spec_file(String &build_script)
{
    // fancy prepare
    boost::trim(build_script);
    if (!build_script.empty())
        build_script += "\n";
}

String SpecificationFile::read(const path &p)
{
    if (p.empty())
        throw SW_RUNTIME_ERROR("Empty path");
    return read_file(p);
}

void SpecificationFile::read()
{
    if (contents)
        return;
    setContents(read(absolute_path));
}

const String &SpecificationFile::getContents()
{
    read();
    return *contents;
}

const String &SpecificationFile::getContents() const
{
    if (!contents)
        throw SW_RUNTIME_ERROR("No contents loaded");
    return *contents;
}

void SpecificationFile::setContents(const String &s)
{
    contents = s;
    prepare_spec_file(*contents);
}

void SpecificationFiles::addFile(const path &relative_path, const path &abspath, const std::optional<String> &contents)
{
    if (relative_path.is_absolute())
        throw SW_RUNTIME_ERROR("Not a relative path: " + to_string(relative_path));
    data[relative_path] = { abspath, contents };
}

void SpecificationFiles::addFile(const path &relative_path, const String &contents)
{
    if (relative_path.is_absolute())
        throw SW_RUNTIME_ERROR("Not a relative path: " + to_string(relative_path));
    data[relative_path] = { {}, contents };
}

fs::file_time_type SpecificationFiles::getLastWriteTime() const
{
    auto lwt = fs::file_time_type::min();
    for (auto &[_, f] : data)
        lwt = std::max(lwt, fs::last_write_time(f.absolute_path));
    return lwt;
}

void SpecificationFiles::read()
{
    for (auto &[_, f] : data)
        f.read();
}

void SpecificationFiles::write(const path &rootdir) const
{
    for (auto &[rel, f] : data)
        write_file(rootdir / rel, f.getContents());
}

nlohmann::json SpecificationFiles::toJsonWithoutContents() const
{
    nlohmann::json j;
    for (auto &[rel, f] : data)
    {
        nlohmann::json jf;
        jf["path"] = normalize_path(rel).u8string();
        j.push_back(jf);
    }
    return j;
}

nlohmann::json SpecificationFiles::toJson() const
{
    nlohmann::json j;
    for (auto &[rel, f] : data)
    {
        nlohmann::json jf;
        jf["path"] = normalize_path(rel).u8string();
        jf["contents"] = f.getContents();
        j.push_back(jf);
    }
    return j;
}

SpecificationFiles SpecificationFiles::fromJson(nlohmann::json &j)
{
    SpecificationFiles f;
    for (auto &v : j)
    {
        if (!v.contains("contents"))
            throw SW_RUNTIME_ERROR("No contents in json");
        f.addFile(v["path"].get<String>(), v["contents"].get<String>());
    }
    return f;
}

SpecificationFiles SpecificationFiles::fromJson(nlohmann::json &j, const path &rootdir)
{
    SpecificationFiles f;
    for (auto &v : j)
    {
        path rel = v["path"].get<String>();
        f.addFile(rel, rootdir / rel);
    }
    return f;
}

} // namespace sw
