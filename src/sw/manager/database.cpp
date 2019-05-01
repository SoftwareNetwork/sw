// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "database.h"

#include "enums.h"
#include "inserts.h"
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
#include <db_packages.h>

#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <fstream>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "db");

namespace sql = sqlpp::sqlite3;

#include "database_pps.h"

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

/*
** This function is used to load the contents of a database file on disk
** into the "main" database of open database connection pInMemory, or
** to save the current contents of the database opened by pInMemory into
** a database file on disk. pInMemory is probably an in-memory database,
** but this function will also work fine if it is not.
**
** Parameter zFilename points to a nul-terminated string containing the
** name of the database file on disk to load from or save to. If parameter
** isSave is non-zero, then the contents of the file zFilename are
** overwritten with the contents of the database opened by pInMemory. If
** parameter isSave is zero, then the contents of the database opened by
** pInMemory are replaced by data loaded from the file zFilename.
**
** If the operation is successful, SQLITE_OK is returned. Otherwise, if
** an error occurs, an SQLite error code is returned.
*/
static int loadOrSaveDb(sqlite3 *pInMemory, const char *zFilename, int isSave)
{
    int rc;                  /* Function return code */
    sqlite3 *pFile;          /* SqliteDatabase connection opened on zFilename */
    sqlite3_backup *pBackup; /* Backup object used to copy data */
    sqlite3 *pTo;            /* SqliteDatabase to copy to (pFile or pInMemory) */
    sqlite3 *pFrom;          /* SqliteDatabase to copy from (pFile or pInMemory) */

    /* Open the database file identified by zFilename. Exit early if this fails
                              ** for any reason. */
    if (!isSave)
        rc = sqlite3_open_v2(zFilename, &pFile, SQLITE_OPEN_READONLY, nullptr);
    else
        rc = sqlite3_open(zFilename, &pFile);
    if (rc == SQLITE_OK)
    {
        /* If this is a 'load' operation (isSave==0), then data is copied
        ** from the database file just opened to database pInMemory.
        ** Otherwise, if this is a 'save' operation (isSave==1), then data
        ** is copied from pInMemory to pFile.  Set the variables pFrom and
        ** pTo accordingly. */
        pFrom = (isSave ? pInMemory : pFile);
        pTo = (isSave ? pFile : pInMemory);

        /* Set up the backup procedure to copy from the "main" database of
        ** connection pFile to the main database of connection pInMemory.
        ** If something goes wrong, pBackup will be set to NULL and an error
        ** code and  message left in connection pTo.
        **
        ** If the backup object is successfully created, call backup_step()
        ** to copy data from pFile to pInMemory. Then call backup_finish()
        ** to release resources associated with the pBackup object.  If an
        ** error occurred, then  an error code and message will be left in
        ** connection pTo. If no error occurred, then the error code belonging
        ** to pTo is set to SQLITE_OK.
        */
        pBackup = sqlite3_backup_init(pTo, "main", pFrom, "main");
        if (pBackup)
        {
            (void)sqlite3_backup_step(pBackup, -1);
            (void)sqlite3_backup_finish(pBackup);
        }
        rc = sqlite3_errcode(pTo);
    }

    /* Close the database connection opened on database file zFilename
    ** and return the result of this function. */
    (void)sqlite3_close(pFile);
    return rc;
}

void Database::open(bool read_only, bool in_memory)
{
    sql::connection_config config;
    config.flags = 0;
    config.flags |= SQLITE_OPEN_FULLMUTEX;// SQLITE_OPEN_NOMUTEX;
    if (read_only && !in_memory)
        config.flags |= SQLITE_OPEN_READONLY;
    else
        config.flags |= SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE;
    if (in_memory)
        config.path_to_database = ":memory:";
    else
        config.path_to_database = normalize_path(fn);
    //config.debug = true;
    db = std::make_unique<sql::connection>(config);
    if (in_memory)
        loadOrSaveDb(db->native_handle(), normalize_path(fn).c_str(), 0);

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

PackagesDatabase::~PackagesDatabase() = default;

void PackagesDatabase::open(bool read_only, bool in_memory)
{
    Database::open(read_only, in_memory);
    pps = std::make_unique<PreparedStatements>(*db);
}

std::unordered_map<UnresolvedPackage, PackageId> PackagesDatabase::resolve(const UnresolvedPackages &in_pkgs, UnresolvedPackages &unresolved_pkgs) const
{
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
    PackageData d;

    auto &pp = pps->packageVersionData;
    pp.params.packageId = getPackageId(p.ppath);
    pp.params.version = p.version.toString();

    auto q = (*db)(pp);
    if (q.empty())
    {
        throw SW_RUNTIME_ERROR("No such package in db: " + p.toString());
    }
    auto &row = q.front();
    d.hash = row.hash.value();
    d.flags = row.flags.value();
    d.group_number = row.groupNumber.value();
    d.prefix = (int)row.prefix.value();
    d.sdir = row.sdir.value();

    for (const auto &row : (*db)(
        select(pkgs.packageId, pkgs.path, pkg_deps.versionRange)
        .from(pkg_deps.join(pkgs).on(pkg_deps.packageId == pkgs.packageId))
        .where(pkg_deps.packageVersionId == row.packageVersionId)))
    {
        d.dependencies.emplace(row.path.value(), row.versionRange.value());
    }

    return d;
}

int64_t PackagesDatabase::getInstalledPackageId(const PackageId &p) const
{
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

void PackagesDatabase::installPackage(const PackageId &p, const PackageData &d)
{
    std::lock_guard lk(m);

    db->start_transaction();

    ScopeGuard sg([this]()
    {
        db->rollback_transaction(false);
        //throw SW_RUNTIME_ERROR("db transaction not finished");
    });

    int64_t package_id = 0;

    // get package id
    auto q = (*db)(select(pkgs.packageId).from(pkgs).where(
        pkgs.path == p.ppath.toString()
        ));
    if (q.empty())
    {
        // add package
        (*db)(insert_into(pkgs).set(
            pkgs.path = p.ppath.toString()
        ));

        // get package id
        auto q = (*db)(select(pkgs.packageId).from(pkgs).where(
            pkgs.path == p.ppath.toString()
            ));
        package_id = q.front().packageId.value();
    }
    else
    {
        package_id = q.front().packageId.value();

        // remove existing version
        (*db)(remove_from(pkg_ver).where(
            pkg_ver.packageId == package_id &&
            pkg_ver.version == p.version.toString()
            ));
    }

    // insert version
    (*db)(insert_into(pkg_ver).set(
        // basic data
        pkg_ver.packageId = package_id,
        pkg_ver.version = p.version.toString(),

        // extended
        pkg_ver.prefix = d.prefix,
        pkg_ver.hash = d.hash,
        pkg_ver.groupNumber = d.group_number,

        // TODO:
        pkg_ver.archiveVersion = 1,

        // misc
        pkg_ver.updated = "",

        pkg_ver.sdir = sqlpp::tvin(d.sdir.u8string())
    ));

    // get version id
    auto q2 = (*db)(select(pkg_ver.packageVersionId).from(pkg_ver).where(
        pkg_ver.packageId == package_id &&
        pkg_ver.version == p.version.toString()
        ));
    for (auto &d : d.dependencies)
    {
        // get package id
        auto q = (*db)(select(pkgs.packageId).from(pkgs).where(
            pkgs.path == d.ppath.toString()
            ));
        if (q.empty())
        {
            // add package
            (*db)(insert_into(pkgs).set(
                pkgs.path = d.ppath.toString()
            ));

            // get package id
            q = (*db)(select(pkgs.packageId).from(pkgs).where(
                pkgs.path == d.ppath.toString()
                ));
        }

        // insert deps
        (*db)(insert_into(pkg_deps).set(
            pkg_deps.packageVersionId = q2.front().packageVersionId.value(),
            pkg_deps.packageId = q.front().packageId.value(),
            pkg_deps.versionRange = d.range.toString()
        ));
    }

    db->commit_transaction();
    sg.dismiss();
}

void PackagesDatabase::installPackage(const Package &p)
{
    installPackage(p, p.getData());
}

PackageVersionGroupNumber PackagesDatabase::getMaxGroupNumber() const
{
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
    (*db)(
        update(pkg_ver)
        .set(pkg_ver.sdir = sqlpp::null)
        .where(pkg_ver.packageId == getPackageId(p.ppath) && pkg_ver.version == p.version.toString())
        );
}

void PackagesDatabase::deleteOverriddenPackageDir(const path &sdir) const
{
    (*db)(
        remove_from(pkg_ver)
        .where(pkg_ver.sdir == sdir.u8string())
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
    C<PackagePath> pkgs2;
    String q;
    if (name.empty())
        q = "SELECT path FROM package ORDER BY path COLLATE NOCASE";
    else
        q = "SELECT path FROM package WHERE path like '%" + name + "%' ORDER BY path COLLATE NOCASE";
    for (const auto &row : (*db)(
        custom_query(sqlpp::verbatim(q))
        .with_result_type_of(select(pkgs.path).from(pkgs))
        ))
    {
        pkgs2.insert(row.path.value());
    }
    return pkgs2;
}

template std::unordered_set<PackagePath>
PackagesDatabase::getMatchingPackages<std::unordered_set>(const String &) const;

template std::set<PackagePath>
PackagesDatabase::getMatchingPackages<std::set>(const String &) const;

std::vector<Version> PackagesDatabase::getVersionsForPackage(const PackagePath &ppath) const
{
    std::vector<Version> versions;
    for (const auto &row : (*db)(select(pkg_ver.version).from(pkg_ver).where(pkg_ver.packageId == getPackageId(ppath))))
        versions.push_back(row.version.value());
    return versions;
}

db::PackageId PackagesDatabase::getPackageId(const PackagePath &ppath) const
{
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
    for (const auto &row : (*db)(select(pkgs.path, pkg_ver.version)
        .from(pkg_ver.join(pkgs).on(pkg_ver.packageId == pkgs.packageId))
        .where(pkg_ver.groupNumber == n)
        .order_by(pkg_ver.groupNumber.asc()))
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
