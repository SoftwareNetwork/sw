// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2017-2020  Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <nlohmann/json_fwd.hpp>
#include <primitives/filesystem.h>

#include <optional>

namespace sw
{

struct SW_SUPPORT_API SpecificationFile
{
    path absolute_path;
    std::optional<String> contents;

    //SpecificationFile(const String &contents);
    //SpecificationFile(const path &abspath, const String &);

    void read();
    const String &getContents();
    const String &getContents() const;
    void setContents(const String &contents);

private:
    static String read(const path &);
};

struct SW_SUPPORT_API SpecificationFiles
{
    using relative_path = path;

    // For inline spec we may pass virtual file name and actual contents
    // that cannot be read from fs.
    // Example, inline cppan.yml: addFile(someroot, "cppan.yml", extracted yml contents from comments);
    // relative_path - path relative to package root, may be virtual, but valid!
    // absolute_path - path on disk, may differ from relative, example: main.cpp where we take inline cppan.yml from
    void addFile(const path &relpath, const path &abspath, const std::optional<String> &contents = std::optional<String>{});
    void addFile(const path &relpath, const String &contents);

    auto &getData() { return data; }
    const auto &getData() const { return data; }
    fs::file_time_type getLastWriteTime() const;
    void read();
    void write(const path &rootdir) const;

    nlohmann::json toJson() const;
    nlohmann::json toJsonWithoutContents() const;
    static SpecificationFiles fromJson(nlohmann::json &);
    static SpecificationFiles fromJson(nlohmann::json &, const path &rootdir);

private:
    std::map<relative_path, SpecificationFile> data;
};

} // namespace sw
