// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "package.h"
#include "source.h"

// !!! see more fields here https://github.com/aws/aws-sdk-cpp/blob/master/aws-cpp-sdk-s3/nuget/aws-cpp-sdk-s3.autopkg

namespace sw
{

namespace detail
{

// TODO: rename PackageData

/** internal data structure

* variants:
* local package: source is local and files are present
* remote to be downloaded: source present
* remote already downloaded: source and files are present
*/
struct PackageData
{
    // basic fields, but not mandatory

    Source source;

    /// by default is 0.0.1
    Version version;

    /// all package files mapped to disk file
    // from path on disk, to path in archive
    // optional for stored package
    std::unordered_map<path, path> files_map;

    /// all deps, does not show conditionals
    UnresolvedPackages dependencies;

    // extended fields

    // basic?
    PackagePath ppath;

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

    PackageId getPackageId(const PackagePath &prefix = PackagePath()) const;
    void applyPrefix(const PackagePath &prefix);
    void checkSourceAndVersion();
};

}

/**
* generic pkg desc
*/
struct SW_MANAGER_API PackageDescription : std::string
{
    using base = std::string;

    PackageDescription(const std::string &);
    virtual ~PackageDescription() = default;

    /// convert to internal data
    virtual detail::PackageData getData() const = 0;
};

using PackageDescriptionPtr = std::unique_ptr<PackageDescription>;
using PackageDescriptionMap = std::unordered_map<PackageId, PackageDescriptionPtr>;

struct SW_MANAGER_API JsonPackageDescription : PackageDescription
{
    JsonPackageDescription(const std::string &);
    virtual ~JsonPackageDescription() = default;

    detail::PackageData getData() const override;
};

struct SW_MANAGER_API YamlPackageDescription : PackageDescription
{
    YamlPackageDescription(const std::string &);
    virtual ~YamlPackageDescription() = default;

    detail::PackageData getData() const override;
};

SW_MANAGER_API
void checkSourceAndVersion(Source &s, const Version &v);

}
