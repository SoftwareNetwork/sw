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

#pragma once

#include "cppan_string.h"
#include "filesystem.h"

#include <functional>
#include <memory>

#define SQLITE_CALLBACK_ARGS int ncols, char** cols, char** names

struct sqlite3;

class SqliteDatabase
{
    typedef int(*Sqlite3Callback)(void*, int /*ncols*/, char** /*cols*/, char** /*names*/);
    typedef std::function<int(int /*ncols*/, char** /*cols*/, char** /*names*/)> DatabaseCallback;

public:
    SqliteDatabase();
    SqliteDatabase(sqlite3 *db);
    SqliteDatabase(const path &dbname, bool read_only = false);
    ~SqliteDatabase();

    void loadDatabase(const path &dbname);
    void save(const path &fn) const;
    void close();

    bool isLoaded() const;
    sqlite3 *getDb() const;

    path getFullName() const;

    bool execute(String sql, void *object, Sqlite3Callback callback, bool nothrow = false, String *errmsg = nullptr) const;
    bool execute(String sql, DatabaseCallback callback = DatabaseCallback(), bool nothrow = false, String *errmsg = nullptr) const;

    int getNumberOfColumns(const String &table) const;
    int getNumberOfTables() const;
    int64_t getLastRowId() const;

    void dropTable(const String &table) const;

private:
    sqlite3 *db = nullptr;
    bool read_only = false;
    path fullName;
};
