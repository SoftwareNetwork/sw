// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "database.h"

#include "directories.h"
#include "enums.h"
#include "exceptions.h"
#include "hash.h"
#include "http.h"
#include "lock.h"
#include "settings.h"
#include "sqlite_database.h"
#include "stamp.h"

#include <primitives/command.h>
#include <primitives/lock.h>
#include <primitives/pack.h>
#include <primitives/templates.h>

// db
#include <primitives/db/sqlite3.h>
#include <sqlite3.h>
#include <sqlpp11/custom_query.h>
#include <sqlpp11/sqlite3/sqlite3.h>
#include <sqlpp11/sqlpp11.h>
#include <db_service.h>
#include <db_packages.h>
#include "sqlite_database.h"

#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <fstream>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "db");

#define PACKAGES_DB_REFRESH_TIME_MINUTES 15

#define PACKAGES_DB_SCHEMA_VERSION 4
#define PACKAGES_DB_SCHEMA_VERSION_FILE "schema.version"
#define PACKAGES_DB_VERSION_FILE "db.version"
#define PACKAGES_DB_DOWNLOAD_TIME_FILE "packages.time"

const String db_repo_name = "SoftwareNetwork/database";
const String db_repo_url = "https://github.com/" + db_repo_name;
const String db_master_url = db_repo_url + "/archive/master.zip";
const String db_version_url = "https://raw.githubusercontent.com/" + db_repo_name + "/master/" PACKAGES_DB_VERSION_FILE;

const path db_dir_name = "database";
const path db_repo_dir_name = "repository";
const String packages_db_name = "packages.db";
const String service_db_name = "service.db";

namespace sql = sqlpp::sqlite3;

TYPED_EXCEPTION(NoSuchVersion);

#include <inserts.cpp>

// save current time during main call
// it is used for detecting young packages
static TimePoint tstart;

namespace sw
{

std::vector<StartupAction> startup_actions{
    {1, StartupAction::ClearCache},
    {2, StartupAction::ServiceDbClearConfigHashes},
    {5, StartupAction::ClearStorageDirExp},
    //{ 6, StartupAction::ClearSourceGroups },
    {7, StartupAction::ClearStorageDirExp | StartupAction::ClearStorageDirBin | StartupAction::ClearStorageDirLib},
    {8, StartupAction::ClearCfgDirs},
    {9, StartupAction::ClearStorageDirExp},
    {10, StartupAction::ClearPackagesDatabase},
    {11, StartupAction::ServiceDbClearConfigHashes},
};

path getDbDirectory()
{
    // db per storage
    return getUserDirectories().storage_dir_etc / db_dir_name;
}

int readPackagesDbSchemaVersion(const path &dir)
{
    auto p = dir / PACKAGES_DB_SCHEMA_VERSION_FILE;
    if (!fs::exists(p))
        return 0;
    return std::stoi(read_file(p));
}

void writePackagesDbSchemaVersion(const path &dir)
{
    write_file(dir / PACKAGES_DB_SCHEMA_VERSION_FILE, std::to_string(PACKAGES_DB_SCHEMA_VERSION));
}

int readPackagesDbVersion(const path &dir)
{
    auto p = dir / PACKAGES_DB_VERSION_FILE;
    if (!fs::exists(p))
        return 0;
    return std::stoi(read_file(p));
}

void writePackagesDbVersion(const path &dir, int version)
{
    write_file(dir / PACKAGES_DB_VERSION_FILE, std::to_string(version));
}

ServiceDatabase &getServiceDatabase(bool init)
{
    // this holder will init on-disk sdb once
    // later thread local calls will just open it
    static ServiceDatabase run_once_db;
    if (init)
        run_once_db.init();

    thread_local ServiceDatabase db;
    return db;
}

ServiceDatabase &getServiceDatabaseReadOnly()
{
    return getServiceDatabase();

    RUN_ONCE
    {
        getServiceDatabase();
    };
    static ServiceDatabase db;
    RUN_ONCE
    {
        db.open(true);
    };
    return db;
}

PackagesDatabase &getPackagesDatabase()
{
    // this holder will init on-disk sdb once
    // later thread local calls will just open it
    static PackagesDatabase run_once_db;

    thread_local PackagesDatabase db;
    return db;
}

Database::Database(const String &name, const String &schema)
{
    db_dir = getDbDirectory();
    fn = db_dir / name;

    if (!fs::exists(fn))
    {
        ScopedFileLock lock(fn);
        if (!fs::exists(fn))
        {
            open();
            created = true;
        }
    }

    if (!db)
        open();

    // craete or update schema
    ::create_directories(getDbDirectory());
    primitives::db::sqlite3::SqliteDatabase db2(db->native_handle());
    createOrUpdateSchema(db2, schema, true);
}

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
}

void Database::recreate()
{
}

template <typename T>
optional<T> Database::getValue(const String &key) const
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

ServiceDatabase::ServiceDatabase()
    : Database(service_db_name, service_db_schema)
{
}

void ServiceDatabase::init()
{
    RUN_ONCE
    {
        checkForUpdates();
        performStartupActions();
    };
}

void ServiceDatabase::performStartupActions() const
{
    // perform startup actions on client update
    try
    {
        static bool once = false;
        if (once)
            return;

        std::set<int> actions_performed; // prevent multiple execution of the same actions
        for (auto &a : startup_actions)
        {
            if (isActionPerformed(a) ||
                actions_performed.find(a.action) != actions_performed.end())
                continue;

            if (!once)
                LOG_INFO(logger, "Performing actions for the new client version");
            once = true;

            actions_performed.insert(a.action);
            setActionPerformed(a);

            // do actions
            if (a.action & StartupAction::ClearCache)
            {
                //CMakePrinter().clear_cache();
            }

            if (a.action & StartupAction::ServiceDbClearConfigHashes)
            {
                clearConfigHashes();

                // also cleanup temp build dir
                error_code ec;
                fs::remove_all(temp_directory_path(), ec);
            }

            if (a.action & StartupAction::ClearPackagesDatabase)
            {
                fs::remove(getDbDirectory() / packages_db_name);
            }

            /*if (a.action & StartupAction::ClearStorageDirExp)
            {
                remove_all_from_dir(getDirectories().storage_dir_exp);
            }*/

            if (a.action & StartupAction::ClearStorageDirBin)
            {
                // also remove exp to trigger cmake
                //remove_all_from_dir(getDirectories().storage_dir_exp);
                remove_all_from_dir(getDirectories().storage_dir_bin);
            }

            if (a.action & StartupAction::ClearStorageDirLib)
            {
                // also remove exp to trigger cmake
                //remove_all_from_dir(getDirectories().storage_dir_exp);
                remove_all_from_dir(getDirectories().storage_dir_lib);
            }

            if (a.action & StartupAction::ClearCfgDirs)
            {
                for (auto &i : boost::make_iterator_range(fs::directory_iterator(getDirectories().storage_dir_cfg), {}))
                {
                    if (fs::is_directory(i))
                        fs::remove_all(i);
                }
            }
        }
    }
    catch (std::exception &e)
    {
        // do not fail
        LOG_WARN(logger, "Warning: " << e.what());
    }
}

void ServiceDatabase::checkForUpdates() const
{
    using namespace std::literals;

    auto last_check = getLastClientUpdateCheck();
    auto d = Clock::now() - last_check;
    if (d < 3h)
        return;

    try
    {
        // if there are updates, set next check (and notification) in 20 mins
        // to issue a message every run
        /*if (Settings::get_user_settings().checkForUpdates())
            setLastClientUpdateCheck(last_check + 20min);
        else
            setLastClientUpdateCheck();*/
    }
    catch (...)
    {
    }
}

TimePoint ServiceDatabase::getLastClientUpdateCheck() const
{
    return Clock::from_time_t(getValue("NextClientVersionCheck", 0LL));
}

void ServiceDatabase::setLastClientUpdateCheck(const TimePoint &p) const
{
    setValue("NextClientVersionCheck", Clock::to_time_t(p));
}

bool ServiceDatabase::isActionPerformed(const StartupAction &action) const
{
    const auto actions = db::service::StartupAction{};
    auto c = ((*db)(
        select(count(actions.startupActionId))
        .from(actions)
        .where(actions.startupActionId == action.id and actions.action == action.action)
        )).front().count.value();
    return c == 1;
}

void ServiceDatabase::setActionPerformed(const StartupAction &action) const
{
    const auto actions = db::service::StartupAction{};
    (*db)(insert_into(actions).set(
        actions.startupActionId = action.id,
        actions.action = action.action
    ));
}

int ServiceDatabase::getPackagesDbSchemaVersion() const
{
    return getValue("PackagesDbSchemaVersion", 0);
}

void ServiceDatabase::setPackagesDbSchemaVersion(int version) const
{
    setValue("PackagesDbSchemaVersion", version);
}

void ServiceDatabase::clearConfigHashes() const
{
    //db->execute("delete from Configs");
}

String ServiceDatabase::getConfigByHash(const String &settings_hash) const
{
    String c;
    /*db->execute("select config_name from Configs where hash = '" + settings_hash + "'",
                [&c](SQLITE_CALLBACK_ARGS) {
                    c = cols[0];
                    return 0;
                });*/
    return c;
}

void ServiceDatabase::addConfigHash(const String &settings_hash, const String &config, const String &config_hash) const
{
    if (config.empty())
        return;
    //db->execute("replace into Configs values ('" + settings_hash + "', '" + config + "', '" + config_hash + "'" + ")");
}

void ServiceDatabase::removeConfigHashes(const String &h) const
{
    //db->execute("delete from Configs where hash = '" + h + "'");
}

int ServiceDatabase::addConfig(const String &config) const
{
    int id = 0;
    /*db->execute("replace into Configs (name, hash) values ('" + config + "', '" + sha256(config) + "')",
                [&id](SQLITE_CALLBACK_ARGS) {
                    id = std::stoi(cols[0]);
                    return 0;
                });*/
    return id;
}

int ServiceDatabase::getConfig(const String &config) const
{
    int id = 0;
    /*db->execute("select id from Configs where name = '" + config + "'",
                [&id](SQLITE_CALLBACK_ARGS) {
                    id = std::stoi(cols[0]);
                    return 0;
                });
    if (id == 0)
    {
        id = addConfig(config);
        if (id == 0)
            return getConfig(config);
    }*/
    return id;
}

void ServiceDatabase::setPackageDependenciesHash(const PackageId &p, const String &hash) const
{
    //db->execute("replace into PackageDependenciesHashes values ('" + p.target_name + "', '" + hash + "')");
}

bool ServiceDatabase::hasPackageDependenciesHash(const PackageId &p, const String &hash) const
{
    bool has = false;
    /*db->execute("select * from PackageDependenciesHashes where package = '" + p.target_name + "' "
                                                                                              "and dependencies = '" +
                    hash + "'",
                [&has](SQLITE_CALLBACK_ARGS) {
                    has = true;
                    return 0;
                });*/
    return has;
}

void ServiceDatabase::addInstalledPackage(const PackageId &p, PackageVersionGroupNumber group_number) const
{
    auto h = p.getFilesystemHash();
    if (getInstalledPackageHash(p) == h)
        return;

    const auto ipkgs = db::service::InstalledPackage{};
    (*db)(sqlpp::sqlite3::insert_or_replace_into(ipkgs).set(
        ipkgs.path = p.ppath.toString(),
        ipkgs.version = p.version.toString(),
        ipkgs.hash = p.getFilesystemHash(),
        ipkgs.groupNumber = group_number
    ));
}

void ServiceDatabase::removeInstalledPackage(const PackageId &p) const
{
    const auto ipkgs = db::service::InstalledPackage{};
    (*db)(remove_from(ipkgs).where(ipkgs.path == p.ppath.toString() and ipkgs.version == p.version.toString()));
}

String ServiceDatabase::getInstalledPackageHash(const PackageId &p) const
{
    const auto ipkgs = db::service::InstalledPackage{};
    auto q = (*db)(
        custom_query(sqlpp::verbatim("SELECT hash FROM installed_package WHERE path = '" +
            p.ppath.toString() + "'  COLLATE NOCASE AND version = '" + p.version.toString() + "'"))
        .with_result_type_of(select(ipkgs.hash).from(ipkgs))
        );
    //auto q = (*db)(select(ipkgs.hash).from(ipkgs).where(ipkgs.path == p.ppath.toString() and ipkgs.version == p.version.toString()));
    if (q.empty())
        return {};
    return q.front().hash.value();
}

int64_t ServiceDatabase::getInstalledPackageId(const PackageId &p) const
{
    const auto ipkgs = db::service::InstalledPackage{};
    auto q = (*db)(
        custom_query(sqlpp::verbatim("SELECT installed_package_id FROM installed_package WHERE path = '" +
            p.ppath.toString() + "' COLLATE NOCASE AND version = '" + p.version.toString() + "'"))
        .with_result_type_of(select(ipkgs.installedPackageId).from(ipkgs))
        );
    //auto q = (*db)(select(ipkgs.installedPackageId).from(ipkgs).where(ipkgs.path == p.ppath.toString() and ipkgs.version == p.version.toString()));
    if (q.empty())
        return 0;
    return q.front().installedPackageId.value();
}

int ServiceDatabase::getInstalledPackageConfigId(const PackageId &p, const String &config) const
{
    auto pid = getInstalledPackageId(p);
    if (pid == 0)
    {
        LOG_DEBUG(logger, "PackageId is not installed: " + p.target_name);
        return 0;
    }
    int cid = getConfig(config);
    int id = 0;
    /*db->execute("select PackageProperties.id from PackageProperties "
                "where package_id = '" +
                    std::to_string(pid) + "' "
                                          "and config_id = '" +
                    std::to_string(cid) + "'",
                [&id](SQLITE_CALLBACK_ARGS) {
                    id = std::stoi(cols[0]);
                    return 0;
                });
    if (id == 0)
    {
        db->execute("insert into PackageProperties (package_id, config_id) values ("
                    "'" +
                        std::to_string(pid) + "', "
                                              "'" +
                        std::to_string(cid) + "')",
                    [](SQLITE_CALLBACK_ARGS) { return 0; }, true);
        return getInstalledPackageConfigId(p, config);
    }*/
    return id;
}

SomeFlags ServiceDatabase::getInstalledPackageFlags(const PackageId &p, const String &config) const
{
    SomeFlags f = 0;
    /*db->execute("select flags from PackageProperties "
                "where id = '" +
                    std::to_string(getInstalledPackageConfigId(p, config)) + "'",
                [&f](SQLITE_CALLBACK_ARGS) {
                    f = std::stoull(cols[0]);
                    return 0;
                });*/
    return f;
}

void ServiceDatabase::setInstalledPackageFlags(const PackageId &p, const String &config, const SomeFlags &f) const
{
    /*db->execute("update PackageProperties set flags = '" + std::to_string(f.to_ullong()) +
                "' where id = '" + std::to_string(getInstalledPackageConfigId(p, config)) + "'");*/
}

optional<ServiceDatabase::OverriddenPackage> ServiceDatabase::getOverriddenPackage(const PackageId &pkg) const
{
    const auto &pkgs = getOverriddenPackages();
    auto i = pkgs.find(pkg);
    if (i == pkgs.end(pkg))
        return {};
    return i->second;
}

const ServiceDatabase::OverriddenPackages &ServiceDatabase::getOverriddenPackages() const
{
    // maybe move them to packages db?
    if (override_remote_packages)
        return override_remote_packages.value();

    OverriddenPackages pkgs;
    const auto orp = db::service::OverrideRemotePackage{};
    const auto orpv = db::service::OverrideRemotePackageVersion{};
    const auto orpvd = db::service::OverrideRemotePackageVersionDependency{};
    for (const auto &row : (*db)(select(orp.overrideRemotePackageId, orp.path).from(orp).unconditionally()))
    {
        for (const auto &row2 : (*db)(select(orpv.overrideRemotePackageVersionId, orpv.version, orpv.sdir, orpv.prefix).from(orpv).where(orpv.overrideRemotePackageId == row.overrideRemotePackageId)))
        {
            auto &o = pkgs[row.path.value()][row2.version.value()];
            o.id = -row2.overrideRemotePackageVersionId.value();
            o.sdir = row2.sdir.value();
            o.prefix = row2.prefix.value();
            for (const auto &row3 : (*db)(select(orpvd.dependency).from(orpvd).where(orpvd.overrideRemotePackageVersionId == row2.overrideRemotePackageVersionId)))
            {
                o.deps.insert(row3.dependency.value());
            }
        }
    }
    override_remote_packages = pkgs;
    return override_remote_packages.value();
}

void ServiceDatabase::overridePackage(const PackageId &pkg, const OverriddenPackage &opkg) const
{
    getOverriddenPackages(); // init if needed

    override_remote_packages.value().erase(pkg);
    override_remote_packages.value().emplace(pkg, opkg);

    const auto orp = db::service::OverrideRemotePackage{};
    const auto orpv = db::service::OverrideRemotePackageVersion{};
    const auto orpvd = db::service::OverrideRemotePackageVersionDependency{};
    db->start_transaction();
    deleteOverriddenPackage(pkg);
    auto q1 = (*db)(select(orp.overrideRemotePackageId).from(orp).where(
        orp.path == pkg.ppath.toString()
    ));
    if (q1.empty())
    {
        (*db)(insert_into(orp).set(
            orp.path = pkg.ppath.toString()
        ));
    }
    auto q = (*db)(select(orp.overrideRemotePackageId).from(orp).where(
        orp.path == pkg.ppath.toString()
    ));
    (*db)(insert_into(orpv).set(
        orpv.overrideRemotePackageId = q.front().overrideRemotePackageId.value(),
        orpv.version = pkg.version.toString(),
        orpv.sdir = fs::canonical(fs::absolute(opkg.sdir)).u8string(),
        orpv.prefix = opkg.prefix
    ));
    auto q2 = (*db)(select(orpv.overrideRemotePackageVersionId).from(orpv).where(
        orpv.overrideRemotePackageId == q.front().overrideRemotePackageId.value() &&
        orpv.version == pkg.version.toString()
    ));
    for (auto &d : opkg.deps)
    {
        (*db)(insert_into(orpvd).set(
            orpvd.overrideRemotePackageVersionId = q2.front().overrideRemotePackageVersionId.value(),
            orpvd.dependency = d.toString()
        ));
    }
    db->commit_transaction();
}

void ServiceDatabase::deleteOverriddenPackage(const PackageId &pkg) const
{
    const auto orp = db::service::OverrideRemotePackage{};
    const auto orpv = db::service::OverrideRemotePackageVersion{};
    auto q = (*db)(select(orp.overrideRemotePackageId).from(orp).where(
        orp.path == pkg.ppath.toString()
    ));
    if (q.empty())
        return;
    (*db)(remove_from(orpv).where(
        orpv.overrideRemotePackageId == q.front().overrideRemotePackageId.value() &&
        orpv.version == pkg.version.toString()
    ));
}

void ServiceDatabase::deleteOverriddenPackageDir(const path &sdir) const
{
    const auto orpv = db::service::OverrideRemotePackageVersion{};
    (*db)(remove_from(orpv).where(
        orpv.sdir == fs::canonical(fs::absolute(sdir)).u8string()
    ));
}

UnresolvedPackages ServiceDatabase::getOverriddenPackageVersionDependencies(db::PackageVersionId project_version_id)
{
    //project_version_id = abs(project_version_id);
    const auto orpvd = db::service::OverrideRemotePackageVersionDependency{};
    UnresolvedPackages deps;
    for (const auto &row3 : (*db)(select(orpvd.dependency).from(orpvd).where(orpvd.overrideRemotePackageVersionId == project_version_id)))
        deps.insert(row3.dependency.value());
    return deps;
}

Packages ServiceDatabase::getInstalledPackages() const
{
    const auto ipkgs = db::service::InstalledPackage{};
    Packages pkgs;
    for (const auto &row : (*db)(select(ipkgs.path, ipkgs.version).from(ipkgs).unconditionally()))
    {
        Package pkg;
        pkg.ppath = row.path.value();
        pkg.version = row.version.value();
        pkg.createNames();
        pkgs.insert(pkg);
    }
    return pkgs;
}

PackagesDatabase::PackagesDatabase()
    : Database(packages_db_name, packages_db_schema)
{
    db_repo_dir = db_dir / db_repo_dir_name;

    if (created)
    {
        LOG_INFO(logger, "Packages database was not found");
        download();
        load();
    }
    else
        updateDb();

    // at the end we always reopen packages db as read only
    open(true);
}

void PackagesDatabase::download() const
{
    LOG_INFO(logger, "Downloading database");

    fs::create_directories(db_repo_dir);

    auto download_archive = [this]() {
        auto fn = get_temp_filename();
        download_file(db_master_url, fn, 1_GB);
        auto unpack_dir = get_temp_filename();
        auto files = unpack_file(fn, unpack_dir);
        for (auto &f : files)
            fs::copy_file(f, db_repo_dir / f.filename(), fs::copy_options::overwrite_existing);
        fs::remove_all(unpack_dir);
        fs::remove(fn);
    };

    const String git = "git";
    if (!primitives::resolve_executable(git).empty())
    {
        auto git_init = [this, &git]() {
            primitives::Command::execute({git, "-C", db_repo_dir.string(), "init", "."});
            primitives::Command::execute({git, "-C", db_repo_dir.string(), "remote", "add", "github", db_repo_url});
            primitives::Command::execute({git, "-C", db_repo_dir.string(), "pull", "github", "master"});
        };

        try
        {
            if (!fs::exists(db_repo_dir / ".git"))
            {
                git_init();
            }
            else
            {
                std::error_code ec1, ec2;
                primitives::Command::execute({git, "-C", db_repo_dir.string(), "pull", "github", "master"}, ec1);
                primitives::Command::execute({git, "-C", db_repo_dir.string(), "reset", "--hard"}, ec2);
                if (ec1 || ec2)
                {
                    // can throw
                    fs::remove_all(db_repo_dir);
                    git_init();
                }
            }
        }
        catch (const std::exception &)
        {
            // cannot throw
            error_code ec;
            fs::remove_all(db_repo_dir, ec);

            download_archive();
        }
    }
    else
    {
        download_archive();
    }

    writeDownloadTime();
}

void PackagesDatabase::load(bool drop) const
{
    auto &sdb = getServiceDatabase();
    auto sver_old = sdb.getPackagesDbSchemaVersion();
    int sver = readPackagesDbSchemaVersion(db_repo_dir);
    if (sver && sver != PACKAGES_DB_SCHEMA_VERSION)
    {
        if (sver > PACKAGES_DB_SCHEMA_VERSION)
            throw SW_RUNTIME_EXCEPTION("Client's packages db schema version is older than remote one. Please, upgrade the cppan client from site or via --self-upgrade");
        if (sver < PACKAGES_DB_SCHEMA_VERSION)
            throw SW_RUNTIME_EXCEPTION("Client's packages db schema version is newer than remote one. Please, wait for server upgrade");
    }
    if (sver > sver_old)
    {
        // recreate(); ?
        sdb.setPackagesDbSchemaVersion(sver);
    }

    auto mdb = db->native_handle();
    sqlite3_stmt *stmt = nullptr;

    // load only known tables
    // alternative: read csv filenames by mask and load all
    // but we don't do this
    Strings data_tables;
    ::SqliteDatabase db2(fn);
    db2.execute("select name from sqlite_master as tables where type='table' and name not like '/_%' ESCAPE '/';", [&data_tables](SQLITE_CALLBACK_ARGS)
    {
        data_tables.push_back(cols[0]);
        return 0;
    });

    db->execute("PRAGMA foreign_keys = OFF;");

    db->execute("BEGIN;");

    auto split_csv_line = [](auto &s)
    {
        std::replace(s.begin(), s.end(), ',', '\0');
    };

    for (auto &td : data_tables)
    {
        if (drop)
            db->execute("delete from " + td);

        auto fn = db_repo_dir / (td + ".csv");
        std::ifstream ifile(fn);
        if (!ifile)
            throw SW_RUNTIME_EXCEPTION("Cannot open file " + fn.string() + " for reading");

        String s;
        int rc;

        // read first line
        std::getline(ifile, s);
        split_csv_line(s);

        // read fields
        auto b = s.c_str();
        auto e = &s.back() + 1;
        Strings cols;
        cols.push_back(b);
        while (1)
        {
            if (b == e)
                break;
            if (*b++ == 0)
                cols.push_back(b);
        }
        auto n_cols = cols.size();

        // add only them
        String query = "insert into " + td + " (";
        for (auto &c : cols)
            query += c + ", ";
        query.resize(query.size() - 2);
        query += ") values (";
        for (size_t i = 0; i < n_cols; i++)
            query += "?, ";
        query.resize(query.size() - 2);
        query += ");";

        if (sqlite3_prepare_v2(mdb, query.c_str(), (int)query.size() + 1, &stmt, 0) != SQLITE_OK)
            throw SW_RUNTIME_EXCEPTION(sqlite3_errmsg(mdb));

        while (std::getline(ifile, s))
        {
            auto b = s.c_str();
            split_csv_line(s);

            for (int i = 1; i <= n_cols; i++)
            {
                if (*b)
                    sqlite3_bind_text(stmt, i, b, -1, SQLITE_TRANSIENT);
                else
                    sqlite3_bind_null(stmt, i);
                while (*b++);
            }

            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE)
                throw SW_RUNTIME_EXCEPTION("sqlite3_step() failed: "s + sqlite3_errmsg(mdb));
            rc = sqlite3_reset(stmt);
            if (rc != SQLITE_OK)
                throw SW_RUNTIME_EXCEPTION("sqlite3_reset() failed: "s + sqlite3_errmsg(mdb));
        }

        if (sqlite3_finalize(stmt) != SQLITE_OK)
            throw SW_RUNTIME_EXCEPTION("sqlite3_finalize() failed: "s + sqlite3_errmsg(mdb));
    }

    db->execute("COMMIT;");

    db->execute("PRAGMA foreign_keys = ON;");
}

void PackagesDatabase::updateDb() const
{
    if (!Settings::get_system_settings().can_update_packages_db || !isCurrentDbOld())
        return;

    LOG_DEBUG(logger, "Checking remote version");
    int version_remote = 0;
    try
    {
        version_remote = std::stoi(download_file(db_version_url));
    }
    catch (std::exception &e)
    {
        LOG_DEBUG(logger, "Couldn't download db version file: " << e.what());
    }
    if (version_remote > readPackagesDbVersion(db_repo_dir))
    {
        // multiprocess aware
        single_process_job(get_lock("db_update"), [this] {
            download();
            load(true);
        });
    }
}

void PackagesDatabase::writeDownloadTime() const
{
    auto tp = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(tp);
    write_file(db_dir / PACKAGES_DB_DOWNLOAD_TIME_FILE, std::to_string(time));
}

TimePoint PackagesDatabase::readDownloadTime() const
{
    auto fn = db_dir / PACKAGES_DB_DOWNLOAD_TIME_FILE;
    String ts = "0";
    if (fs::exists(fn))
        ts = read_file(fn);
    auto tp = std::chrono::system_clock::from_time_t(std::stoll(ts));
    return tp;
}

bool PackagesDatabase::isCurrentDbOld() const
{
    auto tp_old = readDownloadTime();
    auto tp = std::chrono::system_clock::now();
    return (tp - tp_old) > std::chrono::minutes(PACKAGES_DB_REFRESH_TIME_MINUTES);
}

void PackagesDatabase::preInitFindDependencies() const
{
    // !
    updateDb();

    // set current time
    tstart = getUtc();
}

void PackagesDatabase::findLocalDependencies(IdDependencies &id_deps, const UnresolvedPackages &deps) const
{
    preInitFindDependencies();

    std::map<PackageId, DownloadDependency> all_deps;
    for (auto &[id, dep] : id_deps)
        all_deps.emplace(dep, dep);

    for (auto &dep : deps)
    {
        auto &pkgs = getServiceDatabase().getOverriddenPackages();
        PackageId pkg{ dep.ppath, dep.range.toString() };
        auto i = pkgs.find(pkg);
        if (i != pkgs.end(pkg))
        {
            DownloadDependency project;
            project.id = i->second.id;
            //project.flags.set(pfDirectDependency);
            project.ppath = dep.ppath;
            project.version = i->first;
            project.prefix = i->second.prefix;
            project.db_dependencies = getProjectDependencies(project.id, all_deps, i->second.deps);
            //project.setDependencyIds(getProjectDependencies(project.id, all_deps, i->second.deps)); // see dependency.h note
            all_deps[project] = project; // assign first, deps assign second
            continue;
        }

        // TODO: replace later with typed exception, so client will try to fetch same package from server
        throw SW_RUNTIME_EXCEPTION("PackageId '" + dep.ppath.toString() + "' not found.");
    }

    // mark local deps
    const auto &overridden = getServiceDatabase().getOverriddenPackages();
    if (!overridden.empty())
    {
        for (auto &[pkg, d] : all_deps)
            d.local_override = overridden.find(pkg) != overridden.end(pkg);
    }

    // make id deps
    for (auto &ad : all_deps)
    {
        auto &d = ad.second;
        std::unordered_set<db::PackageVersionId> ids;
        for (auto &dd2 : d.db_dependencies) // see dependency.h note
            ids.insert(dd2.second.id);
        d.setDependencyIds(ids);
        id_deps[d.id] = d;
    }
}

IdDependencies PackagesDatabase::findDependencies(const UnresolvedPackages &deps) const
{
    const auto pkgs = db::packages::Package{};

    preInitFindDependencies();

    const auto &overridden = getServiceDatabase().getOverriddenPackages();

    DependenciesMap all_deps;
    for (auto &dep : deps)
    {
        if (dep.ppath.is_loc())
            continue;

        DownloadDependency project;
        project.ppath = dep.ppath;
        project.range = dep.range;
        auto q = (*db)(
            custom_query(sqlpp::verbatim("SELECT package_id FROM package WHERE path = '" + dep.ppath.toString() + "' COLLATE NOCASE"))
            .with_result_type_of(select(pkgs.packageId).from(pkgs))
            );
        if (q.empty())
        {
            auto &pkgs = overridden;
            PackageId pkg{ dep.ppath, dep.range.toString() };
            auto i = pkgs.find(pkg);
            if (i != pkgs.end(pkg))
            {
                project.id = i->second.id;
                //project.flags.set(pfDirectDependency);
                project.ppath = dep.ppath;
                project.version = i->first;
                project.prefix = i->second.prefix;
                project.db_dependencies = getProjectDependencies(project.id, all_deps, i->second.deps);
                //project.setDependencyIds(getProjectDependencies(project.id, all_deps, i->second.deps)); // see dependency.h note
                all_deps[project] = project; // assign first, deps assign second
                continue;
            }

            // TODO: replace later with typed exception, so client will try to fetch same package from server
            throw SW_RUNTIME_EXCEPTION("PackageId '" + project.ppath.toString() + "' not found.");
        }

        project.id = q.front().packageId.value(); // set package id first, then it is replaced with pkg version id, do not remove
        project.id = getExactProjectVersionId(project, project.version, project.flags, project.hash, project.group_number, project.prefix);
        //project.flags.set(pfDirectDependency);
        all_deps[project] = project; // assign first, deps assign second
        all_deps[project].db_dependencies = getProjectDependencies(project.id, all_deps, getServiceDatabase().getOverriddenPackageVersionDependencies(-project.id));
        //all_deps[project].setDependencyIds(getProjectDependencies(project.id, all_deps)); // see dependency.h note
    }

    // mark local deps
    if (!overridden.empty())
    {
        for (auto &[pkg, d] : all_deps)
            d.local_override = overridden.find(pkg) != overridden.end(pkg);
    }

    // make id deps
    IdDependencies dds;
    for (auto &ad : all_deps)
    {
        auto &d = ad.second;
        std::unordered_set<db::PackageVersionId> ids;
        for (auto &dd2 : d.db_dependencies) // see dependency.h note
            ids.insert(dd2.second.id);
        d.setDependencyIds(ids);
        dds[d.id] = d;
    }
    return dds;
}

void check_version_age(const std::string &created)
{
    auto d = tstart - string2timepoint(created);
    auto mins = std::chrono::duration_cast<std::chrono::minutes>(d).count();
    // multiple by 2 because first time interval goes for uploading db
    // and during the second one, the packet is really young
    //LOG_INFO(logger, "mins " << mins);
    if (mins < PACKAGES_DB_REFRESH_TIME_MINUTES * 2)
        throw std::runtime_error("One of the queried packages is 'young'. Young packages must be retrieved from server.");
}

db::PackageVersionId PackagesDatabase::getExactProjectVersionId(const DownloadDependency &project, Version &version, SomeFlags &flags, String &hash, PackageVersionGroupNumber &gn, int &prefix) const
{
    auto err = [](const auto &p, const auto &r)
    {
        return NoSuchVersion("No suitable version '" + r.toString() + "' for project '" + p.toString() + "'");
    };

    db::PackageVersionId id = 0;
    std::set<Version> versions;
    std::unordered_map<Version, db::PackageVersionId> version_ids;

    const auto pkg_ver = db::packages::PackageVersion{};
    for (const auto &row : (*db)(
        select(pkg_ver.packageVersionId, pkg_ver.version)
        .from(pkg_ver)
        .where(pkg_ver.packageId == project.id)))
    {
        versions.insert(row.version.value());
        version_ids[row.version.value()] = row.packageVersionId.value();
    }

    // also check local db
    auto &o = getServiceDatabase().getOverriddenPackages();
    auto i = o.find(project.ppath);
    if (i != o.end())
    {
        for (auto &[v, d] : i->second)
        {
            versions.insert(v);
            version_ids[v] = d.id;
        }
    }

    auto v = project.range.getMaxSatisfyingVersion(versions);
    if (!v)
        throw err(project.ppath, project.range);

    PackageId pkg{ project.ppath, v.value() };
    auto i2 = o.find(pkg);
    if (i2 != o.end(pkg))
    {
        id = version_ids[v.value()];
        version = v.value();
        prefix = i2->second.prefix;
        gn = i2->second.getGroupNumber();
        return id;
    }

    id = version_ids[v.value()];
    auto q = (*db)(
        select(pkg_ver.hash, pkg_ver.flags, pkg_ver.updated, pkg_ver.groupNumber, pkg_ver.prefix)
        .from(pkg_ver)
        .where(pkg_ver.packageVersionId == id));
    auto &row = q.front();
    version = v.value();
    hash = row.hash.value();
    flags = row.flags.value();
    check_version_age(row.updated.value());
    gn = row.groupNumber.value();
    prefix = (int)row.prefix.value();

    return id;
}

PackagesDatabase::Dependencies PackagesDatabase::getProjectDependencies(db::PackageVersionId project_version_id, DependenciesMap &dm, const UnresolvedPackages &overridden_deps) const
{
    const auto pkgs = db::packages::Package{};
    const auto pkgdeps = db::packages::PackageVersionDependency{};

    Dependencies dependencies;
    std::vector<DownloadDependency> deps;

    if (project_version_id > 0)
    {
        for (const auto &row : (*db)(
            select(pkgs.packageId, pkgs.path, pkgdeps.versionRange)
            .from(pkgdeps.join(pkgs).on(pkgdeps.packageId == pkgs.packageId))
            .where(pkgdeps.packageVersionId == project_version_id)))
        {
            DownloadDependency dependency;
            dependency.id = row.packageId.value();
            dependency.ppath = row.path.value();
            dependency.range = row.versionRange.value();
            dependency.id = getExactProjectVersionId(dependency, dependency.version, dependency.flags, dependency.hash, dependency.group_number, dependency.prefix);
            auto i = dm.find(dependency);
            if (i == dm.end())
            {
                dm[dependency] = dependency; // assign first, deps assign second
                dm[dependency].db_dependencies = getProjectDependencies(dependency.id, dm);
                //dm[dependency].setDependencyIds(getProjectDependencies(dependency.id, dm)); // see dependency.h note
            }
            dependencies[dependency.ppath.toString()] = dependency; // see dependency.h note
        }
    }
    else if (project_version_id < 0) // overridden
    {
        for (const auto &d : overridden_deps)
        {
            DownloadDependency dependency;
            dependency.id = -1;

            // read pkg id
            auto q = (*db)(
                custom_query(sqlpp::verbatim("SELECT package_id FROM package WHERE path = '" + d.ppath.toString() + "' COLLATE NOCASE"))
                .with_result_type_of(select(pkgs.packageId).from(pkgs))
                );
            if (!q.empty())
                dependency.id = q.front().packageId.value();
            //else // not registered yet, remote or local-overridden
            // left as is

            dependency.ppath = d.ppath;
            dependency.range = d.range;
            dependency.id = getExactProjectVersionId(dependency, dependency.version, dependency.flags, dependency.hash, dependency.group_number, dependency.prefix);
            auto i = dm.find(dependency);
            if (i == dm.end())
            {
                dm[dependency] = dependency; // assign first, deps assign second
                dm[dependency].db_dependencies = getProjectDependencies(dependency.id, dm, getServiceDatabase().getOverriddenPackageVersionDependencies(-dependency.id));
                //dm[dependency].setDependencyIds(getProjectDependencies(dependency.id, dm, getServiceDatabase().getOverriddenPackageVersionDependencies(-dependency.id))); // see dependency.h note
            }
            dependencies[dependency.ppath.toString()] = dependency; // see dependency.h note
        }
    }
    return dependencies;
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
        std::set<Version> versions(v1.begin(), v1.end());
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
    DownloadDependency d;
    d.ppath = p.ppath;
    d.id = getPackageId(p.ppath);

    Version v = p.version;
    SomeFlags f;
    String h;
    PackageVersionGroupNumber gn;
    int prefix;
    getExactProjectVersionId(d, v, f, h, gn, prefix);
    return v;
}

template <template <class...> class C>
C<PackagePath> PackagesDatabase::getMatchingPackages(const String &name) const
{
    const auto tpkgs = db::packages::Package{};

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
    const auto vpkgs = db::packages::PackageVersion{};
    for (const auto &row : (*db)(select(vpkgs.version).from(vpkgs).where(vpkgs.packageId == getPackageId(ppath))))
        versions.push_back(row.version.value());
    return versions;
}

db::PackageId PackagesDatabase::getPackageId(const PackagePath &ppath) const
{
    db::PackageId id = 0;
    const auto pkgs = db::packages::Package{};
    auto q = (*db)(select(pkgs.packageId).from(pkgs).where(pkgs.path == ppath.toString()));
    if (!q.empty())
        id = q.front().packageId.value();
    return id;
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
        Package pkg;
        pkg.ppath = p.first;
        pkg.version = p.second;
        pkg.createNames();
        r.insert(pkg);
    }

    return r;
}

Packages PackagesDatabase::getDependentPackages(const Packages &pkgs)
{
    Packages r;
    for (auto &pkg : pkgs)
    {
        auto dpkgs = getDependentPackages(pkg);
        r.insert(dpkgs.begin(), dpkgs.end());
    }

    // exclude input
    for (auto &pkg : pkgs)
        r.erase(pkg);

    return r;
}

Packages PackagesDatabase::getTransitiveDependentPackages(const Packages &pkgs)
{
    auto r = pkgs;
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
            r.insert(dpkgs.begin(), dpkgs.end());
            break;
        }

        if (!changed)
            break;
    }

    // exclude input
    for (auto &pkg : pkgs)
        r.erase(pkg);

    return r;
}

DataSources PackagesDatabase::getDataSources()
{
    const auto ds = db::packages::DataSource{};

    DataSources dss;
    for (const auto &row : (*db)(select(ds.url, ds.flags).from(ds).unconditionally()))
    {
        DataSource s;
        s.raw_url = row.url;
        s.flags = row.flags.value();
        if (!s.flags[DataSource::fDisabled])
            dss.push_back(s);
    }
    return dss;
}

}
