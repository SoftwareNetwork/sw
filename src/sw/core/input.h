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
    Unspecified = 0,

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

struct SW_CORE_API Input
{
    Input(const path &, const SwContext &);
    Input(const PackageId &, const SwContext &);

    IDriver &getDriver() const { return *driver; }

    InputType getType() const { return type; }
    path getPath() const;
    PackageId getPackageId() const;

    bool isChanged() const;
    const std::set<TargetSettings> &getSettings() const;
    void addSettings(const TargetSettings &s);
    void clearSettings() { settings.clear(); }
    String getHash() const;

    bool operator<(const Input &rhs) const;

private:
    std::variant<path, PackageId> data;
    InputType type;
    IDriver *driver = nullptr;
    std::set<TargetSettings> settings;

    void init(const path &, const SwContext &);
    void init(const PackageId &, const SwContext &);
};

} // namespace sw
