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
#include "common.h"
#include "directories.h"
#include "sqlite_database.h"

#include <boost/algorithm/string.hpp>
#include <sqlite3.h>

#include <logger.h>
DECLARE_STATIC_LOGGER(logger, "db");

const String db_repo_url = "https://github.com/cppan/database";
const String db_master_url = db_repo_url + "/archive/master.zip";
const path db_dir = directories.storage_dir_etc / "database";
const path db_repo_dir = db_dir / "repository";
const String db_name = "db.sqlite";
const path db_file = db_dir / db_name;

struct TableDescriptor
{
    String name;
    String create;
    int n_fields;
};

const std::vector<TableDescriptor> table_descriptors{
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
        )", 4
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
        )", 9
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
        )", 4
    },
};

void download_db()
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
}

void create_tables(SqliteDatabase *db)
{
    for (auto &td : table_descriptors)
        db->execute(td.create);
}

void load_data(SqliteDatabase *db, bool drop = false)
{
    auto mdb = db->getDb();
    sqlite3_stmt *stmt = nullptr;

    db->execute("BEGIN;");

    for (auto &td : table_descriptors)
    {
        if (drop)
            db->execute("delete from " + td.name);

        String query = "insert into " + td.name + " values (";
        for (int i = 0; i < td.n_fields; i++)
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

            for (int i = 1; i <= td.n_fields; i++)
            {
                sqlite3_bind_text(stmt, i, b, -1, SQLITE_TRANSIENT);
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
}

std::unique_ptr<SqliteDatabase> open_db()
{
    std::unique_ptr<SqliteDatabase> db;

    if (!fs::exists(db_file))
    {
        LOG_INFO(logger, "Packages database was not found.");

        download_db();

        db = std::make_unique<SqliteDatabase>(db_file.string());
        create_tables(db.get());
        load_data(db.get());
    }
    else
    {
        db = std::make_unique<SqliteDatabase>(db_file.string());

        // if current time - lwt > 15
        {
            // download remote lwt

            // if remote lwt > current lwt
            {
                download_db();
                load_data(db.get(), true);
            }
        }
    }

    return db;
}
