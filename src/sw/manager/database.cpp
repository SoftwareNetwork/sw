// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "database.h"

#include "enums.h"
#include "settings.h"
#include "stamp.h"
#include "storage.h"

#include <sw/support/exceptions.h>
#include <sw/support/hash.h>

#include <primitives/command.h>
#include <primitives/lock.h>
#include <primitives/pack.h>
#include <primitives/templates.h>

// db
#include <primitives/db/sqlite3.h>
#include <sqlite3.h>
#include <sqlpp11/sqlite3/connection.h>
#include <sqlpp11/custom_query.h>
#include <sqlpp11/sqlite3/sqlite3.h>
#include <sqlpp11/sqlpp11.h>
#include <db_service.h>
#include <db_packages.h>

#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <fstream>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "db");

const String service_db_name = "service.db";

namespace sql = sqlpp::sqlite3;

TYPED_EXCEPTION(NoSuchVersion);

#include <inserts.cpp>

namespace sw
{

Database::Database(const path &db_name, const String &schema)
    : fn(db_name)
{
    if (!fs::exists(fn))
    {
        ScopedFileLock lock(fn);
        if (!fs::exists(fn))
        {
            open();
        }
    }

    if (!db)
        open();

    // create or update schema
    fs::create_directories(db_name.parent_path());
    primitives::db::sqlite3::SqliteDatabase db2(db->native_handle());
    createOrUpdateSchema(db2, schema, true);
}

Database::~Database() = default;

void Database::open(bool read_only)
{
    sql::connection_config config;
    config.path_to_database = normalize_path(fn);
    config.flags = 0;
    config.flags |= SQLITE_OPEN_FULLMUTEX;// SQLITE_OPEN_NOMUTEX;
    if (read_only)
        config.flags |= SQLITE_OPEN_READONLY;
    else
        config.flags |= SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE;
    //config.debug = true;
    db = std::make_unique<sql::connection>(config);

    // prevent SQLITE_BUSY rc
    // hope 1 min is enough to wait for write operation
    // in multithreaded environment
    sqlite3_busy_timeout(db->native_handle(), 60000);

    // explicit
    db->execute("PRAGMA foreign_keys = ON");
}

template <typename T>
std::optional<T> Database::getValue(const String &key) const
{
    return primitives::db::sqlite3::SqliteDatabase(db->native_handle()).getValue<T>(key);
}

template <typename T>
T Database::getValue(const String &key, const T &default_) const
{
    return primitives::db::sqlite3::SqliteDatabase(db->native_handle()).getValue<T>(key, default_);
}

template <typename T>
void Database::setValue(const String &key, const T &v) const
{
    primitives::db::sqlite3::SqliteDatabase(db->native_handle()).setValue(key, v);
}

int Database::getIntValue(const String &key)
{
    return getValue(key, 0);
}

void Database::setIntValue(const String &key, int v)
{
    return setValue(key, v);
}

PackagesDatabase::PackagesDatabase(const path &db_fn)
    : Database(db_fn, packages_db_schema)
{
}

std::unordered_map<UnresolvedPackage, PackageId> PackagesDatabase::resolve(const UnresolvedPackages &in_pkgs, UnresolvedPackages &unresolved_pkgs) const
{
    const auto pkgs = ::db::packages::Package{};

    std::unordered_map<UnresolvedPackage, PackageId> r;
    for (auto &pkg : in_pkgs)
    {
        auto pid = getPackageId(pkg.ppath);
        if (!pid)
        {
            unresolved_pkgs.insert(pkg);
            continue;
        }

        db::PackageVersionId id = 0;
        VersionSet versions;
        UnorderedVersionMap<db::PackageVersionId> version_ids;

        const auto pkg_ver = ::db::packages::PackageVersion{};
        for (const auto &row : (*db)(
            select(pkg_ver.packageVersionId, pkg_ver.version)
            .from(pkg_ver)
            .where(pkg_ver.packageId == pid)))
        {
            versions.insert(row.version.value());
            version_ids[row.version.value()] = row.packageVersionId.value();
        }

        auto v = pkg.range.getMaxSatisfyingVersion(versions);
        if (!v)
        {
            unresolved_pkgs.insert(pkg);
            continue;
        }

        r.emplace(pkg, PackageId{ pkg.ppath, *v });
    }
    return r;
}

PackageData PackagesDatabase::getPackageData(const PackageId &p) const
{
    const auto pkgs = ::db::packages::Package{};
    const auto pkg_ver = ::db::packages::PackageVersion{};
    const auto pkgdeps = ::db::packages::PackageVersionDependency{};

    PackageData d;

    auto q = (*db)(
        select(pkg_ver.packageVersionId, pkg_ver.hash, pkg_ver.flags, pkg_ver.updated, pkg_ver.groupNumber, pkg_ver.prefix, pkg_ver.sdir)
        .from(pkg_ver)
        .where(pkg_ver.packageId == getPackageId(p.ppath) && pkg_ver.version == p.version.toString()));
    if (q.empty())
        throw SW_RUNTIME_ERROR("No such package in db: " + p.toString());
    auto &row = q.front();
    d.hash = row.hash.value();
    d.flags = row.flags.value();
    d.group_number = row.groupNumber.value();
    d.prefix = (int)row.prefix.value();
    d.sdir = row.sdir.value();

    for (const auto &row : (*db)(
        select(pkgs.packageId, pkgs.path, pkgdeps.versionRange)
        .from(pkgdeps.join(pkgs).on(pkgdeps.packageId == pkgs.packageId))
        .where(pkgdeps.packageVersionId == row.packageVersionId)))
    {
        d.dependencies.emplace(row.path.value(), row.versionRange.value());
    }

    return d;
}

int64_t PackagesDatabase::getInstalledPackageId(const PackageId &p) const
{
    const auto pkgs = ::db::packages::Package{};
    const auto pkg_ver = ::db::packages::PackageVersion{};

    auto q = (*db)(
        select(pkg_ver.packageVersionId, pkg_ver.hash, pkg_ver.flags, pkg_ver.updated, pkg_ver.groupNumber, pkg_ver.prefix)
        .from(pkg_ver)
        .where(pkg_ver.packageId == getPackageId(p.ppath) && pkg_ver.version == p.version.toString()));
    if (q.empty())
        return 0;
    return q.front().packageVersionId.value();
}

String PackagesDatabase::getInstalledPackageHash(const PackageId &p) const
{
    const auto pkgs = ::db::packages::Package{};
    const auto pkg_ver = ::db::packages::PackageVersion{};

    auto q = (*db)(
        select(pkg_ver.packageVersionId, pkg_ver.hash, pkg_ver.flags, pkg_ver.updated, pkg_ver.groupNumber, pkg_ver.prefix)
        .from(pkg_ver)
        .where(pkg_ver.packageId == getPackageId(p.ppath) && pkg_ver.version == p.version.toString()));
    if (q.empty())
        return {};
    return q.front().hash.value();
}

bool PackagesDatabase::isPackageInstalled(const Package &p) const
{
    return getInstalledPackageId(p) != 0 && getInstalledPackageHash(p) == p.getData().hash;
}

void PackagesDatabase::installPackage(const Package &p)
{
    const auto pkg = ::db::packages::Package{};
    const auto pkgv = ::db::packages::PackageVersion{};
    const auto pkgvd = ::db::packages::PackageVersionDependency{};

    std::lock_guard lk(m);

    db->start_transaction();

    int64_t package_id = 0;

    // get package id
    auto q = (*db)(select(pkg.packageId).from(pkg).where(
        pkg.path == p.ppath.toString()
        ));
    if (q.empty())
    {
        // add package
        (*db)(insert_into(pkg).set(
            pkg.path = p.ppath.toString()
        ));

        // get package id
        auto q = (*db)(select(pkg.packageId).from(pkg).where(
            pkg.path == p.ppath.toString()
            ));
        package_id = q.front().packageId.value();
    }
    else
    {
        package_id = q.front().packageId.value();

        // remove existing version
        (*db)(remove_from(pkgv).where(
            pkgv.packageId == package_id &&
            pkgv.version == p.version.toString()
            ));
    }

    PackageVersionGroupNumber gn;
    auto h = std::hash<String>()(p.storage.getName());
    gn = hash_combine(h, p.getData().group_number);

    // insert version
    (*db)(insert_into(pkgv).set(
        // basic data
        pkgv.packageId = package_id,
        pkgv.version = p.version.toString(),

        // extended
        pkgv.prefix = p.getData().prefix,
        pkgv.hash = p.getData().hash,
        pkgv.groupNumber = gn,

        // TODO:
        pkgv.archiveVersion = 1,

        // misc
        pkgv.updated = "",

        pkgv.sdir = sqlpp::tvin(p.getData().sdir.u8string())
    ));

    // get version id
    auto q2 = (*db)(select(pkgv.packageVersionId).from(pkgv).where(
        pkgv.packageId == package_id &&
        pkgv.version == p.version.toString()
        ));
    for (auto &d : p.getData().dependencies)
    {
        // get package id
        auto q = (*db)(select(pkg.packageId).from(pkg).where(
            pkg.path == d.ppath.toString()
            ));
        if (q.empty())
        {
            // add package
            (*db)(insert_into(pkg).set(
                pkg.path = d.ppath.toString()
            ));

            // get package id
            q = (*db)(select(pkg.packageId).from(pkg).where(
                pkg.path == d.ppath.toString()
                ));
        }

        // insert deps
        (*db)(insert_into(pkgvd).set(
            pkgvd.packageVersionId = q2.front().packageVersionId.value(),
            pkgvd.packageId = q.front().packageId.value(),
            pkgvd.versionRange = d.range.toString()
        ));
    }

    db->commit_transaction();
}

PackageVersionGroupNumber PackagesDatabase::getMaxGroupNumber() const
{
    const auto pkg_ver = ::db::packages::PackageVersion{};

    auto q = (*db)(
        select(max(pkg_ver.groupNumber))
        .from(pkg_ver)
        .unconditionally());
    if (q.empty())
        return {};
    return q.front().max.value();
}

std::optional<path> PackagesDatabase::getOverriddenDir(const Package &p) const
{
    const auto pkgs = ::db::packages::Package{};
    const auto pkg_ver = ::db::packages::PackageVersion{};

    auto q = (*db)(
        select(pkg_ver.sdir)
        .from(pkg_ver)
        .where(pkg_ver.packageId == getPackageId(p.ppath) && pkg_ver.version == p.version.toString()));
    if (q.empty() || q.front().sdir.is_null())
        return {};
    return q.front().sdir.value();
}

std::unordered_set<PackageId> PackagesDatabase::getOverriddenPackages() const
{
    const auto pkg_ver = ::db::packages::PackageVersion{};

    std::unordered_set<PackageId> r;
    for (const auto &row : (*db)(
        select(pkg_ver.packageVersionId, pkg_ver.packageId, pkg_ver.version)
        .from(pkg_ver)
        .where(pkg_ver.sdir.is_not_null())))
    {
        r.emplace(getPackagePath(row.packageId.value()), row.version.value());
    }
    return r;
}

void PackagesDatabase::deletePackage(const PackageId &p) const
{
    const auto pkg_ver = ::db::packages::PackageVersion{};

    (*db)(
        update(pkg_ver)
        .set(pkg_ver.sdir = sqlpp::null)
        .where(pkg_ver.packageId == getPackageId(p.ppath) && pkg_ver.version == p.version.toString())
        );
}

void PackagesDatabase::deleteOverriddenPackageDir(const path &sdir) const
{
    const auto pkg_ver = ::db::packages::PackageVersion{};

    (*db)(
        update(pkg_ver)
        .set(pkg_ver.sdir = sqlpp::null)
        .where(pkg_ver.sdir == normalize_path(sdir))
        );
}

void PackagesDatabase::listPackages(const String &name) const
{
    auto pkgs = getMatchingPackages<std::set>(name);
    if (pkgs.empty())
    {
        LOG_INFO(logger, "nothing found");
        return;
    }

    for (auto &pkg : pkgs)
    {
        auto v1 = getVersionsForPackage(pkg);
        VersionSet versions(v1.begin(), v1.end());
        String out = pkg.toString();
        out += " (";
        for (auto &v : versions)
            out += v.toString() + ", ";
        out.resize(out.size() - 2);
        out += ")";
        LOG_INFO(logger, out);
    }
}

Version PackagesDatabase::getExactVersionForPackage(const PackageId &p) const
{
    Version v = p.version;
    /*DownloadDependency d;
    d.ppath = p.ppath;
    d.id = getPackageId(p.ppath);

    SomeFlags f;
    String h;
    PackageVersionGroupNumber gn;
    int prefix;
    getExactProjectVersionId(d, v, f, h, gn, prefix);*/
    return v;
}

template <template <class...> class C>
C<PackagePath> PackagesDatabase::getMatchingPackages(const String &name) const
{
    const auto tpkgs = ::db::packages::Package{};

    C<PackagePath> pkgs;
    String q;
    if (name.empty())
        q = "SELECT path FROM package ORDER BY path COLLATE NOCASE";
    else
        q = "SELECT path FROM package WHERE path like '%" + name + "%' ORDER BY path COLLATE NOCASE";
    for (const auto &row : (*db)(
        custom_query(sqlpp::verbatim(q))
        .with_result_type_of(select(tpkgs.path).from(tpkgs))
        ))
    {
        pkgs.insert(row.path.value());
    }
    return pkgs;
}

template std::unordered_set<PackagePath>
PackagesDatabase::getMatchingPackages<std::unordered_set>(const String &) const;

template std::set<PackagePath>
PackagesDatabase::getMatchingPackages<std::set>(const String &) const;

std::vector<Version> PackagesDatabase::getVersionsForPackage(const PackagePath &ppath) const
{
    std::vector<Version> versions;
    const auto vpkgs = ::db::packages::PackageVersion{};
    for (const auto &row : (*db)(select(vpkgs.version).from(vpkgs).where(vpkgs.packageId == getPackageId(ppath))))
        versions.push_back(row.version.value());
    return versions;
}

db::PackageId PackagesDatabase::getPackageId(const PackagePath &ppath) const
{
    const auto pkgs = ::db::packages::Package{};
    auto q = (*db)(
        custom_query(sqlpp::verbatim("SELECT package_id FROM package WHERE path = '" + ppath.toString() + "' COLLATE NOCASE"))
        .with_result_type_of(select(pkgs.packageId).from(pkgs))
        );
    if (q.empty())
        return 0;
    return q.front().packageId.value();
}

String PackagesDatabase::getPackagePath(db::PackageId id) const
{
    const auto pkgs = ::db::packages::Package{};

    for (const auto &row : (*db)(
        select(pkgs.path)
        .from(pkgs)
        .where(pkgs.packageId == id)))
    {
        return row.path.value();
    }
    throw SW_RUNTIME_ERROR("No such package: " + std::to_string(id));
}

PackageId PackagesDatabase::getGroupLeader(PackageVersionGroupNumber n) const
{
    const auto pkgs = ::db::packages::Package{};
    const auto vpkgs = ::db::packages::PackageVersion{};
    for (const auto &row : (*db)(select(pkgs.path, vpkgs.version)
        .from(vpkgs.join(pkgs).on(vpkgs.packageId == pkgs.packageId))
        .where(vpkgs.groupNumber == n)
        .order_by(vpkgs.groupNumber.asc()))
        )
    {
        return { row.path.value(), row.version.value() };
    }
    throw SW_RUNTIME_ERROR("Group leader not found for group: " + std::to_string(n));
}

Packages PackagesDatabase::getDependentPackages(const PackageId &pkg)
{
    Packages r;

    // 1. Find current project version id.
    auto project_id = getPackageId(pkg.ppath);

    // 2. Find project versions dependent on this version.
    std::set<std::pair<String, String>> pkgs_s;
    /*db->execute(
        "select path, case when branch is not null then branch else major || '.' || minor || '.' || patch end as version "
        "from package_version_dependency "
        "join package_version on package_version.id = project_version_id "
        "join package on package.id = project_id "
        "where project_dependency_id = '" +
            std::to_string(project_id) + "'",
        [&pkgs_s](SQLITE_CALLBACK_ARGS) {
            pkgs_s.insert({cols[0], cols[1]});
            return 0;
        });*/

    for (auto &p : pkgs_s)
    {
        SW_UNIMPLEMENTED;

        /*Package pkg;
        pkg.ppath = p.first;
        pkg.version = p.second;
        //pkg.createNames();
        r.insert(pkg);*/
    }

    return r;
}

Packages PackagesDatabase::getDependentPackages(const Packages &pkgs)
{
    Packages r;
    for (auto &pkg : pkgs)
    {
        auto dpkgs = getDependentPackages(pkg);
        r.merge(dpkgs);
    }

    // exclude input
    for (auto &pkg : pkgs)
        r.erase(pkg);

    return r;
}

Packages PackagesDatabase::getTransitiveDependentPackages(const Packages &pkgs)
{
    SW_UNIMPLEMENTED;

    /*auto r = pkgs;
    std::map<PackageId, bool> retrieved;
    while (1)
    {
        bool changed = false;

        for (auto &pkg : r)
        {
            if (retrieved[pkg])
                continue;

            retrieved[pkg] = true;
            changed = true;

            auto dpkgs = getDependentPackages(pkg);
            r.merge(dpkgs);
            break;
        }

        if (!changed)
            break;
    }

    // exclude input
    for (auto &pkg : pkgs)
        r.erase(pkg);

    return r;*/
}

DataSources PackagesDatabase::getDataSources() const
{
    const auto ds = ::db::packages::DataSource{};

    DataSources dss;
    for (const auto &row : (*db)(select(ds.url, ds.flags).from(ds).unconditionally()))
    {
        DataSource s;
        s.raw_url = row.url;
        s.flags = row.flags.value();
        if (s.flags[DataSource::fDisabled])
            continue;
        dss.push_back(s);
    }
    if (dss.empty())
        throw SW_RUNTIME_ERROR("No data sources available");
    return dss;
}

}
