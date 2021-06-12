// SPDX-License-Identifier: AGPL-3.0-only
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

bool PackagesDatabase::resolve(ResolveRequest &rr, const IStorage &s, bool allow_override) const
{
    auto &upkg = rr.u;

    auto pid = getPackageId(upkg.getPath());
    if (!pid)
        return false;

    auto settings_hash = rr.getSettings().getHash();
    auto q = (*db)(select(configs.configId).from(configs).where(
        configs.hash == settings_hash
    ));
    if (q.empty())
        return false;
    auto config_id = q.front().configId.value();

    bool resolved = false;
    for (const auto &row : (*db)(
        select(pkg_ver.packageVersionId, pkg_ver.version)
        .from(pkg_ver)
        .where(pkg_ver.packageId == pid)))
    {
        // inser pkg ver file
        auto q = (*db)(select(t_pkg_ver_files.fileId).from(t_pkg_ver_files).where(
            t_pkg_ver_files.packageVersionId == row.packageVersionId.value() &&
            t_pkg_ver_files.configId == config_id
        ));
        if (q.empty())
            continue;
        auto file_id = q.front().fileId.value();

        auto p = s.makePackage({ {upkg.getPath(), row.version.value()}, rr.getSettings() });
        auto d = std::make_unique<PackageData>(getPackageData(p->getId()));
        p->setData(std::move(d));

        bool override = 1
            && allow_override
            && rr.isResolved()
            && p->getId().getName() == rr.getPackage().getId().getName() // same pkg
            && !rr.getPackage().getData().hash.empty() // do not override overridden pkgs
            && p->getData().hash != rr.getPackage().getData().hash // on hash change
            ;
        if (override)
        {
            rr.setPackageForce(std::move(p));
            resolved = true;
        }
        else
            resolved |= rr.setPackage(std::move(p));
    }

    return resolved;
}

PackageData PackagesDatabase::getPackageData(const PackageId &p) const
{
    auto &pp = pps->packageVersionData;
    pp.params.packageId = getPackageId(p.getName().getPath());
    pp.params.version = p.getName().getVersion().toString();

    auto q = (*db)(pp);
    if (q.empty())
        throw SW_RUNTIME_ERROR("No such package in db: " + p.getName().toString());

    auto &row = q.front();
    PackageData d;
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

    return d;
}

String PackagesDatabase::getInstalledPackageHash(const PackageId &p) const
{
    return getInstalledPackageHash(getPackageVersionId(p.getName()));
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
    return getPackageVersionId(p.getId().getName()) != 0 && getInstalledPackageHash(p.getId()) == p.getData().getHash();
}

void PackagesDatabase::installPackage(const PackageId &p, const PackageData &d)
{
    SW_UNIMPLEMENTED;
}

void PackagesDatabase::installPackage(const Package &p)
{
    std::lock_guard lk(m);
    auto tr = sqlpp11_transaction_manual(*db);

    auto settings_hash = p.getId().getSettings().getHash();
    int64_t package_id = 0;

    // get package id
    if (auto q =
        (*db)(select(pkgs.packageId).from(pkgs).where(
            pkgs.path == p.getId().getName().getPath().toString()
        ));
        q.empty()
        )
    {
        // add package
        (*db)(insert_into(pkgs).set(
            pkgs.path = p.getId().getName().getPath().toString()
        ));
        package_id = db->last_insert_id();
    }
    else
    {
        package_id = q.front().packageId.value();

        // remove existing version and all packages
        if (settings_hash == 0)
        {
            (*db)(remove_from(pkg_ver).where(
                pkg_ver.packageId == package_id &&
                pkg_ver.version == p.getId().getName().getVersion().toString()
                ));
        }
    }

    int64_t version_id = 0;

    // get version id
    if (auto q =
        (*db)(select(pkg_ver.packageVersionId).from(pkg_ver).where(
            pkg_ver.packageId == package_id &&
            pkg_ver.version == p.getId().getName().getVersion().toString()
        ));
        q.empty()
        )
    {
        // insert version
        (*db)(insert_into(pkg_ver).set(
            // basic data
            pkg_ver.packageId = package_id,
            pkg_ver.version = p.getId().getName().getVersion().toString(),

            // extended
            pkg_ver.prefix = p.getData().prefix,

            // misc
            pkg_ver.updated = "",

            pkg_ver.sdir = sqlpp::tvin(to_string(p.getData().sdir.u8string()))
        ));
        version_id = db->last_insert_id();
    }
    else
    {
        version_id = q.front().packageVersionId.value();
    }

    int64_t file_id = 1;
    // insert file, we have empty hash for local pkgs
    if (auto q = (*db)(
        select(t_files.fileId)
        .from(t_files)
        .where(t_files.hash == p.getData().getHash()));
        q.empty()
        )
    {
        (*db)(insert_into(t_files).set(
            t_files.hash = p.getData().getHash()
        ));
        file_id = db->last_insert_id();
    }
    else
    {
        file_id = q.front().fileId.value();
    }

    int64_t config_id = 1;
    if (auto q = (*db)(select(configs.configId).from(configs).where(
        configs.hash == settings_hash
        ));
        q.empty())
    {
        (*db)(insert_into(configs).set(
            configs.hash = settings_hash
        ));
        config_id = db->last_insert_id();
    }
    else
    {
        config_id = q.front().configId.value();
    }

    // inser pkg ver file
    (*db)(insert_into(t_pkg_ver_files).set(
        t_pkg_ver_files.packageVersionId = version_id,
        t_pkg_ver_files.fileId = file_id,
        t_pkg_ver_files.configId = config_id,
        t_pkg_ver_files.archiveVersion = 1
    ));
}

std::optional<path> PackagesDatabase::getOverriddenDir(const Package &p) const
{
    SW_UNIMPLEMENTED;
    /*auto q = (*db)(
        select(pkg_ver.sdir)
        .from(pkg_ver)
        .where(pkg_ver.packageId == getPackageId(p.getPath()) && pkg_ver.version == p.getVersion().toString()));
    if (q.empty() || q.front().sdir.is_null())
        return {};
    return q.front().sdir.value();*/
}

std::unordered_set<PackageId> PackagesDatabase::getOverriddenPackages() const
{
    SW_UNIMPLEMENTED;
    /*std::unordered_set<PackageId> r;
    for (const auto &row : (*db)(
        select(pkg_ver.packageVersionId, pkg_ver.packageId, pkg_ver.version)
        .from(pkg_ver)
        .where(pkg_ver.sdir.is_not_null())))
    {
        r.emplace(getPackagePath(row.packageId.value()), row.version.value());
    }
    return r;*/
}

void PackagesDatabase::deletePackage(const PackageId &p) const
{
    SW_UNIMPLEMENTED;
    /*(*db)(
        remove_from(pkg_ver)
        .where(pkg_ver.packageId == getPackageId(p.getPath()) && pkg_ver.version == p.getVersion().toString())
        );*/
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

std::vector<PackageVersion> PackagesDatabase::getVersionsForPackage(const PackagePath &ppath) const
{
    std::vector<PackageVersion> versions;
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

db::PackageVersionId PackagesDatabase::getPackageVersionId(const PackageName &p) const
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
