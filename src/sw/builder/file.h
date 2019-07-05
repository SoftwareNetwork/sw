// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "node.h"

//#include <sw/manager/enums.h>

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

struct SW_BUILDER_API File
{
    FileStorage *fs = nullptr;
    path file;

    File() = default;
    File(const path &p, FileStorage &s);
    virtual ~File() = default;

    File &operator=(const path &rhs);

    path getPath() const;
    /*void addImplicitDependency(const path &f); // not thread safe
    void addImplicitDependency(const Files &f); // not thread safe
    void clearImplicitDependencies();
    std::unordered_set<std::shared_ptr<builder::Command>> gatherDependentGenerators() const;*/

    FileRecord &getFileRecord();
    const FileRecord &getFileRecord() const;

    bool empty() const { return file.empty(); }
    bool isChanged() const;
    std::optional<String> isChanged(const fs::file_time_type &t, bool throw_on_missing);
    bool isGenerated() const;
    bool isGeneratedAtAll() const;

private:
    mutable FileRecord *r = nullptr;

    void registerSelf() const;

    friend struct FileDataStorage;
    friend struct FileStorage;
};

struct FileData
{
    enum class RefreshType : uint8_t
    {
        Unrefreshed,
        InProcess,
        NotChanged,
        Changed,
    };

    fs::file_time_type last_write_time;
    //int64_t size = -1;
    //String hash;
    //SomeFlags flags;
    std::weak_ptr<builder::Command> generator;
    bool generated = false;

    // downloaded etc.
    // we cut DAG below commands with all such outputs
    //bool provided = false;

    // if file info is updated during this run
    std::atomic<RefreshType> refreshed{ RefreshType::Unrefreshed };

    FileData() = default;
    FileData(const FileData &);
    FileData &operator=(const FileData &rhs);
};

// config specific
struct SW_BUILDER_API FileRecord
{
    FileStorage *fs = nullptr;
    path file;
    FileData *data = nullptr;

    FileRecord() = default;
    FileRecord(const FileRecord &);
    FileRecord &operator=(const FileRecord &);

    void reset();

    // only lwt change since the last run
    bool isChanged();

    // check if file or any deps changed
    bool isChangedWithDeps();

    // check using lwt
    std::optional<String> isChanged(const fs::file_time_type &t, bool throw_on_missing);

    bool isGenerated() const;
    bool isGeneratedAtAll() const { return data->generated; }
    void setGenerator(const std::shared_ptr<builder::Command> &, bool ignore_errors);
    void setGenerated(bool g = true) { data->generated = g; }
    std::shared_ptr<builder::Command> getGenerator() const;

    bool operator<(const FileRecord &r) const;

    /// get last write time of this file and all deps
    fs::file_time_type getMaxTime() const;

    /// returns true if file was changed
    /// also loades information
    void refresh();

    //void writeToLog() const;

    //fs::file_time_type updateLwt();
};

#define EXPLAIN_OUTDATED(subject, outdated, reason, name) \
    explainMessage(subject, outdated, reason, name)

void explainMessage(const String &subject, bool outdated, const String &reason, const String &name);

}
