// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "exceptions.h"
#include "filesystem.h"

#include <functional>
#include <memory>

#define SQLITE_CALLBACK_ARGS int ncols, char** cols, char** names

TYPED_EXCEPTION_WITH_STD_PARENT(sqlite3_exception, logic_error);

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
