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

namespace sw
{

struct ITarget;

struct IDependency
{
    virtual ~IDependency() = 0;

    virtual const TargetSettings &getSettings() const = 0;
    virtual UnresolvedPackage getUnresolvedPackage() const = 0;
    virtual bool isResolved() const = 0;
    virtual void setTarget(const ITarget &) = 0;
};

struct ITarget : ICastable
{
    virtual ~ITarget() = 0;

    virtual const PackageId &getPackage() const = 0;

    /// can be registered to software network
    virtual bool isReal() const = 0;

    ///
    virtual const Source &getSource() const = 0;

    ///
    virtual Files getSourceFiles() const = 0;
    // getFiles()? some langs does not have 'sources'
    // also add get binary files?

    /// get all direct dependencies
    virtual std::vector<IDependency *> getDependencies() const = 0;

    // get output config

    /// returns true if target is not fully prepared yet
    virtual bool prepare() = 0;

    ///
    virtual Commands getCommands() const = 0;

    // get settings()?

    /*
    virtual
    map<string key, arguments> get public settings()?
    get interface settings?
    */

    // String get package settings(); // json coded or whatever via interface?
    // String getDescription()

    // compare using settings
    // remove?
    virtual bool operator==(const TargetSettings &) const = 0;
    virtual bool operator<(const TargetSettings &) const = 0;
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

struct TargetLoader
{
    virtual ~TargetLoader() = 0;

    // on zero input packages, load all
    virtual void loadPackages(const TargetSettings &, const PackageIdSet &allowed_packages = {}) = 0;
};

// it is impossible to keep targets in std::map<TargetSettings, ITargetPtr>,
// because each target knows how to compare itself
// also TargetSettings are mutable?
struct TargetData : std::vector<ITargetPtr>
{
    using Base = std::vector<ITargetPtr>;

    void loadPackages(const TargetSettings &, const PackageIdSet &allowed_packages = {});
    void setEntryPoint(const std::shared_ptr<TargetLoader> &);

    Base::iterator find(const TargetSettings &s);
    Base::const_iterator find(const TargetSettings &s) const;

private:
    // shared, because multiple pkgs has same entry point
    std::shared_ptr<TargetLoader> ep;
    // regex storage
    // files cache
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

    operator bool() const { return index() == 1; }
    T &operator*() { return std::get<1>(*this); }
    const T &operator*() const { return std::get<1>(*this); }
    T &operator->() { return std::get<1>(*this); }
    const T &operator->() const { return std::get<1>(*this); }
    const SimpleExpectedErrorCode &ec() { return std::get<0>(*this); }
};

} // namespace detail

struct TargetMap : PackageVersionMapBase<TargetData, std::unordered_map, primitives::version::VersionMap>
{
    using Base = PackageVersionMapBase<TargetData, std::unordered_map, primitives::version::VersionMap>;

    enum
    {
        Ok,
        PackagePathNotFound,
        PackageNotFound,
        TargetNotCreated, // by settings
    };

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

} // namespace sw
