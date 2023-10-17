// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>

#include "package_database.h"

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
#include <primitives/sqlpp11.h>

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

void Database::open(bool read_only, bool in_memory)
{
    sql::connection_config config;
    config.flags = 0;
    //config.flags |= SQLITE_OPEN_NOMUTEX; // sets multithreaded db access, must protect all connection uses with mutex
    config.flags |= SQLITE_OPEN_FULLMUTEX; // sets serialized db access
    if (read_only && !in_memory)
        config.flags |= SQLITE_OPEN_READONLY;
    else
        config.flags |= SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE;
    if (in_memory)
        config.path_to_database = ":memory:";
    else
        config.path_to_database = to_string(to_path_string(fn));
    //config.debug = true;
    db = std::make_unique<sql::connection>(config);
    if (in_memory)
        loadOrSaveDb(db->native_handle(), (const char *)to_path_string(fn).c_str(), 0);

    // prevent SQLITE_BUSY rc
    // hope 1 min is enough to wait for write operation
    // in multithreaded environment
    sqlite3_busy_timeout(db->native_handle(), 60000);

    // explicit
    db->execute("PRAGMA foreign_keys = ON");
    if (1
        && !in_memory // in memory does not need WAL at all
        && !read_only // read only with WAL will work only if -wal and -shm db files already exist
        )
    {
        // allows to use db from separate processes
        db->execute("PRAGMA journal_mode = WAL");
        // better WAL sync to disk
        db->execute("PRAGMA synchronous = NORMAL");
    }
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
    pp.params.packageId = getPackageId(p.getPath());
    pp.params.version = p.getVersion().toString();

    auto q = (*db)(pp);
    if (q.empty())
    {
        throw SW_RUNTIME_ERROR("No such package in db: " + p.toString());
    }
    auto &row = q.front();
    d.hash = getInstalledPackageHash(row.packageVersionId);
    d.flags = row.flags.value();
    d.prefix = (int)row.prefix.value();
    d.sdir = row.sdir.value();

    auto q2 = (*db)(
        select(t_pkg_ver_files.source)
        .from(t_pkg_ver_files)
        .where(t_pkg_ver_files.packageVersionId == row.packageVersionId));
    if (q2.empty())
        throw SW_LOGIC_ERROR("no pkg ver file");
    if (!q2.front().source.is_null())
        d.source = q2.front().source.value();

    for (const auto &row : (*db)(
        select(pkgs.packageId, pkgs.path, pkg_deps.versionRange)
        .from(pkg_deps.join(pkgs).on(pkg_deps.packageId == pkgs.packageId))
        .where(pkg_deps.packageVersionId == row.packageVersionId)))
    {
        d.dependencies.emplace(row.path.value(), row.versionRange.value());
    }

    return d;
}

db::PackageVersionId PackagesDatabase::getInstalledPackageId(const PackageId &p) const
{
    return getPackageVersionId(p);
}

String PackagesDatabase::getInstalledPackageHash(const PackageId &p) const
{
    return getInstalledPackageHash(getInstalledPackageId(p));
}

String PackagesDatabase::getInstalledPackageHash(db::PackageVersionId vid) const
{
    auto q2 = (*db)(
        select(t_pkg_ver_files.fileId)
        .from(t_pkg_ver_files)
        .where(t_pkg_ver_files.packageVersionId == vid));
    if (q2.empty())
        throw SW_LOGIC_ERROR("no pkg ver file");
    auto q3 = (*db)(
        select(t_files.hash)
        .from(t_files)
        .where(t_files.fileId == q2.front().fileId));
    if (q3.empty())
        throw SW_LOGIC_ERROR("no file");
    return q3.front().hash.value();
}

bool PackagesDatabase::isPackageInstalled(const Package &p) const
{
    return getInstalledPackageId(p) != 0 && getInstalledPackageHash(p) == p.getData().getHash(StorageFileType::SourceArchive);
}

void PackagesDatabase::installPackage(const PackageId &p, const PackageData &d)
{
    std::lock_guard lk(m);
    auto tr = sqlpp11_transaction_manual(*db);

    int64_t package_id = 0;

    // get package id
    auto q = (*db)(select(pkgs.packageId).from(pkgs).where(
        pkgs.path == p.getPath().toString()
        ));
    if (q.empty())
    {
        // add package
        (*db)(insert_into(pkgs).set(
            pkgs.path = p.getPath().toString()
        ));

        // get package id
        auto q = (*db)(select(pkgs.packageId).from(pkgs).where(
            pkgs.path == p.getPath().toString()
            ));
        package_id = q.front().packageId.value();
    }
    else
    {
        package_id = q.front().packageId.value();

        // remove existing version
        (*db)(remove_from(pkg_ver).where(
            pkg_ver.packageId == package_id &&
            pkg_ver.version == p.getVersion().toString()
            ));
    }

    // insert version
    (*db)(insert_into(pkg_ver).set(
        // basic data
        pkg_ver.packageId = package_id,
        pkg_ver.version = p.getVersion().toString(),

        // extended
        pkg_ver.prefix = d.prefix,

        // misc
        pkg_ver.updated = "",

        pkg_ver.sdir = to_string(d.sdir.u8string())
    ));

    // get version id
    auto q2 =
        (*db)(select(pkg_ver.packageVersionId).from(pkg_ver).where(
        pkg_ver.packageId == package_id &&
        pkg_ver.version == p.getVersion().toString()
        ));
    auto vid = q2.front().packageVersionId.value();

    // insert file
    (*db)(insert_into(t_files).set(
        t_files.hash = d.getHash(StorageFileType::SourceArchive)
    ));
    auto fid = db->last_insert_id();

    // inser pkg ver file
    (*db)(insert_into(t_pkg_ver_files).set(
        t_pkg_ver_files.packageVersionId = vid,
        t_pkg_ver_files.fileId = fid,
        t_pkg_ver_files.type = 1,
        t_pkg_ver_files.configId = 1,
        t_pkg_ver_files.archiveVersion = 1
    ));

    //
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
            pkg_deps.packageVersionId = vid,
            pkg_deps.packageId = q.front().packageId.value(),
            pkg_deps.versionRange = d.range.toString()
        ));
    }
}

void PackagesDatabase::installPackage(const Package &p)
{
    installPackage(p, p.getData());
}

std::optional<path> PackagesDatabase::getOverriddenDir(const Package &p) const
{
    auto q = (*db)(
        select(pkg_ver.sdir)
        .from(pkg_ver)
        .where(pkg_ver.packageId == getPackageId(p.getPath()) && pkg_ver.version == p.getVersion().toString()));
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
        remove_from(pkg_ver)
        .where(pkg_ver.packageId == getPackageId(p.getPath()) && pkg_ver.version == p.getVersion().toString())
        );
}

void PackagesDatabase::deleteOverriddenPackageDir(const path &sdir) const
{
    (*db)(
        remove_from(pkg_ver)
        .where(pkg_ver.sdir == to_string(sdir.u8string()))
        );
}

std::vector<PackagePath> PackagesDatabase::getMatchingPackages(const String &name, int limit, int offset) const
{
    String slimit;
    if (limit > 0)
        slimit = " LIMIT " + std::to_string(limit);
    String soffset;
    if (offset > 0)
        soffset = " LIMIT " + std::to_string(offset);

    std::vector<PackagePath> pkgs2;
    String q;
    if (name.empty())
        q = "SELECT path FROM package ORDER BY path COLLATE NOCASE" + slimit + soffset;
    else
        q = "SELECT path FROM package WHERE path like '%" + name + "%' ORDER BY path COLLATE NOCASE" + slimit + soffset;
    for (const auto &row : (*db)(
        custom_query(sqlpp::verbatim(q))
        .with_result_type_of(select(pkgs.path).from(pkgs))
        ))
    {
        pkgs2.push_back(row.path.value());
    }
    return pkgs2;
}

VersionSet PackagesDatabase::getVersionsForPackage(const PackagePath &ppath) const
{
    VersionSet versions;
    for (const auto &row : (*db)(select(pkg_ver.version).from(pkg_ver).where(pkg_ver.packageId == getPackageId(ppath))))
        versions.insert(row.version.value());
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

db::PackageVersionId PackagesDatabase::getPackageVersionId(const PackageId &p) const
{
    auto q = (*db)(
        select(pkg_ver.packageVersionId)
        .from(pkg_ver)
        .where(pkg_ver.packageId == getPackageId(p.getPath()) && pkg_ver.version == p.getVersion().toString()));
    if (q.empty())
        return 0;
    return q.front().packageVersionId.value();
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

}
