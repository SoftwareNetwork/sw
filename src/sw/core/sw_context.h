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
    PackageId,

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

    virtual bool canLoad(const Input &) const { return false; }
    virtual void load(const Input &);
    // prepare()
    // getCommands()
};

//struct INativeDriver : IDriver {}; // ?

struct SW_CORE_API SwContext : SwBuilderContext
{
    using Drivers = std::map<PackageId, std::unique_ptr<IDriver>>;

    path source_dir;

    SwContext(const path &local_storage_root_dir);
    virtual ~SwContext();

    void registerDriver(std::unique_ptr<IDriver> driver);
    const Drivers &getDrivers() const { return drivers; }

    void build(const Strings &inputs);
    // void load(); // only
    // void configure(); // = load() + save execution plan

    // move privates to impl?
private:
    Drivers drivers;

    std::vector<Input> makeInputs(const Strings &inputs);

    void load(const path &file_or_dir);
    FilesOrdered getAvailableFrontendConfigFilenames() const;
    static std::optional<path> resolveConfig(const path &file_or_dir, const FilesOrdered &fe_s);
    static std::optional<path> findConfig(const path &dir, const FilesOrdered &fe_s);

    void build(const path &file_or_dir);
    void build(const Files &files_or_dirs);
    void build(const String &file_or_dir_or_package);

    friend struct Input;
};

struct Input
{
    String subject;
    // settings?

    Input(const String &subject, const SwContext &);

    InputType getType() const { return type; }
    IDriver &getDriver() { return *driver; }

private:
    InputType type;
    IDriver *driver = nullptr;
};

} // namespace sw
