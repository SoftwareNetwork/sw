// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <sw/builder/sw_context.h>

#include <unordered_map>

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

struct Input;

struct IDriver
{
    virtual ~IDriver() = 0;

    virtual PackageId getPackageId() const = 0;
    //virtual FilesOrdered getAvailableFrontendConfigFilenames() const = 0;

    virtual bool canLoad(const Input &) const = 0;
    virtual void load(const std::set<Input> &) = 0;
    // prepare()
    // getCommands()
};

//struct INativeDriver : IDriver {}; // ?

struct ITarget
{
    virtual ~ITarget() = 0;

    // get deps
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

    // move to drivers?
    path source_dir;

    SwContext(const path &local_storage_root_dir);
    virtual ~SwContext();

    void registerDriver(std::unique_ptr<IDriver> driver);
    const Drivers &getDrivers() const { return drivers; }

    void build(const Inputs &inputs);
    // void load(); // only
    // void configure(); // = load() + save execution plan

    // move privates to impl?
private:
    using ProcessedInputs = std::set<Input>;

    Drivers drivers;

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
