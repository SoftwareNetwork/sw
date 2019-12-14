// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <primitives/filesystem.h>

#include <optional>

namespace sw
{

struct PackageId;
struct RawInput;
struct RawInputData;
struct SwContext;
struct TargetEntryPoint;
using TargetEntryPointPtr = std::shared_ptr<TargetEntryPoint>;

struct SW_CORE_API Specification
{
    void addFile(const path &relative_path, const String &contents);
    int64_t getHash() const;

//private:
    std::map<path, String> files;
};

struct SW_CORE_API IDriver
{
    using EntryPointsVector1 = std::vector<TargetEntryPointPtr>;
    using EntryPointsVector = std::vector<EntryPointsVector1>;

    virtual ~IDriver() = 0;

    /// Test if driver is able to load this path or package.
    ///
    /// Input types - all except InputType::InstalledPackage.
    /// Path in raw input is absolute.
    ///
    /// On success path is returned.
    /// It is changed for InputType::DirectorySpecificationFile
    /// and left unchanged for other input types.
    ///
    virtual std::optional<path> canLoadInput(const RawInput &) const = 0;

    /// Create entry points for inputs.
    /// inputs are unique and non null
    /// inputs will receive their entry points
    /// result is number of vectors of entry points equal to inputs, in the same order
    /// one input may provide several entry points (yml)
    /// we return shared points because we cannot load them into context because package ids is not known in advance
    /// (in case of loading not installed package)
    /// if entry points were already loaded (like for installed packages), internal vector may be empty
    ///
    [[nodiscard]]
    virtual EntryPointsVector createEntryPoints(SwContext &, const std::vector<RawInput> &) const = 0;

    /// get raw specification
    /// complex return value?
    /// for example set of files
    virtual std::unique_ptr<Specification> getSpecification(const RawInput &) const = 0;

    ///
    //virtual PackageVersionSpecificationHash getHash(const RawInput &) const;
    virtual int64_t getGroupNumber(const RawInput &) const;

    // get features()?
};

} // namespace sw
