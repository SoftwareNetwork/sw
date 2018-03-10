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

#include "sqlite_database.h"

#include "lock.h"

#include <boost/algorithm/string.hpp>
#include <sqlite3.h>

#include <algorithm>

#include <primitives/log.h>
//DECLARE_STATIC_LOGGER(logger, "sqlite_db");

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

sqlite3 *load_from_file(const path &fn, bool read_only)
{
    sqlite3 *db = nullptr;
    bool ok = true;
    int flags = 0;
    if (sqlite3_threadsafe())
        flags |= SQLITE_OPEN_NOMUTEX;// SQLITE_OPEN_FULLMUTEX;
    if (read_only)
        flags |= SQLITE_OPEN_READONLY;
    else
        flags |= SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    sqlite3_enable_shared_cache(1);
    ok = sqlite3_open_v2(fn.string().c_str(), &db, flags, nullptr) == SQLITE_OK;
    if (!ok)
    {
        String error = "Can't open database file: " + fn.string() + " error: " + sqlite3_errmsg(db);
        sqlite3_close(db);
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
        sqlite3_close(db);
        throw std::runtime_error(error);
    }
    return db;
}

sqlite3 *load_from_file_to_memory(const path &fn)
{
    sqlite3 *db = open_in_memory();
    auto ret = loadOrSaveDb(db, fn.string().c_str(), 0);
    if (ret != SQLITE_OK)
    {
        String error = "Can't load database: " + fn.string() + " error: " + sqlite3_errstr(ret);
        sqlite3_close(db);
        throw std::runtime_error(error);
    }
    return db;
}

void save_from_memory_to_file(const path &fn, sqlite3 *db)
{
    auto ret = loadOrSaveDb(db, fn.string().c_str(), 1);
    if (ret != SQLITE_OK)
    {
        String error = "Can't save database: " + fn.string() + " error: " + sqlite3_errstr(ret);
        sqlite3_close(db);
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

SqliteDatabase::SqliteDatabase(const path &dbname, bool ro)
    : read_only(ro)
{
    LOG_TRACE(logger, "Initializing database: " << dbname << (read_only ? ", in-memory mode" : ""));

    // TODO: remove when memory db is turned on
    read_only = false;

    loadDatabase(dbname);
}

SqliteDatabase::~SqliteDatabase()
{
    close();
}

void SqliteDatabase::close()
{
    if (!isLoaded())
        return;

    // turn on only for memory db
    //save(fullName);

    sqlite3_close(db);
    db = nullptr;
}

void SqliteDatabase::loadDatabase(const path &dbname)
{
    if (isLoaded())
        return;

    close();

    LOG_TRACE(logger, "Opening database: " << dbname);

    if (read_only)
        db = load_from_file_to_memory(dbname);
    else
        db = load_from_file(dbname, read_only);

    fullName = dbname;
}

void SqliteDatabase::save(const path &fn) const
{
    if (!isLoaded())
        return;
    save_from_memory_to_file(fn, db);
}

bool SqliteDatabase::isLoaded() const
{
    return db != nullptr;
}

bool SqliteDatabase::execute(String sql, void *object, Sqlite3Callback callback, bool nothrow, String *err) const
{
    if (!isLoaded())
        throw std::runtime_error("db is not loaded");

    boost::trim(sql);

    // TODO: remove later when sqlite won't be crashing
    static std::mutex m;
    std::unique_lock<std::mutex> lk(m);

    // lock always for now
    ScopedFileLock lock(get_lock(fullName), std::defer_lock);
    if (!read_only)
        lock.lock();

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
        if (nothrow)
        {
            if (errmsg && err)
                *err = error;
        }
        else
            throw std::runtime_error(error);
    }
    return error.empty();
}

bool SqliteDatabase::execute(String sql, DatabaseCallback callback, bool nothrow, String *err) const
{
    if (!isLoaded())
        throw std::runtime_error("db is not loaded");

    boost::trim(sql);

    // TODO: remove later when sqlite won't be crashing
    static std::mutex m;
    std::unique_lock<std::mutex> lk(m);

    // lock always for now
    ScopedFileLock lock(get_lock(fullName), std::defer_lock);
    if (!read_only)
        lock.lock();

    //
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
        if (nothrow)
        {
            if (errmsg && err)
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
        if (nothrow)
        {
            if (errmsg && err)
                *err = error;
        }
        else
            throw std::runtime_error(error);
    }
    return error.empty();
}

path SqliteDatabase::getFullName() const
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

void SqliteDatabase::dropTable(const String &table) const
{
    execute("drop table " + table + ";");
}

int64_t SqliteDatabase::getLastRowId() const
{
    return sqlite3_last_insert_rowid(db);
}
