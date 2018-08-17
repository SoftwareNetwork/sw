// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "node.h"
#include <enums.h>

#include <primitives/filesystem.h>

#include <atomic>
#include <mutex>
#include <optional>

namespace sw
{

namespace builder
{

struct Command;

}

struct FileRecord;
struct FileStorage;

struct SW_BUILDER_API File : Data
{
    path file;

    File() = default;
    File(const path &p);
    virtual ~File() = default;

    File &operator=(const path &rhs);

    path getPath() const;
    void addExplicitDependency(const path &f);
    void addExplicitDependency(const Files &f);
    void addImplicitDependency(const path &f);
    void addImplicitDependency(const Files &f);
    void clearDependencies();
    void clearImplicitDependencies();
    std::unordered_set<std::shared_ptr<builder::Command>> gatherDependentGenerators() const;

    FileRecord &getFileRecord();
    const FileRecord &getFileRecord() const;

    bool empty() const { return file.empty(); }
    bool isChanged() const;
    bool isGenerated() const;

private:
    mutable FileRecord *r = nullptr;

    void registerSelf() const;

    friend struct FileStorage;
};

enum FileFlags
{
    //ffNotExists     = 0,
};

struct SW_BUILDER_API FileRecord
{
    path file;
    fs::file_time_type last_write_time;
    int64_t size = -1;
    String hash;
    SomeFlags flags;

    // make sets?
    std::unordered_map<path, FileRecord *> explicit_dependencies;
    std::unordered_map<path, FileRecord *> implicit_dependencies;

    FileRecord() = default;
    FileRecord(const FileRecord &);
    FileRecord &operator=(const FileRecord &);

    bool isChanged();
    void load(const path &p = path());
    void destroy() { delete this; }
    void reset();
    size_t getHash() const;
    bool isGenerated() const;
    void setGenerator(const std::shared_ptr<builder::Command> &);
    std::shared_ptr<builder::Command> getGenerator() const;

    bool operator<(const FileRecord &r) const;

    //private:
        // if file info is updated during this run
    std::atomic_bool refreshed{ false };

    /// get last write time of this file and all deps
    fs::file_time_type getMaxTime() const;

    /// returns true if file was changed
    bool refresh();

private:
    std::weak_ptr<builder::Command> generator;
};

SW_BUILDER_API
FileStorage &getFileStorage();

path getFilesLogFileName();

#define EXPLAIN_OUTDATED(subject, outdated, reason, name) \
    explainMessage(subject, outdated, reason, name)

void explainMessage(const String &subject, bool outdated, const String &reason, const String &name);

}
