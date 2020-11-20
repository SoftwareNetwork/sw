// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>

#include "storage_remote.h"

#include "api.h"
#include "package_database.h"
#include "remote.h"
#include "settings.h"

#include <primitives/command.h>
#include <primitives/csv.h>
#include <primitives/exceptions.h>
#include <primitives/executor.h>
#include <primitives/lock.h>
#include <primitives/pack.h>
#include <sqlite3.h>
#include <sqlpp11/sqlite3/connection.h>

#include <fstream>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "storage");

#define PACKAGES_DB_REFRESH_TIME_MINUTES 15
#define PACKAGES_DB_DOWNLOAD_TIME_FILE "packages.time"

static const String packages_db_name = "packages.db";

namespace sw
{

RemoteStorage::RemoteStorage(LocalStorage &ls, const Remote &r, bool allow_network)
    : StorageWithPackagesDatabase(r.name, ls.getDatabaseRootDir() / "remote")
    , r(r), ls(ls), allow_network(allow_network)
{
    db_repo_dir = ls.getDatabaseRootDir() / "remote" / r.name / "repository";

    static const auto db_loaded_var = "db_loaded";

    if (isNetworkAllowed())
    {
        if (!getPackagesDatabase().getIntValue(db_loaded_var))
        {
            LOG_DEBUG(logger, "Packages database was not found");
            download();
            load();
            getPackagesDatabase().setIntValue(db_loaded_var, 1);
        }
        else
            updateDb();
    }

    // at the end we always reopen packages db as read only
    getPackagesDatabase().open(true, true);
}

RemoteStorage::~RemoteStorage() = default;

/*ResolveResult RemoteStorage::resolve(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const
{
    preInitFindDependencies();
    if (Settings::get_user_settings().gForceServerQuery)
    {
        unresolved_pkgs = pkgs;
        return {};
    }
    return StorageWithPackagesDatabase::resolve(pkgs, unresolved_pkgs);
}*/

bool RemoteStorage::resolve(ResolveRequest &rr) const
{
    preInitFindDependencies();
    if (Settings::get_user_settings().gForceServerQuery)
        return false;
    return StorageWithPackagesDatabase::resolve(rr);
}

void RemoteStorage::download() const
{
    LOG_INFO(logger, "Downloading database from " + getRemote().name + " remote");

    fs::create_directories(db_repo_dir);

    if (!r.db.local_dir.empty())
    {
        for (auto &p : fs::directory_iterator(r.db.local_dir))
        {
            if (p.is_directory())
                continue;
            fs::copy_file(p, db_repo_dir / p.path().filename(), fs::copy_options::overwrite_existing);
        }
        writeDownloadTime();
        return;
    }

    auto download_archive = [this]()
    {
        auto fn = support::get_temp_filename();
        download_file(r.db.url, fn, 1_GB);
        auto unpack_dir = support::get_temp_filename();
        auto files = unpack_file(fn, unpack_dir);
        for (auto &f : files)
            fs::copy_file(f, db_repo_dir / f.filename(), fs::copy_options::overwrite_existing);
        fs::remove_all(unpack_dir);
        fs::remove(fn);
    };

    const String git = "git";
    if (!primitives::resolve_executable(git).empty() && !r.db.git_repo_url.empty())
    {
        auto git_init = [this, &git]() {
            primitives::Command::execute({git, "-C", db_repo_dir.string(), "init", "."});
            primitives::Command::execute({git, "-C", db_repo_dir.string(), "remote", "add", "github", r.db.git_repo_url});
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

static std::istream &safe_getline(std::istream &i, String &s)
{
    std::getline(i, s);
    if (!s.empty() && s.back() == '\r')
        s.resize(s.size() - 1);
    return i;
}

void RemoteStorage::load() const
{
    struct Column
    {
        String name;
        bool skip = false;
    };

    static const std::vector<std::pair<String, String>> skip_cols
    {
        {"package_version", "group_number"},
        {"package_version", "archive_version"},
        {"package_version", "hash"},
    };
    auto is_skipped_column = [](const String &tablename, const String &name)
    {
        return std::find(skip_cols.begin(), skip_cols.end(), std::pair<String, String>{ tablename,name }) != skip_cols.end();
    };

    auto mdb = getPackagesDatabase().db->native_handle();
    sqlite3_stmt *stmt = nullptr;

    // load only known tables
    // alternative: read csv filenames by mask and load all
    // but we don't do this
    Strings data_tables;
    sqlite3 *db2;
    if (sqlite3_open_v2((const char *)getPackagesDatabase().fn.u8string().c_str(), &db2, SQLITE_OPEN_READONLY, 0) != SQLITE_OK)
        throw SW_RUNTIME_ERROR("cannot open db: " + to_string(getPackagesDatabase().fn));
    int rc = sqlite3_exec(db2, "select name from sqlite_master as tables where type='table' and name not like '/_%' ESCAPE '/';",
        [](void *o, int, char **cols, char **)
        {
            Strings &data_tables = *(Strings *)o;
            data_tables.push_back(cols[0]);
            return 0;
        }, &data_tables, 0);
    sqlite3_close(db2);
    if (rc != SQLITE_OK)
        throw SW_RUNTIME_ERROR("cannot query db for tables: " + to_string(getPackagesDatabase().fn));

    getPackagesDatabase().db->execute("PRAGMA foreign_keys = OFF;");
    getPackagesDatabase().db->execute("BEGIN;");

    auto split_csv_line = [](const auto &s)
    {
        return primitives::csv::parse_line(s, ',', '\"', '\"');
    };

    for (auto &td : data_tables)
    {
        getPackagesDatabase().db->execute("delete from " + td);

        auto fn = db_repo_dir / (td + ".csv");
        std::ifstream ifile(fn);
        if (!ifile)
            throw SW_RUNTIME_ERROR("Cannot open file " + fn.string() + " for reading");

        // read first line - header
        String s;
        safe_getline(ifile, s);
        auto csvcols = split_csv_line(s);

        // read fields from header
        std::vector<Column> cols;
        for (auto &c : csvcols)
        {
            cols.push_back({ *c });
            if (is_skipped_column(td, cols.back().name))
                cols.back().skip = true;
        }
        auto n_cols = cols.size();

        // add only them
        String query = "insert into " + td + " (";
        for (auto &c : cols)
        {
            if (!c.skip)
                query += c.name + ", ";
        }
        query.resize(query.size() - 2);
        query += ") values (";
        for (size_t i = 0; i < n_cols; i++)
        {
            if (!cols[i].skip)
                query += "?, ";
        }
        query.resize(query.size() - 2);
        query += ");";

        if (sqlite3_prepare_v2(mdb, query.c_str(), (int)query.size() + 1, &stmt, 0) != SQLITE_OK)
            throw SW_RUNTIME_ERROR(sqlite3_errmsg(mdb));

        while (safe_getline(ifile, s))
        {
            int col = 1;
            for (const auto &[i,c] : enumerate(split_csv_line(s)))
            {
                if (cols[i].skip)
                    continue;
                if (c)
                    rc = sqlite3_bind_text(stmt, col, c->c_str(), -1, SQLITE_TRANSIENT);
                else
                    rc = sqlite3_bind_null(stmt, col);
                if (rc != SQLITE_OK)
                    throw SW_RUNTIME_ERROR("bad bind");
                col++;
            }

            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE)
                throw SW_RUNTIME_ERROR("sqlite3_step() failed: "s + sqlite3_errmsg(mdb));
            rc = sqlite3_reset(stmt);
            if (rc != SQLITE_OK)
                throw SW_RUNTIME_ERROR("sqlite3_reset() failed: "s + sqlite3_errmsg(mdb));
        }

        if (sqlite3_finalize(stmt) != SQLITE_OK)
            throw SW_RUNTIME_ERROR("sqlite3_finalize() failed: "s + sqlite3_errmsg(mdb));
    }

    getPackagesDatabase().db->execute("COMMIT;");
    getPackagesDatabase().db->execute("PRAGMA foreign_keys = ON;");
}

void RemoteStorage::updateDb() const
{
    if (!Settings::get_user_settings().gForceServerDatabaseUpdate)
    {
        if (!Settings::get_system_settings().can_update_packages_db || !isCurrentDbOld())
            return;
    }

    if (r.db.getVersion() > readPackagesDatabaseVersion(db_repo_dir))
    {
        // multiprocess aware
        single_process_job(getPackagesDatabase().fn.parent_path() / "db_update", [this] {
            download();
            load();
        });
    }
}

void RemoteStorage::preInitFindDependencies() const
{
    if (!isNetworkAllowed())
        return;

    // !
    updateDb();
}

void RemoteStorage::writeDownloadTime() const
{
    auto tp = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(tp);
    write_file(getPackagesDatabase().fn.parent_path() / PACKAGES_DB_DOWNLOAD_TIME_FILE, std::to_string(time));
}

TimePoint RemoteStorage::readDownloadTime() const
{
    auto fn = getPackagesDatabase().fn.parent_path() / PACKAGES_DB_DOWNLOAD_TIME_FILE;
    String ts = "0";
    if (fs::exists(fn))
        ts = read_file(fn);
    auto tp = std::chrono::system_clock::from_time_t(std::stoll(ts));
    return tp;
}

bool RemoteStorage::isCurrentDbOld() const
{
    auto tp_old = readDownloadTime();
    auto tp = std::chrono::system_clock::now();
    return (tp - tp_old) > std::chrono::minutes(PACKAGES_DB_REFRESH_TIME_MINUTES);
}

/*LocalPackage RemoteStorage::download(const PackageId &id) const
{
    ls.get(*this, id, StorageFileType::SourceArchive);
    return LocalPackage(ls, id);
}*/

/*LocalPackage RemoteStorage::install(const Package &id) const
{
    LocalPackage p(ls, id);
    if (ls.isPackageInstalled(id))
        return p;

    // actually we may want to remove only stamps, hashes etc.
    // but remove everything for now
    std::error_code ec;
    fs::remove_all(p.getDir(), ec);

    ls.get(*this, id, StorageFileType::SourceArchive);
    ls.getPackagesDatabase().installPackage(id);
    return p;
}*/

struct RemoteFileWithHashVerification : vfs::FileWithHashVerification
{
    Strings urls;
    Package p;
    mutable String hash;

    RemoteFileWithHashVerification(const Package &p) : p(p) {}

    String getHash() const override
    {
        if (hash.empty())
            throw SW_RUNTIME_ERROR("empty hash, pkg = " + p.toString());
        return hash;
    }

    bool copy(const path &fn) const override
    {
        /*auto add_downloads = [this]()
        {
            auto remote_storage = dynamic_cast<const RemoteStorage *>(&p.getStorage());
            if (!remote_storage)
                return;
            PackageId pkg = p;
            getExecutor().push([remote_storage, pkg]
            {
                remote_storage->getRemote().getApi()->addDownload(pkg);
            });
        };*/

        if (copy(fn, p.getData().getHash(StorageFileType::SourceArchive)))
        {
            return true;
        }

        if (auto remote_storage = dynamic_cast<const RemoteStorageWithFallbackToRemoteResolving *>(&p.getStorage()))
        {
            UnresolvedPackage u = p;
            UnresolvedPackages upkgs;
            auto m = remote_storage->resolveFromRemote({ u }, upkgs);
            if (upkgs.empty())
            {
                if (copy(fn, m.find(u)->second->getData().getHash(StorageFileType::SourceArchive)))
                {
                    return true;
                }
            }
        }

        return false;
    }

    bool copy(const path &fn, const String &hash) const
    {
        auto download_from_source = [&](const auto &url)
        {
            try
            {
                LOG_TRACE(logger, "Downloading file: " << url);
                download_file(url, fn);
            }
            catch (std::exception &e)
            {
                LOG_TRACE(logger, "Downloading file: " << url << ", error: " << e.what());
                return false;
            }
            return true;
        };

        for (auto &url : urls)
        {
            if (!download_from_source(url))
                continue;
            auto sfh = get_strong_file_hash(fn, hash);
            if (sfh == hash)
            {
                this->hash = sfh;
                LOG_TRACE(logger, "Downloaded file: " << url << " hash = " << sfh);
                return true;
            }
            auto fh = support::get_file_hash(fn);
            if (fh == hash)
            {
                this->hash = fh;
                LOG_TRACE(logger, "Downloaded file: " << url << " hash = " << fh);
                return true;
            }
        }

        return false;
    }
};

std::unique_ptr<vfs::File> RemoteStorage::getFile(const PackageId &id, StorageFileType t) const
{
    switch (t)
    {
    case StorageFileType::SourceArchive:
    {
        auto provs = r.dss;
        Package pkg(*this, id);
        auto rf = std::make_unique<RemoteFileWithHashVerification>(pkg);
        for (auto &p : provs)
            rf->urls.push_back(p.getUrl(pkg));
        return rf;
    }
    default:
        SW_UNREACHABLE;
    }
}

RemoteStorageWithFallbackToRemoteResolving::RemoteStorageWithFallbackToRemoteResolving(LocalStorage &ls, const Remote &r, bool allow_network)
    : RemoteStorage(ls, r, allow_network)
{
}

/*ResolveResult RemoteStorageWithFallbackToRemoteResolving::resolve(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const
{
    auto m = RemoteStorage::resolve(pkgs, unresolved_pkgs);
    if (unresolved_pkgs.empty())
        return m;
    if (!isNetworkAllowed())
        return m;
    if (remote_resolving_is_not_working)
        return m;

    // clear dirty output
    unresolved_pkgs.clear();

    LOG_DEBUG(logger, "Requesting dependency list from " + getRemote().name + " remote...");

    try
    {
        // fallback to really remote db
        return resolveFromRemote(pkgs, unresolved_pkgs);
    }
    catch (std::exception &e)
    {
        // we ignore remote storage errors, print them,
        // mark all deps as unresolved and
        // return empty result
        // we also mark remove resolving as not working, so we won't be trying this again
        remote_resolving_is_not_working = true;
        LOG_WARN(logger, "Remote: " << getName() << ": " << e.what());
        unresolved_pkgs = pkgs;
        return {};
    }
}*/

bool RemoteStorageWithFallbackToRemoteResolving::resolve(ResolveRequest &rr) const
{
    return RemoteStorage::resolve(rr);
    // remote resolving is disabled for now
}

ResolveResult RemoteStorageWithFallbackToRemoteResolving::resolveFromRemote(const UnresolvedPackages &pkgs, UnresolvedPackages &unresolved_pkgs) const
{
    return getRemote().getApi()->resolvePackages(pkgs, unresolved_pkgs, data, *this);
}

PackageDataPtr RemoteStorageWithFallbackToRemoteResolving::loadData(const PackageId &pkg) const
{
    auto i = data.find(pkg);
    if (i == data.end())
        return RemoteStorage::loadData(pkg);
    return i->second.clone();
}

}
