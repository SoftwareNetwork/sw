/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

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

struct SW_CORE_API Input
{
    using EntryPointsVector = std::vector<TargetEntryPointPtr>;

    Input(const IDriver &, const path &, InputType);
    virtual ~Input();

    void load(SwContext &);

    virtual std::unique_ptr<Specification> getSpecification() const = 0;

    // used for batch loading inputs (if applicable)
    const IDriver &getDriver() const { return driver; }

    /// allow to load several inputs via driver
    virtual bool isBatchLoadable() const = 0;
    /// allow to throw input->load() into thread pool
    virtual bool isParallelLoadable() const = 0;

    void setOutdated(bool b) { outdated = b; }
    /*virtual */bool isOutdated() const;
    bool isLoaded() const;
    const EntryPointsVector &getEntryPoints() const;

    size_t getHash() const;
    void setHash(size_t);

    InputType getType() const { return type; }
    path getPath() const;

    // same input may be used to load multiple packages
    // they all share same prefix
    std::pair<PackageIdSet, int> getPackages() const;
    void addPackage(const LocalPackage &);

    bool operator==(const Input &rhs) const;
    bool operator<(const Input &rhs) const;

protected:
    virtual void setEntryPoints(const EntryPointsVector &in);

private:
    InputType type;
    path p;
    PackageIdSet pkgs;
    int prefix = -1;
    //
    const IDriver &driver;
    // one input may have several eps
    // example: .yml frontend - 1 document, but multiple eps, one per package
    EntryPointsVector eps;
    size_t hash = 0;
    bool outdated = true;

    virtual EntryPointsVector load1(SwContext &) = 0;
};

static_assert(!std::is_copy_constructible_v<Input>, "must not be copied");
static_assert(!std::is_copy_assignable_v<Input>, "must not be copied");

struct SW_CORE_API InputWithSettings
{
    InputWithSettings(Input &);

    const std::set<TargetSettings> &getSettings() const;
    void addSettings(const TargetSettings &s);
    void clearSettings() { settings.clear(); }
    String getHash() const;
    Input &getInput() { return i; }
    const Input &getInput() const { return i; }

    [[nodiscard]]
    std::vector<ITargetPtr> loadTargets(SwBuild &) const;

protected:
    Input &i;
    std::set<TargetSettings> settings;
};

} // namespace sw
