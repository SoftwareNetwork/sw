// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "target.h"

namespace sw
{

struct IDriver;
struct SwContext;

enum class InputType : int32_t
{
    /// drivers may use their own methods for better loading packages
    /// rather than when direct spec file provided
    InstalledPackage,

    ///
    SpecificationFile,

    /// from some regular file
    InlineSpecification,

    /// only try to find spec file
    DirectorySpecificationFile,

    /// no input file, use any heuristics
    Directory,
};

struct SW_CORE_API RawInput
{
    InputType getType() const { return type; }
    path getPath() const;
    PackageId getPackageId() const;

    bool operator==(const RawInput &rhs) const;
    bool operator<(const RawInput &rhs) const;

protected:
    std::variant<path, PackageId> data;
    InputType type;

    RawInput() = default;
};

struct SW_CORE_API InputWithSettings : RawInput
{
    const std::set<TargetSettings> &getSettings() const;
    void addSettings(const TargetSettings &s);
    void clearSettings() { settings.clear(); }
    String getHash() const;

protected:
    std::set<TargetSettings> settings;

    InputWithSettings() = default;
};

struct SW_CORE_API Input : InputWithSettings
{
    Input(const path &, const SwContext &);
    Input(const PackageId &, const SwContext &);

    IDriver &getDriver() const { return *driver; }

    bool isChanged() const;
    void addEntryPoint(const TargetEntryPointPtr &);
    void load(SwBuild &);
    String getSpecification() const;

private:
    IDriver *driver = nullptr;
    std::vector<TargetEntryPointPtr> eps;

    void init(const path &, const SwContext &);
    void init(const PackageId &, const SwContext &);
};

} // namespace sw
