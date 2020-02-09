// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "target.h"

namespace sw
{

struct Specification;
struct IDriver;
struct SwContext;

enum class InputType : uint8_t
{
    ///
    SpecificationFile,

    /// no input file, use some heuristics
    Directory,

    /// from some regular file
    InlineSpecification,

    /// only try to find spec file
    DirectorySpecificationFile,
};

struct RawInputData
{
    InputType type;
    path p;
};

struct SW_CORE_API RawInput : protected RawInputData
{
    InputType getType() const { return type; }
    path getPath() const;

    bool operator==(const RawInput &rhs) const;
    bool operator<(const RawInput &rhs) const;

protected:
    RawInput() = default;
};

struct SW_CORE_API Input : RawInput
{
    using EntryPointsVector = std::vector<TargetEntryPointPtr>;

    Input(/*const IDriver &, */const path &, InputType);
    virtual ~Input();

    void load(SwContext &);

    virtual std::unique_ptr<Specification> getSpecification() const = 0;

    // used for batch loading inputs (if applicable)
    //const IDriver &getDriver() const { return driver; }

    bool isChanged() const;
    bool isLoaded() const;
    //PackageVersionGroupNumber getGroupNumber() const;
    const EntryPointsVector &getEntryPoints() const { return eps; }

private:
    //const IDriver &driver;
    // one input may have several eps
    // example: .yml frontend - 1 document, but multiple eps, one per package
    EntryPointsVector eps;
    //PackageVersionGroupNumber gn = 0;

    virtual EntryPointsVector load1(SwContext &) = 0;
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
    std::vector<ITargetPtr> loadTargets(SwBuild &) const;

protected:
    const Input &i;
    std::set<TargetSettings> settings;
};

} // namespace sw
