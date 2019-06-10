// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "driver.h"
#include "target.h"

#include <sw/builder/sw_context.h>
#include <sw/manager/package_data.h>

namespace sw
{

enum class InputType : int32_t
{
    Unspecified = 0,

    ///
    SpecificationFile,

    /// from some regular file
    InlineSpecification,

    /// only try to find spec file
    DirectorySpecificationFile,

    /// no input file, use any heuristics
    Directory,
};

// represents user request (if possible) returned from sw context
struct SW_CORE_API Request
{

};

struct SW_CORE_API SwContext : SwBuilderContext
{
    struct InputVariant : std::variant<String, path, PackageId>
    {
        using Base = std::variant<String, path, PackageId>;
        using Base::Base;
        InputVariant(const char *p) : Base(std::string(p)) {}
    };

    // unique
    struct Inputs : std::set<InputVariant>
    {
        using Base = std::set<InputVariant>;
        using Base::Base; // separate types
        Inputs(const Strings &inputs) // dynamic detection
        {
            for (auto &i : inputs)
                insert(i);
        }
    };

    using Drivers = std::map<PackageId, std::unique_ptr<IDriver>>;

    // move to drivers? remove?
    path source_dir;

    SwContext(const path &local_storage_root_dir);
    virtual ~SwContext();

    void registerDriver(std::unique_ptr<IDriver> driver);
    const Drivers &getDrivers() const { return drivers; }

    // TODO: return some build object
    std::unique_ptr<Request> load(const Inputs &inputs);
    void build(const Inputs &inputs);
    // void configure(); // = load() + save execution plan
    bool prepareStep();
    PackageDescriptionMap getPackages() const;

    TargetMap &getTargets() { return targets; }
    const TargetMap &getTargets() const { return targets; }

private:
    using ProcessedInputs = std::set<Input>;

    Drivers drivers;
    std::map<IDriver *, ProcessedInputs> active_drivers;
    TargetMap targets;

    ProcessedInputs makeInputs(const Inputs &inputs);
    void load(const ProcessedInputs &inputs);
};

struct Input
{
    Input(const String &, const SwContext &);
    Input(const path &, const SwContext &);
    Input(const PackageId &, const SwContext &);

    IDriver &getDriver() const { return *driver; }
    InputType getType() const { return type; }
    path getPath() const;

    bool operator<(const Input &rhs) const { return subject < rhs.subject; }

private:
    path subject;
    InputType type;
    IDriver *driver = nullptr;
    // settings?

    void init(const String &, const SwContext &);
    void init(const path &, const SwContext &);
    void init(const PackageId &);
};

} // namespace sw
