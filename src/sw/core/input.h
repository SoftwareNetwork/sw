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

/*struct IInput
{
    virtual ~IInput() = 0;

    // used for batch loading inputs (if applicable)
    virtual IDriver &getDriver() const = 0;

    //bool isChanged() const;
    //bool isLoaded() const; ?
    //std::unique_ptr<Specification> getSpecification() const;
    //const std::vector<TargetEntryPointPtr> &getEntryPoints() const;
    //PackageVersionGroupNumber getGroupNumber() const; ?
    //void addEntryPoints(const std::vector<TargetEntryPointPtr> &);
};*/

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
    /// determine input type
    //Input(IDriver &, const path &);
    /// forced input type
    Input(const IDriver &, const path &, InputType);

    const IDriver &getDriver() const { return driver; }

    bool isChanged() const;
    void addEntryPoints(const std::vector<TargetEntryPointPtr> &);
    bool isLoaded() const;
    std::unique_ptr<Specification> getSpecification() const;
    PackageVersionGroupNumber getGroupNumber() const;
    const std::vector<TargetEntryPointPtr> &getEntryPoints() const { return eps; }

    //bool operator==(const Input &rhs) const;
    //bool operator<(const Input &rhs) const;

private:
    const IDriver &driver;
    // one input may have several eps
    // example: .yml frontend - 1 document, but multiple eps, one per package
    std::vector<TargetEntryPointPtr> eps;
    //PackageVersionGroupNumber gn = 0;

    void init(const path &, const SwContext &);
    //void init(const LocalPackage &, const SwContext &);

    bool findDriver(InputType t, const SwContext &);
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
