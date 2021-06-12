// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "package.h"

#include <primitives/filesystem.h>

namespace sw
{

struct Package;
struct Input;
//struct SwContext;
struct package_loader;
struct package_transform;
struct SwBuild;
enum class InputType : uint8_t;

struct SW_CORE_API IDriver : physical_package
{
    virtual ~IDriver();

    /// Detect available inputs on path.
    //virtual std::vector<std::unique_ptr<Input>> detectInputs(const path &abspath) const = 0;

    /// Detect available inputs of specified type on path.
    /// Only for local inputs
    //virtual std::vector<std::unique_ptr<Input>> detectInputs(const path &abspath, InputType) const = 0;

    /// only for package inputs
    //virtual std::unique_ptr<Input> getInput(const Package &) const = 0;

    /// Optimized input loading in a batch.
    /// Inputs are unique and non null.
    /// Inputs will receive their entry points.
    //virtual void loadInputsBatch(const std::set<Input*> &) const = 0;

    /// returns driver capabilities
    //virtual uint64_t getCapabilities() const { return 0; }

    //virtual std::vector<std::unique_ptr<Input>> getPredefinedInputs() const { return {}; }
    // add predefined targets etc.
    //virtual void setupBuild(SwBuild &) const {}

    //using package_loader_ptr = std::unique_ptr<package_loader>;
    virtual package_loader *load_package(const Package &) = 0;
    virtual std::vector<package_loader *> load_packages(const path &) = 0;
};

} // namespace sw
