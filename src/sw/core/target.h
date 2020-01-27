// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "settings.h"

#include <sw/builder/command.h>
#include <sw/builder/node.h>
#include <sw/manager/package_version_map.h>
#include <sw/manager/source.h>

#include <any>
#include <variant>

namespace sw
{

struct ITarget;
struct SwBuild;

struct SW_CORE_API IDependency
{
    virtual ~IDependency() = 0;

    virtual const TargetSettings &getSettings() const = 0;
    virtual UnresolvedPackage getUnresolvedPackage() const = 0;
    virtual bool isResolved() const = 0;
    virtual void setTarget(const ITarget &) = 0;
    virtual const ITarget &getTarget() const = 0;
};

/// Very basic interface for targets and must be very stable.
/// You won't be operating much using it.
/// Instead, text interface for querying data will be available.
struct SW_CORE_API ITarget : ICastable
{
    virtual ~ITarget() = 0;

    //
    // basic info/description section
    //

    virtual const PackageId &getPackage() const = 0;

    // merge getSource(), getFiles() and getDependencies() into single call returning json/target settings?
    // into getDescription() or getInformation() or something similar

    // how to fetch package
    ///
    virtual const Source &getSource() const = 0;

    /// all input (for creating an input package) non-generated files under base source dir
    virtual Files getSourceFiles() const = 0;
    // better to call getFiles() because source files = files for specific configuration
    // and such sets may differ for different configs
    // getFiles()? some langs does not have 'sources'
    // also add get binary files?

    /// get all direct dependencies
    virtual std::vector<IDependency *> getDependencies() const = 0;

    //
    // build section
    //

    /// prepare target for building
    /// returns true if target is not fully prepared yet
    virtual bool prepare() = 0;

    // get commands for building
    ///
    virtual Commands getCommands() const = 0;

    // get tests
    // reconsider?
    // get using settings?
    virtual Commands getTests() const = 0;

    //
    // extended info section
    // configuration specific
    // (build information, output information)
    //

    /// final (output) configuration
    /// available before prepare or after?
    /// round trips
    //virtual const TargetSettings &getConfiguration() const = 0;

    /// input settings
    /// do not round trip
    virtual const TargetSettings &getSettings() const = 0;

    // settings for consumers (targets) and users?
    // output command or module name
    ///
    virtual const TargetSettings &getInterfaceSettings() const = 0;

    // get binary settings, get doc settings?
    // String get package settings(); // json coded or whatever via interface?
    // String getDescription()

    // returns prepared command for executing
    // result may be null
    //
    // getCommand()

    // result may be null
    // getLoadableModule()
};

// shared_ptr for vector storage
using ITargetPtr = std::shared_ptr<ITarget>;

/*struct INativeTarget : ITarget
{
    // header only does not provide these
    virtual path getOutputFile() const = 0;
    virtual path getImportLibrary() const = 0;
    // get cl args?
    // get link args?
};*/

struct SW_CORE_API TargetContainer
{
    using Base = std::vector<ITargetPtr>;

    // find target with equal settings
    Base::iterator findEqual(const TargetSettings &);
    Base::const_iterator findEqual(const TargetSettings &) const;

    // find target with equal subset of provided settings
    // findEqualSubset()
    Base::iterator findSuitable(const TargetSettings &);
    Base::const_iterator findSuitable(const TargetSettings &) const;

    void push_back(const ITargetPtr &);

    void clear();
    bool empty() const;

    auto begin() { return targets.begin(); }
    auto end() { return targets.end(); }

    auto begin() const { return targets.begin(); }
    auto end() const { return targets.end(); }

    Base::iterator erase(Base::iterator begin, Base::iterator end);

private:
    std::vector<ITargetPtr> targets;
};

namespace detail
{

struct SimpleExpectedErrorCode
{
    int ec;
    String message;

    SimpleExpectedErrorCode(int ec = 0)
        : ec(ec)
    {}
    SimpleExpectedErrorCode(int ec, const String &msg)
        : ec(ec), message(msg)
    {}

    bool operator==(int i) const { return ec == i; }
    const String &getMessage() const { return message; }
};

template <class T, class ... Args>
struct SimpleExpected : std::variant<SimpleExpectedErrorCode, T, Args...>
{
    using Base = std::variant<SimpleExpectedErrorCode, T, Args...>;

    using Base::Base;

    SimpleExpected(const SimpleExpectedErrorCode &e)
        : Base(e)
    {}

    operator bool() const { return Base::index() == 1; }
    T &operator*() { return std::get<1>(*this); }
    const T &operator*() const { return std::get<1>(*this); }
    T &operator->() { return std::get<1>(*this); }
    const T &operator->() const { return std::get<1>(*this); }
    const SimpleExpectedErrorCode &ec() { return std::get<0>(*this); }
};

} // namespace detail

struct SW_CORE_API TargetMap : PackageVersionMapBase<TargetContainer, std::unordered_map, primitives::version::VersionMap>
{
    using Base = PackageVersionMapBase<TargetContainer, std::unordered_map, primitives::version::VersionMap>;

    enum
    {
        Ok,
        PackagePathNotFound,
        PackageNotFound,
        TargetNotCreated, // by settings
    };

    ~TargetMap();

    using Base::find;

    detail::SimpleExpected<std::pair<Version, ITarget *>> find(const PackagePath &pp, const TargetSettings &ts) const;
    ITarget *find(const PackageId &pkg, const TargetSettings &ts) const;
    ITarget *find(const UnresolvedPackage &pkg, const TargetSettings &ts) const;

    //

    template <class T>
    static std::optional<Version> select_version(T &v)
    {
        if (v.empty())
            return {};
        if (!v.empty_releases())
            return v.rbegin_releases()->first;
        return v.rbegin()->first;
    }

private:
    detail::SimpleExpected<Base::version_map_type::iterator> find_and_select_version(const PackagePath &pp);
    detail::SimpleExpected<Base::version_map_type::const_iterator> find_and_select_version(const PackagePath &pp) const;
};

// equals to one group number in terms of non local packages
struct TargetEntryPoint
{
    virtual ~TargetEntryPoint() = 0;

    [[nodiscard]]
    virtual std::vector<ITargetPtr> loadPackages(SwBuild &, const TargetSettings &, const PackageIdSet &allowed_packages, const PackagePath &prefix) const = 0;

    // add get group number api?
    // or entry point hash?
};

using TargetEntryPointPtr = std::shared_ptr<TargetEntryPoint>;

struct TargetData
{
    ~TargetData();

    // create if empty
    template <class U>
    U &getData()
    {
        if (!data.has_value())
            data = U();
        return std::any_cast<U&>(data);
    }

    template <class U>
    const U &getData() const
    {
        if (!data.has_value())
            throw SW_RUNTIME_ERROR("No target data was set");
        return std::any_cast<U&>(data);
    }

private:
    // regex storage
    // files cache
    // etc.
    std::any data;
};

} // namespace sw
