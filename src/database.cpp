/*
 * Copyright (c) 2016, Egor Pugin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *     3. Neither the name of the copyright holder nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "database.h"

#include "command.h"
#include "config.h"
#include "directories.h"
#include "enums.h"
#include "lock.h"
#include "sqlite_database.h"

#include <boost/algorithm/string.hpp>
#include <sqlite3.h>

#include <logger.h>
DECLARE_STATIC_LOGGER(logger, "db");

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

const TableDescriptors service_tables{
    {
        "NRuns",
        R"(
            CREATE TABLE "NRuns" (
            "n_runs" INTEGER NOT NULL
            );
            insert into NRuns values (0);
        )"
    },
};

const TableDescriptors data_tables{
    {
        "Projects",
        R"(
            CREATE TABLE "Projects" (
            "id" INTEGER NOT NULL,
            "path" TEXT(2048) NOT NULL,
            "type_id" INTEGER,
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
            "project_id" INTEGER,
            "major" INTEGER,
            "minor" INTEGER,
            "patch" INTEGER,
            "branch" TEXT,
            "flags" INTEGER NOT NULL,
            "created" DATE NOT NULL,
            "sha256" TEXT NOT NULL,
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
    // try to keep databases only to user storage dir, not local one
    Directories dirs;
    dirs.set_storage_dir(Config::get_user_config().local_settings.storage_dir);
    return dirs.storage_dir_etc / db_dir_name;
}

int readPackagesDbSchemaVersion(const path &dir)
{
    return std::stoi(read_file(dir / PACKAGES_DB_SCHEMA_VERSION_FILE));
}

void writePackagesDbSchemaVersion(const path &dir)
{
    write_file(dir / PACKAGES_DB_SCHEMA_VERSION_FILE, std::to_string(PACKAGES_DB_SCHEMA_VERSION));
}

int readPackagesDbVersion(const path &dir)
{
    return std::stoi(read_file(dir / PACKAGES_DB_VERSION_FILE));
}

void writePackagesDbVersion(const path &dir, int version)
{
    write_file(dir / PACKAGES_DB_VERSION_FILE, std::to_string(version));
}

ServiceDatabase &getServiceDatabase()
{
    static ServiceDatabase db;
    return db;
}

PackagesDatabase &getPackagesDatabase()
{
    static PackagesDatabase db;
    return db;
}

Database::Database(const String &name, const TableDescriptors &tds)
{
    db_dir = getDbDirectory();
    fn = db_dir / name;
    if (!fs::exists(fn))
    {
        ScopedFileLock lock(fn);
        if (!fs::exists(fn))
        {
            db = std::make_unique<SqliteDatabase>(fn.string());
            for (auto &td : tds)
                db->execute(td.create);
            created = true;
        }
    }
    if (!db)
        db = std::make_unique<SqliteDatabase>(fn.string());
}

ServiceDatabase::ServiceDatabase()
    : Database(service_db_name, service_tables)
{
}

int ServiceDatabase::getNumberOfRuns() const
{
    int n_runs = 0;
    db->execute("select n_runs from NRuns;", [&n_runs](int, char**cols, char**)
    {
        n_runs = std::stoi(cols[0]);
        return 0;
    });
    return n_runs;
}

int ServiceDatabase::increaseNumberOfRuns()
{
    auto prev = getNumberOfRuns();
    db->execute("update NRuns set n_runs = n_runs + 1;");
    return prev;
}

PackagesDatabase::PackagesDatabase()
    : Database(packages_db_name, data_tables)
{
    db_repo_dir = db_dir / db_repo_dir_name;

    if (created)
    {
        LOG_INFO(logger, "Packages database was not found.");
        download();
        load();
        return;
    }

    if (isCurrentDbOld())
    {
        auto version_remote = std::stoi(download_file(db_version_url));
        if (version_remote > readPackagesDbVersion(db_repo_dir))
        {
            download();
            load(true);
        }
    }
}

void PackagesDatabase::download()
{
    LOG_INFO(logger, "Downloading database...");

    fs::create_directories(db_repo_dir);

    String git = "git";
    if (has_executable_in_path(git))
    {
        if (!fs::exists(db_repo_dir / ".git"))
        {
            command::execute({ git,"-C",db_repo_dir.string(),"init","." });
            command::execute({ git,"-C",db_repo_dir.string(),"remote","add","github",db_repo_url });
            command::execute({ git,"-C",db_repo_dir.string(),"fetch","--depth","1","github","master" });
            command::execute({ git,"-C",db_repo_dir.string(),"reset","--hard","FETCH_HEAD" });
        }
        else
        {
            command::execute({ git,"-C",db_repo_dir.string(),"pull","github","master" });
        }
    }
    else
    {
        DownloadData dd;
        dd.url = db_master_url;
        dd.file_size_limit = 1'000'000'000;
        dd.fn = get_temp_filename();
        download_file(dd);
        auto unpack_dir = get_temp_filename();
        auto files = unpack_file(dd.fn, unpack_dir);
        for (auto &f : files)
            fs::copy_file(f, db_repo_dir / f.filename(), fs::copy_option::overwrite_if_exists);
        fs::remove_all(unpack_dir);
        fs::remove(dd.fn);
    }

    writeDownloadTime();
}

void PackagesDatabase::load(bool drop)
{
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
        std::ifstream ifile(fn.string());
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

PackagesDatabase::TimePoint PackagesDatabase::readDownloadTime() const
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
    return (tp - tp_old) > std::chrono::minutes(15);
}

DownloadDependencies PackagesDatabase::findDependencies(const Packages &deps) const
{
    DependenciesMap all_deps;
    for (auto &dep : deps)
    {
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

        auto find_deps = [&dep, &all_deps, this](auto &dependency)
        {
            dependency.flags.set(pfDirectDependency);
            dependency.id = getExactProjectVersionId(dependency, dependency.version, dependency.flags, dependency.sha256);
            all_deps[dependency] = dependency; // assign first, deps assign second
            all_deps[dependency].dependencies = getProjectDependencies(dependency.id, all_deps);
        };

        if (type == ProjectType::RootProject)
        {
            std::vector<DownloadDependency> projects;

            // root projects should return all children (lib, exe)
            db->execute("select id, path, flags from Projects where path like '" + project.ppath.toString() +
                ".%' and type_id in (1,2) order by path",
                [&projects](SQLITE_CALLBACK_ARGS)
            {
                DownloadDependency project;
                project.id = std::stoull(cols[0]);
                project.ppath = String(cols[1]);
                project.flags = std::stoull(cols[3]);
                projects.push_back(project);
                return 0;
            });

            if (projects.empty())
                // TODO: replace later with typed exception, so client will try to fetch same package from server
                throw std::runtime_error("Root project '" + project.ppath.toString() + "' is empty");

            for (auto &p : projects)
                find_deps(p);
        }
        else
        {
            find_deps(project);
        }
    }

    // make id deps
    DownloadDependencies dds;
    for (auto &ad : all_deps)
    {
        auto &d = ad.second;
        std::set<ProjectVersionId> ids;
        for (auto &dd2 : d.dependencies)
            ids.insert(dd2.second.id);
        d.setDependencyIds(ids);
        dds[d.id] = d;
    }
    return dds;
}

ProjectVersionId PackagesDatabase::getExactProjectVersionId(const DownloadDependency &project, Version &version, ProjectFlags &flags, String &sha256) const
{
    auto err = [](const auto &v, const auto &p)
    {
        return std::runtime_error("No such version/branch '" + v.toAnyVersion() + "' for project '" + p.toString() + "'");
    };

    ProjectVersionId id = 0;

    if (!version.isBranch())
    {
        auto &v = version;

        db->execute(
            "select id, major, minor, patch, flags, sha256 from ProjectVersions where "
            "project_id = " + std::to_string(project.id) + " and "
            "major = " + std::to_string(v.major) + " and "
            "minor = " + std::to_string(v.minor) + " and "
            "patch = " + std::to_string(v.patch), [&id, &flags, &sha256](SQLITE_CALLBACK_ARGS)
        {
            id = std::stoull(cols[0]);
            flags |= decltype(project.flags)(std::stoull(cols[4]));
            sha256 = cols[5];
            return 0;
        });

        if (id == 0)
        {
            if (v.patch != -1)
                throw err(version, project.ppath);

            db->execute(
                "select id, major, minor, patch, flags, sha256 from ProjectVersions where "
                "project_id = " + std::to_string(project.id) + " and "
                "major = " + std::to_string(v.major) + " and "
                "minor = " + std::to_string(v.minor) + " and "
                "branch is null order by major desc, minor desc, patch desc limit 1",
                [&id, &version, &flags, &sha256](SQLITE_CALLBACK_ARGS)
            {
                id = std::stoull(cols[0]);
                version.patch = std::stoi(cols[3]);
                flags |= decltype(project.flags)(std::stoull(cols[4]));
                sha256 = cols[5];
                return 0;
            });

            if (id == 0)
            {
                if (v.minor != -1)
                    throw err(version, project.ppath);

                db->execute(
                    "select id, major, minor, patch, flags, sha256 from ProjectVersions where "
                    "project_id = " + std::to_string(project.id) + " and "
                    "major = " + std::to_string(v.major) + " and "
                    "branch is null order by major desc, minor desc, patch desc limit 1",
                    [&id, &version, &flags, &sha256](SQLITE_CALLBACK_ARGS)
                {
                    id = std::stoull(cols[0]);
                    version.minor = std::stoi(cols[2]);
                    version.patch = std::stoi(cols[3]);
                    flags |= decltype(project.flags)(std::stoull(cols[4]));
                    sha256 = cols[5];
                    return 0;
                });

                if (id == 0)
                {
                    if (v.major != -1)
                        throw err(version, project.ppath);

                    db->execute(
                        "select id, major, minor, patch, flags, sha256 from ProjectVersions where "
                        "project_id = " + std::to_string(project.id) + " and "
                        "branch is null order by major desc, minor desc, patch desc limit 1",
                        [&id, &version, &flags, &sha256](SQLITE_CALLBACK_ARGS)
                    {
                        id = std::stoull(cols[0]);
                        version.major = std::stoi(cols[1]);
                        version.minor = std::stoi(cols[2]);
                        version.patch = std::stoi(cols[3]);
                        flags |= decltype(project.flags)(std::stoull(cols[4]));
                        sha256 = cols[5];
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
            "select id, major, minor, patch, flags, sha256 from ProjectVersions where "
            "project_id = " + std::to_string(project.id) + " and "
            "branch = " + version.toString(), [&id, &flags, &sha256](SQLITE_CALLBACK_ARGS)
        {
            id = std::stoull(cols[0]);
            flags |= decltype(project.flags)(std::stoull(cols[4]));
            sha256 = cols[5];
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
        "where project_version_id = " + std::to_string(project_version_id) + " order by path",
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
        dependency.id = getExactProjectVersionId(dependency, dependency.version, dependency.flags, dependency.sha256);
        auto i = dm.find(dependency);
        if (i == dm.end())
        {
            dm[dependency] = dependency; // assign first, deps assign second
            dm[dependency].dependencies = getProjectDependencies(dependency.id, dm);
        }
        dependencies[dependency.ppath.toString()] = dependency;
    }
    return dependencies;
}
