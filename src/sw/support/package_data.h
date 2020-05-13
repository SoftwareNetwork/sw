// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "package_id.h"
#include "source.h"

#include <nlohmann/json_fwd.hpp>

namespace sw
{

namespace detail
{

struct SW_SUPPORT_API PackageData
{
    PackageId id;
    std::unique_ptr<Source> source;

    /// all package files mapped to disk file
    // from path on disk, to path in archive
    // optional for stored package
    // package may have different root dirs, source dir
    // so destination of file (to) may differ from the origin (from)
    std::unordered_map<path, path> files_map;

    /// all deps
    UnresolvedPackages dependencies;

public:
    PackageData(const PackageId &);
    PackageData(const String &json);
    // if passed by const ref, there is undefined behavior if key is missing
    // so we copy for reliability
    PackageData(nlohmann::json);

    PackageId getPackageId(const PackagePath &prefix = PackagePath()) const;
    void applyPrefix(const PackagePath &prefix);
    void applyVersion();
    void addFile(const path &root, const path &from, const path &to);
    nlohmann::json toJson() const;
};

// unused for now
// see more fields here https://github.com/aws/aws-sdk-cpp/blob/master/aws-cpp-sdk-s3/nuget/aws-cpp-sdk-s3.autopkg
struct ExtendedPackageData
{
    //
    // extended fields
    String name; // user-friendly name
    String type; // type: exe, lib, python lib etc.

    struct Author
    {
        String name;
        String email;
    } author;

    /// or license type with enum value from known licenses
    /// by default free if in org., pub. and proprietary if in com. or pvt.
    String license;

    // score from driver?

    struct Categories
    {
        String primary;
        String secondary;
        // other?
    } categories;

    Strings tags;
    Strings keywords;

    String summary;
    String description;

    String url;
    String bugs_url;

    // store fields

    FilesSorted icons;
    FilesOrdered previews;
    FilesOrdered screenshots;

    // languages (translations)

    // age = 0+, 3+, 12+, 16+, 18+, 21+, ...

    // internal service fields?

    //PackageType getType();
};

}

// generic pkg desc
using PackageDescription = detail::PackageData;
using PackageDescriptionPtr = std::unique_ptr<PackageDescription>;
using PackageDescriptionMap = std::unordered_map<PackageId, PackageDescriptionPtr>;

}
