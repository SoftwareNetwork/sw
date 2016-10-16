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

#include "sqlite_database.h"

#include <algorithm>

#include <sqlite3.h>

#include <logger.h>
DECLARE_STATIC_LOGGER(logger, "sqlite_db");

#define MAX_ERROR_SQL_LENGTH 200

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
int loadOrSaveDb(sqlite3 *pInMemory, const char *zFilename, int isSave)
{
    int rc;                  /* Function return code */
    sqlite3 *pFile;          /* SqliteDatabase connection opened on zFilename */
    sqlite3_backup *pBackup; /* Backup object used to copy data */
    sqlite3 *pTo;            /* SqliteDatabase to copy to (pFile or pInMemory) */
    sqlite3 *pFrom;          /* SqliteDatabase to copy from (pFile or pInMemory) */

    /* Open the database file identified by zFilename. Exit early if this fails
                              ** for any reason. */
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

sqlite3 *load_from_file(const String &fn)
{
    sqlite3 *db = nullptr;
    if (sqlite3_open(fn.c_str(), &db))
    {
        String error = "Can't open database file: " + fn + " error: " + sqlite3_errmsg(db);
        LOG_ERROR(logger, error);
        throw std::runtime_error(error);
    }
    return db;
}

sqlite3 *open_in_memory()
{
    sqlite3 *db = nullptr;
    if (sqlite3_open(":memory:", &db))
    {
        String error = "Can't open in memory database, error: ";
        error += sqlite3_errmsg(db);
        LOG_ERROR(logger, error);
        throw std::runtime_error(error);
    }
    return db;
}

sqlite3 *load_from_file_to_memory(const String &fn)
{
    sqlite3 *db = open_in_memory();
    auto ret = loadOrSaveDb(db, fn.c_str(), 0);
    if (ret != SQLITE_OK)
    {
        String error = "Can't load database: " + fn + " error: " + sqlite3_errstr(ret);
        LOG_ERROR(logger, error);
        throw std::runtime_error(error);
    }
    return db;
}

void save_from_memory_to_file(const String &fn, sqlite3 *db)
{
    auto ret = loadOrSaveDb(db, fn.c_str(), 1);
    if (ret != SQLITE_OK)
    {
        String error = "Can't save database: " + fn + " error: " + sqlite3_errstr(ret);
        LOG_ERROR(logger, error);
        throw std::runtime_error(error);
    }
}

SqliteDatabase::SqliteDatabase()
{
    db = open_in_memory();
}

SqliteDatabase::SqliteDatabase(sqlite3 *db)
    : db(db)
{
}

SqliteDatabase::SqliteDatabase(const String &dbname)
{
    LOG_TRACE(logger, "Initializing database: " << dbname);

    loadDatabase(dbname);

    name = dbname.substr(std::max((int)dbname.rfind("/"), (int)dbname.rfind("\\")) + 1);
    fullName = dbname;
}

SqliteDatabase::~SqliteDatabase()
{
    sqlite3_close(db);
    db = nullptr;
}

void SqliteDatabase::loadDatabase(const String &dbname)
{
    if (isLoaded())
        return;

    LOG_TRACE(logger, "Opening database: " << dbname);

    db = load_from_file(dbname.c_str());

    execute("PRAGMA cache_size = -2000;"); // cache size (N * page size)
    execute("PRAGMA page_size = 4096;"); // page size bytes (N * page size)
    execute("PRAGMA journal_mode = OFF;"); // set to no journal
    //execute("PRAGMA synchronous = 1;"); // set to wait for OS sync (0 - no wait, 1 - wait OS, 2 - wait all)
    execute("PRAGMA foreign_keys = ON;");
}

void SqliteDatabase::save(const path &fn) const
{
    if (!isLoaded())
        return;
    save_from_memory_to_file(fn.string(), db);
}

bool SqliteDatabase::isLoaded() const
{
    return db != nullptr;
}

bool SqliteDatabase::execute(const String &sql, void *object, Sqlite3Callback callback, bool nothrow, String *err) const
{
    if (!isLoaded())
        throw std::runtime_error("db is not loaded");

    LOG_TRACE(logger, "Executing sql statement: " << sql);
    char *errmsg;
    String error;
    sqlite3_exec(db, sql.c_str(), callback, object, &errmsg);
    if (errmsg)
    {
        auto s = sql.substr(0, MAX_ERROR_SQL_LENGTH);
        if (sql.size() > MAX_ERROR_SQL_LENGTH)
            s += "...";
        error = "Error executing sql statement:\n" + s + "\nError: " + errmsg;
        sqlite3_free(errmsg);
        LOG_ERROR(logger, error);
        if (nothrow)
        {
            if (errmsg)
                *err = error;
        }
        else
            throw std::runtime_error(error);
    }
    return error.empty();
}

bool SqliteDatabase::execute(const String &sql, DatabaseCallback callback, bool nothrow, String *err) const
{
    if (!isLoaded())
        throw std::runtime_error("db is not loaded");

    LOG_TRACE(logger, "Executing sql statement: " << sql);
    char *errmsg;
    String error;
    auto cb = [](void *o, int ncols, char **cols, char **names)
    {
        auto f = (DatabaseCallback *)o;
        if (*f)
            return (*f)(ncols, cols, names);
        return 0;
    };
    int rc = sqlite3_exec(db, sql.c_str(), cb, &callback, &errmsg);
    if (errmsg)
    {
        auto s = sql.substr(0, MAX_ERROR_SQL_LENGTH);
        if (sql.size() > MAX_ERROR_SQL_LENGTH)
            s += "...";
        error = "Error executing sql statement:\n" + s + "\nError: " + errmsg;
        sqlite3_free(errmsg);
        LOG_ERROR(logger, error);
        if (nothrow)
        {
            if (errmsg)
                *err = error;
        }
        else
            throw std::runtime_error(error);
    }
    else if (rc != SQLITE_OK)
    {
        auto s = sql.substr(0, MAX_ERROR_SQL_LENGTH);
        if (sql.size() > MAX_ERROR_SQL_LENGTH)
            s += "...";
        error = "Error executing sql statement:\n" + s;
        LOG_ERROR(logger, error);
        if (nothrow)
        {
            if (errmsg)
                *err = error;
        }
        else
            throw std::runtime_error(error);
    }
    return error.empty();
}

String SqliteDatabase::getName() const
{
    return name;
}

String SqliteDatabase::getFullName() const
{
    return fullName;
}

sqlite3 *SqliteDatabase::getDb() const
{
    return db;
}

int SqliteDatabase::getNumberOfColumns(const String &table) const
{
    int n = 0;
    execute("pragma table_info(" + table + ");", [&n](SQLITE_CALLBACK_ARGS)
    {
        n++;
        return 0;
    });
    return n;
}

int SqliteDatabase::getNumberOfTables() const
{
    int n = 0;
    execute("select count(*) from sqlite_master as tables where type='table';", [&n](SQLITE_CALLBACK_ARGS)
    {
        n = std::stoi(cols[0]);
        return 0;
    });
    return n;
}
