// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

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

    //virtual bool isPredefinedInput() const { return false; }

    bool isOutdated(const fs::file_time_type &) const;
    bool isLoaded() const;

    String getName() const;
    virtual size_t getHash() const;

    void setEntryPoint(EntryPointPtr);

    // no dry-run targets
    [[nodiscard]]
    std::vector<ITargetPtr> loadPackages(SwBuild &, const PackageSettings &, const AllowedPackages &allowed_packages, const PackagePath &prefix) const;

private:
    SwContext &swctx;
    const IDriver &driver;
    std::unique_ptr<Specification> specification;
    EntryPointPtr ep;

    virtual EntryPointPtr load1(SwContext &) = 0;
};

struct SW_CORE_API UserInput
{
    UserInput(Input &);

    const std::unordered_set<PackageSettings> &getSettings() const;
    void addSettings(const PackageSettings &s);
    //void clearSettings() { settings.clear(); }
    String getHash() const;
    Input &getInput() { return i; }
    //const Input &getInput() const { return i; }

    //[[nodiscard]]
    //std::vector<ITargetPtr> loadPackages(SwBuild &) const;

protected:
    Input &i;
    std::unordered_set<PackageSettings> settings;
};

} // namespace sw
