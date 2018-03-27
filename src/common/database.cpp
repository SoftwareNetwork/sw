/*
 * Copyright (C) 2016-2017, Egor Pugin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "database.h"

#include "directories.h"
#include "exceptions.h"
#include "enums.h"
#include "hash.h"
#include "http.h"
#include "lock.h"
#include "settings.h"
#include "sqlite_database.h"
#include "stamp.h"
#include "printers/cmake.h"

#include <primitives/command.h>
#include <primitives/lock.h>
#include <primitives/pack.h>
#include <primitives/templates.h>

#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/nowide/fstream.hpp>
#include <sqlite3.h>

#include <shared_mutex>

#include <primitives/log.h>
//DECLARE_STATIC_LOGGER(logger, "db");

#define PACKAGES_DB_REFRESH_TIME_MINUTES 15

#define PACKAGES_DB_SCHEMA_VERSION 1
#define PACKAGES_DB_SCHEMA_VERSION_FILE "schema.version"
#define PACKAGES_DB_VERSION_FILE "db.version"
#define PACKAGES_DB_DOWNLOAD_TIME_FILE "packages.time"

const String db_repo_url = "https://github.com/cppan/database";
const String db_master_url = db_repo_url + "/archive/master.zip";
const String db_version_url = "https://raw.githubusercontent.com/cppan/database/master/" PACKAGES_DB_VERSION_FILE;

const path db_dir_name = "database";
const path db_repo_dir_name = "repository";
const String packages_db_name = "packages.db";
const String service_db_name = "service.db";

TYPED_EXCEPTION(NoSuchVersion);

std::vector<StartupAction> startup_actions{
    { 1, StartupAction::ClearCache },
    { 2, StartupAction::ServiceDbClearConfigHashes },
    { 4, StartupAction::CheckSchema },
    { 5, StartupAction::ClearStorageDirExp },
    { 6, StartupAction::ClearSourceGroups },
    { 7, StartupAction::ClearStorageDirExp | StartupAction::ClearStorageDirBin | StartupAction::ClearStorageDirLib },
    { 8, StartupAction::ClearCfgDirs },
    { 9, StartupAction::ClearStorageDirExp },
    { 10, StartupAction::ClearPackagesDatabase },
    { 11, StartupAction::ServiceDbClearConfigHashes },
    { 12, StartupAction::ClearStorageDirExp | StartupAction::ClearStorageDirObj },
    { 13, StartupAction::ClearStorageDirExp },
};

const TableDescriptors &get_service_tables()
{
    // to prevent side effects as with global variable
    // ! append new tables to the end only !
    static const TableDescriptors service_tables{

        { "ClientStamp",
        R"(
            CREATE TABLE "ClientStamp" (
                "stamp" INTEGER NOT NULL
            );
        )" },

        {"ConfigHashes",
         R"(
            CREATE TABLE "ConfigHashes" (
                "hash" TEXT NOT NULL,           -- program (settings) hash
                "config" TEXT NOT NULL,         -- config
                "config_hash" TEXT NOT NULL,    -- config hash
                PRIMARY KEY ("hash")
            );
        )"},

        { "FileStamps",
        R"(
            CREATE TABLE "FileStamps" (
                "file" TEXT NOT NULL,
                "stamp" INTEGER NOT NULL,
                PRIMARY KEY ("file")
            );
        )" },

        {"InstalledPackages",
         R"(
            CREATE TABLE "InstalledPackages" (
                "id" INTEGER NOT NULL,
                "package" TEXT NOT NULL,
                "version" TEXT NOT NULL,
                "hash" TEXT NOT NULL,
                PRIMARY KEY ("id"),
                UNIQUE ("package", "version")
            );
        )"},

        {"NextClientVersionCheck",
         R"(
            CREATE TABLE "NextClientVersionCheck" (
                "timestamp" INTEGER NOT NULL
            );
            insert into NextClientVersionCheck values (0);
        )"},

        {"NRuns", // unneeded?
         R"(
            CREATE TABLE "NRuns" (
                "n_runs" INTEGER NOT NULL
            );
            insert into NRuns values (0);
        )"},

        {"PackagesDbSchemaVersion",
         R"(
            CREATE TABLE "PackagesDbSchemaVersion" (
                "version" INTEGER NOT NULL
            );
            insert into PackagesDbSchemaVersion values ()" +
             std::to_string(PACKAGES_DB_SCHEMA_VERSION) + R"();
        )"},

        {"PackageDependenciesHashes",
         R"(
            CREATE TABLE "PackageDependenciesHashes" (
                "package" TEXT NOT NULL,
                "dependencies" TEXT NOT NULL,
                PRIMARY KEY ("package")
            );
        )"},

        { "SourceGroups",
        R"(
            CREATE TABLE "SourceGroups" (
                "id" INTEGER NOT NULL,
                "package_id" INTEGER NOT NULL,
                "path" TEXT NOT NULL,
                PRIMARY KEY ("id"),
                FOREIGN KEY ("package_id") REFERENCES "InstalledPackages" ("id") ON DELETE CASCADE
            );
        )" },

        { "SourceGroupFiles",
        R"(
            CREATE TABLE "SourceGroupFiles" (
                "source_group_id" INTEGER NOT NULL,
                "path" TEXT NOT NULL,
                FOREIGN KEY ("source_group_id") REFERENCES "SourceGroups" ("id") ON DELETE CASCADE
            );
        )" },

        {"StartupActions",
         R"(
            CREATE TABLE "StartupActions" (
                "id" INTEGER NOT NULL,
                "action" INTEGER NOT NULL,
                PRIMARY KEY ("id", "action")
            );
        )"},

        {"TableHashes",
         R"(
            CREATE TABLE "TableHashes" (
                "tbl" TEXT NOT NULL,
                "hash" TEXT NOT NULL,
                PRIMARY KEY ("tbl")
            );
        )"},
    };
    return service_tables;
}

const TableDescriptors data_tables{
    {
        "Projects",
        R"(
            CREATE TABLE "Projects" (
                "id" INTEGER NOT NULL,
                "path" TEXT(2048) NOT NULL,
                "type_id" INTEGER NOT NULL,
                "flags" INTEGER NOT NULL,
                PRIMARY KEY ("id")
            );
            CREATE UNIQUE INDEX "ProjectPath" ON "Projects" ("path" ASC);
        )"
    },
    {
        "ProjectVersions",
        R"(
            CREATE TABLE "ProjectVersions" (
                "id" INTEGER NOT NULL,
                "project_id" INTEGER NOT NULL,
                "major" INTEGER,
                "minor" INTEGER,
                "patch" INTEGER,
                "branch" TEXT,
                "flags" INTEGER NOT NULL,
                "created" DATE NOT NULL,
                "hash" TEXT NOT NULL,
                PRIMARY KEY ("id"),
                FOREIGN KEY ("project_id") REFERENCES "Projects" ("id")
            );
        )"
    },
    {
        "ProjectVersionDependencies",
        R"(
            CREATE TABLE "ProjectVersionDependencies" (
                "project_version_id" INTEGER NOT NULL,
                "project_dependency_id" INTEGER NOT NULL,
                "version" TEXT NOT NULL,
                "flags" INTEGER NOT NULL,
                PRIMARY KEY ("project_version_id", "project_dependency_id"),
                FOREIGN KEY ("project_version_id") REFERENCES "ProjectVersions" ("id"),
                FOREIGN KEY ("project_dependency_id") REFERENCES "Projects" ("id")
            );
        )"
    },
};

path getDbDirectory()
{
    // db per storage
    return directories.storage_dir_etc / db_dir_name;
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

    thread_local
    ServiceDatabase db;
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
    // this holder will init on-disk pkgdb once
    // later thread local calls will just open it
    static PackagesDatabase run_once_db;

    thread_local
    PackagesDatabase db;
    return db;
}

Database::Database(const String &name, const TableDescriptors &tds)
    : tds(tds)
{
    db_dir = getDbDirectory();
    fn = db_dir / name;
    if (!fs::exists(fn))
    {
        ScopedFileLock lock(fn);
        if (!fs::exists(fn))
        {
            open();
            for (auto &td : tds)
                db->execute(td.query);
            created = true;
        }
    }
    if (!db)
        open();
}

void Database::open(bool read_only)
{
    db = std::make_unique<SqliteDatabase>(fn.string(), read_only);
}

void Database::recreate()
{
    db.reset();
    ScopedFileLock lock(fn);
    fs::remove(fn);
    db = std::make_unique<SqliteDatabase>(fn.string());
    for (auto &td : tds)
        db->execute(td.query);
    created = true;
}

ServiceDatabase::ServiceDatabase()
    : Database(service_db_name, get_service_tables())
{
}

void ServiceDatabase::init()
{
    RUN_ONCE
    {
        createTables();
        checkStamp();
        increaseNumberOfRuns();
        checkForUpdates();
    };

    // move out of RUN_ONCE because it may try to init sdb again
    performStartupActions();
}

void ServiceDatabase::createTables() const
{
    // add table hashes
    if (created)
    {
        for (auto &td : tds)
            setTableHash(td.name, sha256(td.query));
    }

    auto create_table = [this](const auto &td)
    {
        db->execute(td.query);
        setTableHash(td.name, sha256(td.query));
    };

    // TableHashes first, out of order
    auto th = std::find_if(tds.begin(), tds.end(), [](const auto &td)
    {
        return td.name == "TableHashes";
    });
    if (!db->getNumberOfColumns(th->name))
        create_table(*th);

    // create only new tables
    for (auto &td : tds)
    {
        if (db->getNumberOfColumns(td.name))
            continue;
        create_table(td);
    }
}

void ServiceDatabase::recreateTable(const TableDescriptor &td) const
{
    db->dropTable(td.name);
    db->execute(td.query);
    setTableHash(td.name, sha256(td.query));
}

void ServiceDatabase::checkStamp() const
{
    String s;
    db->execute("select * from ClientStamp",
        [&s](SQLITE_CALLBACK_ARGS)
    {
        s = cols[0];
        return 0;
    });

    if (s == cppan_stamp)
        return;

    if (s.empty())
        db->execute("replace into ClientStamp values ('" + cppan_stamp + "')");
    else
        db->execute("update ClientStamp set stamp = '" + cppan_stamp + "'");

    // if stamp is changed, we do some usual stuff between versions

    clearFileStamps();
}

void ServiceDatabase::performStartupActions() const
{
    registerCmakePackage();

    // perform startup actions on client update
    try
    {
        static bool once = false;
        if (once)
            return;

        std::set<int> actions_performed; // prevent multiple execution of the same actions
        for (auto &a : startup_actions)
        {
            if (isActionPerformed(a))
                continue;

            if (actions_performed.find(a.action) != actions_performed.end())
            {
                setActionPerformed(a);
                continue;
            }

            if (!once)
                LOG_INFO(logger, "Initializing storage");
            once = true;

            actions_performed.insert(a.action);
            setActionPerformed(a);

            // do actions
            if (a.action & StartupAction::ClearCache)
            {
                CMakePrinter().clear_cache();
            }

            if (a.action & StartupAction::ServiceDbClearConfigHashes)
            {
                clearConfigHashes();

                // also cleanup temp build dir
                boost::system::error_code ec;
                fs::remove_all(temp_directory_path(), ec);
            }

            if (a.action & StartupAction::CheckSchema)
            {
                // create new tables
                createTables();

                // re-create changed tables
                for (auto &td : tds)
                {
                    auto h = sha256(td.query);
                    if (getTableHash(td.name) == h)
                        continue;
                    db->dropTable(td.name);
                    db->execute(td.query);
                    setTableHash(td.name, h);
                }
            }

            if (a.action & StartupAction::ClearPackagesDatabase)
            {
                fs::remove(getDbDirectory() / packages_db_name);
            }

            if (a.action & StartupAction::ClearStorageDirExp)
            {
                remove_all_from_dir(directories.storage_dir_exp);
            }

            if (a.action & StartupAction::ClearStorageDirObj)
            {
                remove_all_from_dir(directories.storage_dir_obj);
            }

            if (a.action & StartupAction::ClearStorageDirBin)
            {
                // also remove exp to trigger cmake
                remove_all_from_dir(directories.storage_dir_exp);
                remove_all_from_dir(directories.storage_dir_bin);
            }

            if (a.action & StartupAction::ClearStorageDirLib)
            {
                // also remove exp to trigger cmake
                remove_all_from_dir(directories.storage_dir_exp);
                remove_all_from_dir(directories.storage_dir_lib);
            }

            if (a.action & StartupAction::ClearSourceGroups)
            {
                clearSourceGroups();
            }

            if (a.action & StartupAction::ClearCfgDirs)
            {
                for (auto &i : boost::make_iterator_range(fs::directory_iterator(directories.storage_dir_cfg), {}))
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
        if (Settings::get_user_settings().checkForUpdates())
            setLastClientUpdateCheck(last_check + 20min);
        else
            setLastClientUpdateCheck();
    }
    catch (...)
    {
    }
}

TimePoint ServiceDatabase::getLastClientUpdateCheck() const
{
    TimePoint tp;
    db->execute("select * from NextClientVersionCheck",
        [&tp](SQLITE_CALLBACK_ARGS)
    {
        tp = Clock::from_time_t(std::stoll(cols[0]));
        return 0;
    });
    return tp;
}

void ServiceDatabase::setLastClientUpdateCheck(const TimePoint &p) const
{
    db->execute("update NextClientVersionCheck set timestamp = '" +
        std::to_string(Clock::to_time_t(p)) + "'");
}

String ServiceDatabase::getTableHash(const String &table) const
{
    String h;
    db->execute("select hash from TableHashes where tbl = '" + table + "'",
        [&h](SQLITE_CALLBACK_ARGS)
    {
        h = cols[0];
        return 0;
    });
    return h;
}

void ServiceDatabase::setTableHash(const String &table, const String &hash) const
{
    db->execute("replace into TableHashes values ('" + table + "', '" + hash + "')");
}

Stamps ServiceDatabase::getFileStamps() const
{
    Stamps st;
    db->execute("select * from FileStamps",
        [&st](SQLITE_CALLBACK_ARGS)
    {
        st[cols[0]] = std::stoll(cols[1]);
        return 0;
    });
    return st;
}

void ServiceDatabase::setFileStamps(const Stamps &stamps) const
{
    if (stamps.empty())
    {
        clearFileStamps();
        return;
    }
    String q = "replace into FileStamps values ";
    for (auto &s : stamps)
        q += "('" + normalize_path(s.first) + "', '" + std::to_string(s.second) + "'),";
    q.resize(q.size() - 1);
    q += ";";
    db->execute(q);
}

void ServiceDatabase::clearFileStamps() const
{
    db->execute("delete from FileStamps");
}

bool ServiceDatabase::isActionPerformed(const StartupAction &action) const
{
    int n = 0;
    try
    {
        db->execute("select count(*) from StartupActions where id = '" +
            std::to_string(action.id) + "' and action = '" + std::to_string(action.action) + "'",
            [&n](SQLITE_CALLBACK_ARGS)
        {
            n = std::stoi(cols[0]);
            return 0;
        });
    }
    catch (const std::exception&)
    {
        // if error is in StartupActions, recreate it
        auto th = std::find_if(tds.begin(), tds.end(), [](const auto &td)
        {
            return td.name == "StartupActions";
        });
        recreateTable(*th);
    }
    return n == 1;
}

void ServiceDatabase::setActionPerformed(const StartupAction &action) const
{
    db->execute("insert into StartupActions values ('" +
        std::to_string(action.id) + "', '" + std::to_string(action.action) + "')");
}

int ServiceDatabase::getNumberOfRuns() const
{
    int n_runs = 0;
    db->execute("select n_runs from NRuns;", [&n_runs](SQLITE_CALLBACK_ARGS)
    {
        n_runs = std::stoi(cols[0]);
        return 0;
    });
    return n_runs;
}

int ServiceDatabase::increaseNumberOfRuns() const
{
    auto prev = getNumberOfRuns();
    db->execute("update NRuns set n_runs = n_runs + 1;");
    return prev;
}

int ServiceDatabase::getPackagesDbSchemaVersion() const
{
    int version = 0;
    db->execute("select version from PackagesDbSchemaVersion;", [&version](SQLITE_CALLBACK_ARGS)
    {
        version = std::stoi(cols[0]);
        return 0;
    });
    return version;
}

void ServiceDatabase::setPackagesDbSchemaVersion(int version) const
{
    db->execute("update PackagesDbSchemaVersion set version = " + std::to_string(version));
}

void ServiceDatabase::clearConfigHashes() const
{
    db->execute("delete from ConfigHashes");
}

String ServiceDatabase::getConfigByHash(const String &settings_hash) const
{
    String c;
    db->execute("select config from ConfigHashes where hash = '" + settings_hash + "'",
        [&c](SQLITE_CALLBACK_ARGS)
    {
        c = cols[0];
        return 0;
    });
    return c;
}

void ServiceDatabase::addConfigHash(const String &settings_hash, const String &config, const String &config_hash) const
{
    if (config.empty())
        return;
    db->execute("replace into ConfigHashes values ('" + settings_hash + "', '" + config + "', '" + config_hash + "'" + ")");
}

void ServiceDatabase::removeConfigHashes(const String &h) const
{
    db->execute("delete from ConfigHashes where config_hash = '" + h + "'");
}

void ServiceDatabase::setPackageDependenciesHash(const Package &p, const String &hash) const
{
    db->execute("replace into PackageDependenciesHashes values ('" + p.target_name + "', '" + hash + "')");
}

bool ServiceDatabase::hasPackageDependenciesHash(const Package &p, const String &hash) const
{
    bool has = false;
    db->execute("select * from PackageDependenciesHashes where package = '" + p.target_name + "' "
        "and dependencies = '" + hash + "'",
        [&has](SQLITE_CALLBACK_ARGS)
    {
        has = true;
        return 0;
    });
    return has;
}

void ServiceDatabase::setSourceGroups(const Package &p, const SourceGroups &sgs) const
{
    auto id = getInstalledPackageId(p);
    if (id == 0)
        return;
    removeSourceGroups(id);
    for (auto &sg : sgs)
    {
        db->execute("insert into SourceGroups (package_id, path) values ('" + std::to_string(id) + "', '" + sg.first + "');");
        if (!sg.second.empty())
        {
            auto sg_id = db->getLastRowId();
            String q = "insert into SourceGroupFiles values ";
            for (auto &f : sg.second)
                q += "('" + std::to_string(sg_id) + "', '" + f + "'),";
            q.resize(q.size() - 1);
            q += ";";
            db->execute(q);
        }
    }
}

SourceGroups ServiceDatabase::getSourceGroups(const Package &p) const
{
    SourceGroups sgs;
    auto id = getInstalledPackageId(p);
    if (id == 0)
        return sgs;
    std::map<int, String> ids;
    db->execute("select id, path from SourceGroups where package_id = '" + std::to_string(id) + "';",
        [&ids](SQLITE_CALLBACK_ARGS)
    {
        ids[std::stoi(cols[0])] = cols[1];
        return 0;
    });
    for (auto &i : ids)
    {
        auto &sg = sgs[i.second];
        db->execute("select path from SourceGroupFiles where source_group_id = '" + std::to_string(i.first) + "';",
            [&sg](SQLITE_CALLBACK_ARGS)
        {
            sg.insert(cols[0]);
            return 0;
        });
    }
    return sgs;
}

void ServiceDatabase::removeSourceGroups(const Package &p) const
{
    auto id = getInstalledPackageId(p);
    if (id == 0)
        return;
    removeSourceGroups(id);
}

void ServiceDatabase::removeSourceGroups(int id) const
{
    db->execute("delete from SourceGroups where package_id = '" + std::to_string(id) + "';");
}

void ServiceDatabase::clearSourceGroups() const
{
    db->execute("delete from SourceGroupFiles;");
    db->execute("delete from SourceGroups;");
}

void ServiceDatabase::addInstalledPackage(const Package &p) const
{
    auto h = p.getFilesystemHash();
    if (getInstalledPackageHash(p) == h)
        return;
    db->execute("replace into InstalledPackages (package, version, hash) values ('" + p.ppath.toString() + "', '" + p.version.toString() + "', '" + p.getFilesystemHash() + "')");
}

void ServiceDatabase::removeInstalledPackage(const Package &p) const
{
    db->execute("delete from InstalledPackages where package = '" + p.ppath.toString() + "' and version = '" + p.version.toString() + "'");
}

String ServiceDatabase::getInstalledPackageHash(const Package &p) const
{
    String hash;
    db->execute("select hash from InstalledPackages where package = '" + p.ppath.toString() + "' and version = '" + p.version.toString() + "'",
        [&hash](SQLITE_CALLBACK_ARGS)
    {
        hash = cols[0];
        return 0;
    });
    return hash;
}

int ServiceDatabase::getInstalledPackageId(const Package &p) const
{
    int id = 0;
    db->execute("select id from InstalledPackages where package = '" + p.ppath.toString() + "' and version = '" + p.version.toString() + "'",
        [&id](SQLITE_CALLBACK_ARGS)
    {
        id = std::stoi(cols[0]);
        return 0;
    });
    return id;
}

PackagesSet ServiceDatabase::getInstalledPackages() const
{
    std::set<std::pair<String, String>> pkgs_s;
    db->execute("select package, version from InstalledPackages",
        [&pkgs_s](SQLITE_CALLBACK_ARGS)
    {
        pkgs_s.insert({ cols[0], cols[1] });
        return 0;
    });

    PackagesSet pkgs;
    for (auto &p : pkgs_s)
    {
        Package pkg;
        pkg.ppath = p.first;
        pkg.version = p.second;
        pkg.createNames();
        pkgs.insert(pkg);
    }
    return pkgs;
}

PackagesDatabase::PackagesDatabase()
    : Database(packages_db_name, data_tables)
{
    db_repo_dir = db_dir / db_repo_dir_name;

    RUN_ONCE
    {
        init();
    };

    // at the end we always reopen packages db as read only
    open(true);
}

void PackagesDatabase::init()
{
    if (created)
    {
        LOG_INFO(logger, "Packages database was not found");
        download();
        load();
    }
    else if (Settings::get_system_settings().can_update_packages_db && isCurrentDbOld())
    {
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
            single_process_job(get_lock("db_update"), [this]
            {
                download();
                load(true);
            });
        }
    }
}

void PackagesDatabase::download()
{
    LOG_INFO(logger, "Downloading database");

    auto download_archive = [this]()
    {
        fs::create_directories(db_repo_dir);
        auto fn = get_temp_filename();
        download_file(db_master_url, fn, 1_GB);
        auto unpack_dir = get_temp_filename();
        auto files = unpack_file(fn, unpack_dir);
        for (auto &f : files)
            fs::copy_file(f, db_repo_dir / f.filename(), fs::copy_option::overwrite_if_exists);
        fs::remove_all(unpack_dir);
        fs::remove(fn);
    };

    const String git = "git";
    if (!primitives::resolve_executable(git).empty())
    {
        auto git_init = [this, &git]()
        {
            fs::create_directories(db_repo_dir);
            primitives::Command::execute({ git,"-C",db_repo_dir.string(),"init","." });
            primitives::Command::execute({ git,"-C",db_repo_dir.string(),"remote","add","github",db_repo_url });
            primitives::Command::execute({ git,"-C",db_repo_dir.string(),"pull","github","master" });
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
                primitives::Command::execute({ git,"-C",db_repo_dir.string(),"pull","github","master" }, ec1);
                primitives::Command::execute({ git,"-C",db_repo_dir.string(),"reset","--hard" }, ec2);
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
            boost::system::error_code ec;
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

void PackagesDatabase::load(bool drop)
{
    auto &sdb = getServiceDatabase();
    auto sver_old = sdb.getPackagesDbSchemaVersion();
    int sver = readPackagesDbSchemaVersion(db_repo_dir);
    if (sver && sver != PACKAGES_DB_SCHEMA_VERSION)
    {
        if (sver > PACKAGES_DB_SCHEMA_VERSION)
            throw std::runtime_error("Client's packages db schema version is older than remote one. Please, upgrade the cppan client from site or via --self-upgrade");
        if (sver < PACKAGES_DB_SCHEMA_VERSION)
            throw std::runtime_error("Client's packages db schema version is newer than remote one. Please, wait for server upgrade");
    }
    if (sver > sver_old)
    {
        recreate();
        sdb.setPackagesDbSchemaVersion(sver);
    }

    db->execute("PRAGMA foreign_keys = OFF;");

    auto mdb = db->getDb();
    sqlite3_stmt *stmt = nullptr;

    db->execute("BEGIN;");

    for (auto &td : data_tables)
    {
        if (drop)
            db->execute("delete from " + td.name);

        auto n_cols = db->getNumberOfColumns(td.name);

        String query = "insert into " + td.name + " values (";
        for (int i = 0; i < n_cols; i++)
            query += "?, ";
        query.resize(query.size() - 2);
        query += ");";

        if (sqlite3_prepare_v2(mdb, query.c_str(), (int)query.size() + 1, &stmt, 0) != SQLITE_OK)
            throw std::runtime_error(sqlite3_errmsg(mdb));

        auto fn = db_repo_dir / (td.name + ".csv");
        boost::nowide::ifstream ifile(fn.string());
        if (!ifile)
            throw std::runtime_error("Cannot open file " + fn.string() + " for reading");

        String s;
        while (std::getline(ifile, s))
        {
            auto b = s.c_str();
            std::replace(s.begin(), s.end(), ';', '\0');

            for (int i = 1; i <= n_cols; i++)
            {
                if (*b)
                    sqlite3_bind_text(stmt, i, b, -1, SQLITE_TRANSIENT);
                else
                    sqlite3_bind_null(stmt, i);
                while (*b) b++;
                b++;
            }

            if (sqlite3_step(stmt) != SQLITE_DONE)
                throw std::runtime_error("sqlite3_step() failed");
            if (sqlite3_reset(stmt) != SQLITE_OK)
                throw std::runtime_error("sqlite3_reset() failed");
        }

        if (sqlite3_finalize(stmt) != SQLITE_OK)
            throw std::runtime_error("sqlite3_finalize() failed");
    }

    db->execute("COMMIT;");

    db->execute("PRAGMA foreign_keys = ON;");
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

IdDependencies PackagesDatabase::findDependencies(const Packages &deps) const
{
    DependenciesMap all_deps;
    for (auto &dep : deps)
    {
        if (dep.second.flags[pfLocalProject])
            continue;

        ProjectType type;
        DownloadDependency project;
        project.ppath = dep.second.ppath;
        project.version = dep.second.version;

        db->execute("select id, type_id, flags from Projects where path = '" + dep.second.ppath.toString() + "'",
            [&project, &type](SQLITE_CALLBACK_ARGS)
        {
            project.id = std::stoull(cols[0]);
            type = (ProjectType)std::stoi(cols[1]);
            project.flags = std::stoull(cols[2]);
            return 0;
        });

        if (project.id == 0)
            // TODO: replace later with typed exception, so client will try to fetch same package from server
            throw std::runtime_error("Package '" + project.ppath.toString() + "' not found.");

        auto find_deps = [&all_deps, this](auto &dependency)
        {
            dependency.flags.set(pfDirectDependency);
            dependency.id = getExactProjectVersionId(dependency, dependency.version, dependency.flags, dependency.hash);
            all_deps[dependency] = dependency; // assign first, deps assign second
            all_deps[dependency].db_dependencies = getProjectDependencies(dependency.id, all_deps);
        };

        if (type == ProjectType::RootProject)
        {
            std::vector<DownloadDependency> projects;

            // root projects should return all children (lib, exe)
            db->execute("select id, path, flags from Projects where path like '" + project.ppath.toString() +
                ".%' and type_id in ('1','2') order by path",
                [&projects, &project](SQLITE_CALLBACK_ARGS)
            {
                DownloadDependency dep;
                dep.id = std::stoull(cols[0]);
                dep.ppath = String(cols[1]);
                dep.version = project.version;
                dep.flags = std::stoull(cols[2]);
                projects.push_back(dep);
                return 0;
            });

            if (projects.empty())
                // TODO: replace later with typed exception, so client will try to fetch same package from server
                throw std::runtime_error("Root project '" + project.ppath.toString() + "' is empty");

            int n = 0;
            for (auto &p : projects)
            {
                try
                {
                    find_deps(p);
                    n++;
                }
                catch (NoSuchVersion &)
                {
                }
            }
            if (n == 0)
            {
                throw NoSuchVersion("No such version/branch '" + project.version.toAnyVersion() + "' for project '" +
                    project.ppath.toString() + "'");
            }
        }
        else
        {
            find_deps(project);
        }
    }

    // make id deps
    IdDependencies dds;
    for (auto &ad : all_deps)
    {
        auto &d = ad.second;
        std::unordered_set<ProjectVersionId> ids;
        for (auto &dd2 : d.db_dependencies)
            ids.insert(dd2.second.id);
        d.setDependencyIds(ids);
        dds[d.id] = d;
    }
    return dds;
}

void check_version_age(const TimePoint &t1, const char *created)
{
    auto d = t1 - string2timepoint(created);
    auto mins = std::chrono::duration_cast<std::chrono::minutes>(d).count();
    // multiple by 2 because first time interval goes for uploading db
    // and during the second one, the packet is really young
    if (mins < PACKAGES_DB_REFRESH_TIME_MINUTES * 2)
        throw std::runtime_error("One of the queried packages is 'young'. Young packages must be retrieved from server.");
}

ProjectVersionId PackagesDatabase::getExactProjectVersionId(const DownloadDependency &project, Version &version, ProjectFlags &flags, String &hash) const
{
    auto err = [](const auto &v, const auto &p)
    {
        return NoSuchVersion("No such version/branch '" + v.toAnyVersion() + "' for project '" + p.toString() + "'");
    };

    // save current time during first call
    // it is used for detecting young packages
    static auto tstart = getUtc();

    ProjectVersionId id = 0;
    static const String select = "select id, major, minor, patch, flags, hash, created from ProjectVersions where ";

    if (!version.isBranch())
    {
        auto &v = version;

        db->execute(
            select + " "
            "project_id = '" + std::to_string(project.id) + "' and "
            "major = '" + std::to_string(v.major) + "' and "
            "minor = '" + std::to_string(v.minor) + "' and "
            "patch = '" + std::to_string(v.patch) + "'", [&id, &flags, &hash](SQLITE_CALLBACK_ARGS)
        {
            id = std::stoull(cols[0]);
            flags |= decltype(project.flags)(std::stoull(cols[4]));
            hash = cols[5];
            check_version_age(tstart, cols[6]);
            return 0;
        });

        if (id == 0)
        {
            if (v.patch != -1)
                throw err(version, project.ppath);

            db->execute(
                select + " "
                "project_id = '" + std::to_string(project.id) + "' and "
                "major = '" + std::to_string(v.major) + "' and "
                "minor = '" + std::to_string(v.minor) + "' and "
                "branch is null order by major desc, minor desc, patch desc limit 1",
                [&id, &version, &flags, &hash](SQLITE_CALLBACK_ARGS)
            {
                id = std::stoull(cols[0]);
                version.patch = std::stoi(cols[3]);
                flags |= decltype(project.flags)(std::stoull(cols[4]));
                hash = cols[5];
                check_version_age(tstart, cols[6]);
                return 0;
            });

            if (id == 0)
            {
                if (v.minor != -1)
                    throw err(version, project.ppath);

                db->execute(
                    select + " "
                    "project_id = '" + std::to_string(project.id) + "' and "
                    "major = '" + std::to_string(v.major) + "' and "
                    "branch is null order by major desc, minor desc, patch desc limit 1",
                    [&id, &version, &flags, &hash](SQLITE_CALLBACK_ARGS)
                {
                    id = std::stoull(cols[0]);
                    version.minor = std::stoi(cols[2]);
                    version.patch = std::stoi(cols[3]);
                    flags |= decltype(project.flags)(std::stoull(cols[4]));
                    hash = cols[5];
                    check_version_age(tstart, cols[6]);
                    return 0;
                });

                if (id == 0)
                {
                    if (v.major != -1)
                        throw err(version, project.ppath);

                    db->execute(
                        select + " "
                        "project_id = '" + std::to_string(project.id) + "' and "
                        "branch is null order by major desc, minor desc, patch desc limit 1",
                        [&id, &version, &flags, &hash](SQLITE_CALLBACK_ARGS)
                    {
                        id = std::stoull(cols[0]);
                        version.major = std::stoi(cols[1]);
                        version.minor = std::stoi(cols[2]);
                        version.patch = std::stoi(cols[3]);
                        flags |= decltype(project.flags)(std::stoull(cols[4]));
                        hash = cols[5];
                        check_version_age(tstart, cols[6]);
                        return 0;
                    });

                    if (id == 0)
                    {
                        // TODO:
                        throw err(version, project.ppath);
                    }
                }
            }
        }
    }
    else
    {
        db->execute(
            select + " "
            "project_id = '" + std::to_string(project.id) + "' and "
            "branch = '" + version.toString() + "'", [&id, &flags, &hash](SQLITE_CALLBACK_ARGS)
        {
            id = std::stoull(cols[0]);
            flags |= decltype(project.flags)(std::stoull(cols[4]));
            hash = cols[5];
            check_version_age(tstart, cols[6]);
            return 0;
        });

        if (id == 0)
        {
            // TODO:
            throw err(version, project.ppath);
        }
    }

    return id;
}

PackagesDatabase::Dependencies PackagesDatabase::getProjectDependencies(ProjectVersionId project_version_id, DependenciesMap &dm) const
{
    Dependencies dependencies;
    std::vector<DownloadDependency> deps;

    db->execute(
        "select Projects.id, path, version, Projects.flags, ProjectVersionDependencies.flags "
        "from ProjectVersionDependencies join Projects on project_dependency_id = Projects.id "
        "where project_version_id = '" + std::to_string(project_version_id) + "' order by path",
        [&deps](SQLITE_CALLBACK_ARGS)
    {
        int col_id = 0;
        DownloadDependency d;
        d.id = std::stoull(cols[col_id++]);
        d.ppath = cols[col_id++];
        d.version = String(cols[col_id++]);
        d.flags = decltype(d.flags)(std::stoull(cols[col_id++])); // project's flags
        d.flags |= decltype(d.flags)(std::stoull(cols[col_id++])); // merge with deps' flags
        deps.push_back(d);
        return 0;
    });

    for (auto &dependency : deps)
    {
        dependency.id = getExactProjectVersionId(dependency, dependency.version, dependency.flags, dependency.hash);
        auto i = dm.find(dependency);
        if (i == dm.end())
        {
            dm[dependency] = dependency; // assign first, deps assign second
            dm[dependency].db_dependencies = getProjectDependencies(dependency.id, dm);
        }
        dependencies[dependency.ppath.toString()] = dependency;
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
        auto versions = getVersionsForPackage(pkg);
        String out = pkg.toString();
        out += " (";
        for (auto &v : versions)
            out += v.toString() + ", ";
        out.resize(out.size() - 2);
        out += ")";
        LOG_INFO(logger, out);
    }
}

Version PackagesDatabase::getExactVersionForPackage(const Package &p) const
{
    DownloadDependency d;
    d.ppath = p.ppath;
    d.id = getPackageId(p.ppath);

    Version v = p.version;
    ProjectFlags f;
    String h;
    getExactProjectVersionId(d, v, f, h);
    return v;
}

template <template <class...> class C>
C<ProjectPath> PackagesDatabase::getMatchingPackages(const String &name) const
{
    C<ProjectPath> pkgs;
    String q;
    if (name.empty())
        q = "select path from Projects where type_id <> '3' order by path";
    else
        q = "select path from Projects where type_id <> '3' and path like '%" + name + "%' order by path";
    db->execute(q, [&pkgs](SQLITE_CALLBACK_ARGS)
    {
        pkgs.insert(String(cols[0]));
        return 0;
    });
    return pkgs;
}

template std::unordered_set<ProjectPath>
PackagesDatabase::getMatchingPackages<std::unordered_set>(const String &) const;

template std::set<ProjectPath>
PackagesDatabase::getMatchingPackages<std::set>(const String &) const;

std::vector<Version> PackagesDatabase::getVersionsForPackage(const ProjectPath &ppath) const
{
    std::vector<Version> versions;
    db->execute(
        "select case when branch is not null then branch else major || '.' || minor || '.' || patch end as version "
        "from ProjectVersions where project_id = '" + std::to_string(getPackageId(ppath)) + "' order by branch, major, minor, patch",
        [&versions](SQLITE_CALLBACK_ARGS)
    {
        versions.push_back(String(cols[0]));
        return 0;
    });
    return versions;
}

ProjectId PackagesDatabase::getPackageId(const ProjectPath &ppath) const
{
    ProjectId id = 0;
    db->execute("select id from Projects where path = '" + ppath.toString() + "'", [&id](SQLITE_CALLBACK_ARGS)
    {
        id = std::stoi(cols[0]);
        return 0;
    });
    return id;
}

PackagesSet PackagesDatabase::getDependentPackages(const Package &pkg)
{
    PackagesSet r;

    // 1. Find current project version id.
    ProjectId project_id = getPackageId(pkg.ppath);

    // 2. Find project versions dependent on this version.
    // Probably set to ProjectVersionId, String, String to prevent throwing exceptions, but left as is for now.
    std::set<std::tuple<Version, String, String>> pkgs_s;
    db->execute(
        R"(select version, path,
        case when branch is not null then branch else major || '.' || minor || '.' || patch end as version2
        from ProjectVersionDependencies
        join ProjectVersions on ProjectVersions.id = project_version_id
        join Projects on Projects.id = project_id
        where project_dependency_id = ')" + std::to_string(project_id) + "'",
        [&pkgs_s](SQLITE_CALLBACK_ARGS)
    {
        pkgs_s.emplace(cols[0], cols[1], cols[2]);
        return 0;
    });

    // 3. Match versions.
    for (auto &p : pkgs_s)
    {
        auto &v = std::get<0>(p);
        if (v == pkg.version || v.canBe(pkg.version))
        {
            Package d;
            d.ppath = std::get<1>(p);
            d.version = std::get<2>(p);
            d.createNames();
            r.insert(d);
        }
    }

    return r;
}

PackagesSet PackagesDatabase::getDependentPackages(const PackagesSet &pkgs)
{
    PackagesSet r;
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

PackagesSet PackagesDatabase::getTransitiveDependentPackages(const PackagesSet &pkgs)
{
    using Retrieved = std::unordered_map<Package, PackagesSet>;

    static std::shared_mutex m;
    static Retrieved retrieved;

    auto r = pkgs;
    while (1)
    {
        bool changed = false;

        for (auto &pkg : r)
        {
            {
                std::shared_lock<std::shared_mutex> lk(m);
                auto i = retrieved.find(pkg);
                if (i != retrieved.end())
                {
                    r.insert(i->second.begin(), i->second.end());
                    continue;
                }
            }

            changed = true;

            auto dpkgs = getDependentPackages(pkg);
            r.insert(dpkgs.begin(), dpkgs.end());
            {
                std::unique_lock<std::shared_mutex> lk(m);
                retrieved[pkg] = dpkgs;
            }
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
