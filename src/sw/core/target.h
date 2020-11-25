// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "settings.h"

#include <sw/builder/command.h>
#include <sw/builder/node.h>
#include <sw/manager/package.h>
#include <sw/support/package_version_map.h>
#include <sw/support/source.h>

#include <any>
#include <map>
#include <memory>
#include <variant>

namespace sw
{

struct LogicalInput;
struct IRule;
struct ITarget;
struct SwBuild;
struct ResolveRequest;
struct Resolver;

struct SW_CORE_API ResolverHolder
{
    /// resolve deps using this target resolver
    Resolver &getResolver() const; // to pass to children
    Resolver *setResolver(Resolver &); // returns old resolver
    bool resolve(ResolveRequest &) const;

private:
    Resolver *resolver = nullptr;
};

struct SW_CORE_API AllowedPackages
{
    AllowedPackages() = default;
    AllowedPackages(const UnresolvedPackages &);
    AllowedPackages(const PackageIdSet &);

    bool contains(const PackageId &) const;
    bool empty() const;

private:
    std::unordered_map<PackagePath, VersionRange> pkgs;
    UnresolvedPackages branches;
};

struct SW_CORE_API TargetFile
{
    TargetFile(const path &abspath, bool is_generated = false, bool is_from_other_target = false);
    /// calculates relpath from abspath and rootdir if abspath is under root
    /// leaves abspath as is if it's not under root
    //TargetFile(const path &abspath, const path &rootdir, bool is_generated = false, bool is_from_other_target = false);

    const path &getPath() const { return fn; }
    bool isGenerated() const { return is_generated; }
    bool isFromOtherTarget() const { return is_from_other_target; }

private:
    path fn; // abs
    bool is_generated;
    bool is_from_other_target;
};

using TargetFiles = std::unordered_map<path, TargetFile>;

struct SW_CORE_API IDependency
{
    virtual ~IDependency() = 0;

    virtual const PackageSettings &getSettings() const = 0;
    virtual UnresolvedPackage getUnresolvedPackage() const = 0;
    virtual bool isResolved() const = 0;
    virtual void setTarget(const ITarget &) = 0;
    virtual const ITarget &getTarget() const = 0;
};

using IDependencyPtr = std::shared_ptr<IDependency>;

/// Very basic interface for targets and must be very stable.
/// You won't be operating much using it.
/// Instead, text interface for querying data will be available.
struct SW_CORE_API ITarget : ICastable, ResolverHolder
{
    virtual ~ITarget();

    //
    // basic info/description section
    //

    virtual const PackageId &getPackage() const = 0;

    // merge getSource(), getFiles() and getDependencies() into single call returning json/target settings?
    // into getDescription() or getInformation() or something similar

    // how to fetch package
    ///
    virtual const Source &getSource() const = 0;

    /// get target files
    /// purposes:
    /// 1. to create archives
    /// 2. for ide support
    /// ...?
    virtual TargetFiles getFiles(StorageFileType) const = 0;

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
    //virtual const PackageSettings &getConfiguration() const = 0;

    /// input settings
    /// do not round trip
    virtual const PackageSettings &getSettings() const = 0;

    // settings for consumers (targets) and users?
    // output command or module name
    ///
    virtual const PackageSettings &getInterfaceSettings() const = 0;

    // get binary settings, get doc settings?
    // String get package settings(); // json coded or whatever via interface?
    // String getDescription()

    // returns prepared command for executing
    // result may be null
    //
    // getCommand()

    // result may be null
    // getLoadableModule()

    // getRunRule()?
    // getExecutableRule()?
    // by default returns nullptr
    //virtual std::unique_ptr<IRule> getRule() const;
};

using ITargetPtr = std::unique_ptr<ITarget>;

/*struct INativeTarget : ITarget
{
    // header only does not provide these
    virtual path getOutputFile() const = 0;
    virtual path getImportLibrary() const = 0;
    // get cl args?
    // get link args?
};*/

// this target will be created by core
// when saved settings loaded
// when program detection occurs
struct SW_CORE_API PredefinedTarget : ITarget
{
    PackageSettings public_ts;

    PredefinedTarget(const PackageId &, const PackageSettings &);
    PredefinedTarget(const PredefinedTarget &) = delete;
    virtual ~PredefinedTarget();

    std::vector<IDependency *> getDependencies() const override;

    // return what we know
    const PackageId &getPackage() const override { return pkg; }
    const PackageSettings &getSettings() const override { return ts; }
    const PackageSettings &getInterfaceSettings() const override { return public_ts; }

    // lightweight target
    const Source &getSource() const override { static EmptySource es; return es; }  // empty source
    TargetFiles getFiles(StorageFileType) const override { return {}; }             // no files
    bool prepare() override { return false; }                                       // no prepare
    Commands getCommands() const override { return {}; }                            // no commands
    Commands getTests() const override { return {}; }                               // no tests

private:
    PackageId pkg;
    PackageSettings ts;
    mutable bool deps_set = false;
    mutable std::vector<IDependencyPtr> deps;
};

struct SW_CORE_API TargetContainer
{
    using Storage = std::set<ITarget*>;
    using Base = Storage;

    // find target with equal settings
    Base::iterator findEqual(const PackageSettings &);
    Base::const_iterator findEqual(const PackageSettings &) const;

    // find target with equal subset of provided settings
    // findEqualSubset()
    Base::iterator findSuitable(const PackageSettings &);
    Base::const_iterator findSuitable(const PackageSettings &) const;

    void push_back(ITarget &);

    void clear();
    bool empty() const;
    size_t size() const { return targets.size(); }

    auto begin() { return targets.begin(); }
    auto end() { return targets.end(); }

    auto begin() const { return targets.begin(); }
    auto end() const { return targets.end(); }

    Base::iterator erase(Base::iterator begin, Base::iterator end);

private:
    Storage targets;
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

template <class ... Args>
struct ExtendedVersionMap : ::primitives::version::VersionMap<Args...>
{
};

struct TargetMap : PackageVersionMapBase<TargetContainer, std::unordered_map, ExtendedVersionMap>
{
    using Base = PackageVersionMapBase<TargetContainer, std::unordered_map, ExtendedVersionMap>;

    using Base::find;

    SW_CORE_API
    detail::SimpleExpected<std::pair<Version, ITarget *>> find(const PackagePath &pp, const PackageSettings &ts) const;
    SW_CORE_API
    ITarget *find(const PackageId &pkg, const PackageSettings &ts) const;
    SW_CORE_API
    ITarget *find(const UnresolvedPackage &pkg, const PackageSettings &ts) const;
};

//
struct SW_CORE_API TargetEntryPoint
{
    virtual ~TargetEntryPoint();

    [[nodiscard]]
    virtual std::vector<ITargetPtr> loadPackages(SwBuild &, const PackageSettings &, const AllowedPackages &allowed_packages, const PackagePath &prefix) const = 0;
};

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
