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

struct SW_CORE_API Input : RawInput
{
    Input(const path &, const SwContext &);
    Input(const PackageId &, const SwContext &);

    IDriver &getDriver() const { return *driver; }

    bool isChanged() const;
    void addEntryPoints(const std::vector<TargetEntryPointPtr> &);
    bool isLoaded() const;
    String getSpecification() const;
    const std::vector<TargetEntryPointPtr> &getEntryPoints() const { return eps; }

private:
    IDriver *driver = nullptr;
    // one input may have several eps
    // example: .yml frontend - 1 document, but multiple eps, one per package
    std::vector<TargetEntryPointPtr> eps;

    void init(const path &, const SwContext &);
    void init(const PackageId &, const SwContext &);
};

struct SW_CORE_API InputWithSettings
{
    InputWithSettings(const Input &);

    const std::set<TargetSettings> &getSettings() const;
    void addSettings(const TargetSettings &s);
    void clearSettings() { settings.clear(); }
    String getHash() const;
    const Input &getInput() const { return i; }

    [[nodiscard]]
    std::vector<ITargetPtr> load(SwBuild &) const;

protected:
    const Input &i;
    std::set<TargetSettings> settings;
};

} // namespace sw
