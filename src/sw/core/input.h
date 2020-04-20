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

#include <memory>
#include <vector>

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

// one input - one ep
struct SW_CORE_API Input
{
    using EntryPointPtr = std::unique_ptr<TargetEntryPoint>;

    Input(SwContext &, const IDriver &, std::unique_ptr<Specification>);
    Input(const Input &) = delete;
    virtual ~Input();

    void load();

    //
    Specification &getSpecification();
    const Specification &getSpecification() const;

    // used for batch loading inputs (if applicable)
    const IDriver &getDriver() const { return driver; }

    /// allow to load several inputs via driver
    virtual bool isBatchLoadable() const { return false; }
    /// allow to throw input->load() into thread pool
    virtual bool isParallelLoadable() const { return false; }

    bool isOutdated(const fs::file_time_type &) const;
    bool isLoaded() const;

    String getName() const;
    virtual size_t getHash() const;

    void setEntryPoint(EntryPointPtr);

    // no dry-run targets
    [[nodiscard]]
    std::vector<ITargetPtr> loadPackages(SwBuild &, const TargetSettings &, const PackageIdSet &allowed_packages, const PackagePath &prefix) const;

private:
    SwContext &swctx;
    const IDriver &driver;
    std::unique_ptr<Specification> specification;
    EntryPointPtr ep;

    virtual EntryPointPtr load1(SwContext &) = 0;
};

struct SW_CORE_API BuildInput
{
    BuildInput(Input &);

    // same input may be used to load multiple packages
    // they all share same prefix
    const PackageIdSet &getPackages() const { return pkgs; }
    PackagePath getPrefix() const { return prefix ? *prefix : PackagePath{}; }
    void addPackage(const LocalPackage &);

    // no dry-run targets
    [[nodiscard]]
    std::vector<ITargetPtr> loadPackages(SwBuild &, const TargetSettings &, const PackageIdSet &allowed_packages = {}) const;

    Input &getInput() { return i; }
    const Input &getInput() const { return i; }

    bool operator==(const BuildInput &rhs) const;
    bool operator!=(const BuildInput &rhs) const { return !operator==(rhs); }

private:
    PackageIdSet pkgs;
    std::optional<PackagePath> prefix;
    Input &i;
};

struct SW_CORE_API InputWithSettings
{
    InputWithSettings(const BuildInput &);

    const std::set<TargetSettings> &getSettings() const;
    void addSettings(const TargetSettings &s);
    void clearSettings() { settings.clear(); }
    String getHash() const;
    BuildInput &getInput() { return i; }
    const BuildInput &getInput() const { return i; }

    [[nodiscard]]
    std::vector<ITargetPtr> loadTargets(SwBuild &) const;

protected:
    BuildInput i;
    std::set<TargetSettings> settings;
};

} // namespace sw
