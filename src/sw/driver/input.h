// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <sw/core/target.h>

#include <memory>
#include <vector>

namespace sw
{

struct Specification;
struct IDriver;
struct SwContext;
struct Build;

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
struct SW_DRIVER_CPP_API Input
{
    Input(SwContext &, const IDriver &, std::unique_ptr<Specification>);
    Input(const Input &) = delete;
    virtual ~Input();

    virtual void load() = 0;

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
    virtual bool isLoaded() const = 0;

    String getName() const;
    virtual size_t getHash() const;

    /// load every target from input
    /// "local" mode
    /// no dry-run targets
    [[nodiscard]]
    virtual std::vector<ITargetPtr> loadPackages(Build &) const = 0;

    /// load specific package from input
    /// no dry-run targets
    [[nodiscard]]
    virtual ITargetPtr loadPackage(Build &, const Package &) const = 0;

protected:
    SwContext &swctx;
private:
    const IDriver &driver;
    std::unique_ptr<Specification> specification;
};

struct SW_DRIVER_CPP_API UserInput
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
