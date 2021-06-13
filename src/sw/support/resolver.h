// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "unresolved_package_name.h"

namespace sw
{

struct Package;
struct PackageSettings;
struct UnresolvedPackageId;
using PackagePtr = std::unique_ptr<Package>;

struct SW_SUPPORT_API ResolveRequestResult
{
    PackagePtr r;

    bool isResolved() const { return !!r; }

    // if package version higher than current, overwrite
    // if both are branches, do not accept new
    // assuming passed package has same package path and branch/version matches
    // input is not null
    bool setPackage(PackagePtr);
    void setPackageForce(PackagePtr);
};

/*
* components:
* 1. package path
* 2. package version
* 3. package settings
* 4. security context
* 5. timestamp (slice). Used for <= search.
*/
struct SW_SUPPORT_API ResolveRequest : ResolveRequestResult
{
    UnresolvedPackageName u;
    // value or ref?
    const PackageSettings &settings;
    // value or ref?
    // or take it from swctx?
    // or from sw build - one security ctx for build
    //SecurityContext sctx;
    // timestamp - resolve packages only before this timestamp
    // like, e.g., on build start

    ResolveRequest(const UnresolvedPackageName &u, const PackageSettings &s) : u(u), settings(s) {}
    ResolveRequest(const UnresolvedPackageId &up);

    bool operator==(const ResolveRequest &rhs) const;

    bool setPackage(PackagePtr);

    const PackageSettings &getSettings() const { return settings; }
    const UnresolvedPackageName &getUnresolvedPackageName() const { return u; }
    Package &getPackage() const { if (!isResolved()) throw SW_RUNTIME_ERROR("Package was not resolved: " + toString()); return *r; }
    String toString() const;
};

struct SW_SUPPORT_API IResolver
{
    virtual ~IResolver() = 0;

    /// modern resolve call
    virtual bool resolve(ResolveRequest &) const = 0;
};

struct SW_SUPPORT_API Resolver : IResolver
{
    virtual ~Resolver() = default;

    bool resolve(ResolveRequest &) const override;
    void addStorage(IResolver &);
    virtual std::unique_ptr<Resolver> clone() const { return std::make_unique<Resolver>(*this); }

private:
    std::vector<IResolver *> storages;
};

} // namespace sw
