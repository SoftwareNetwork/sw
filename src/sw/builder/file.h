// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin

#pragma once

#include "node.h"

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

struct FileStorage;

struct FileData
{
    enum class RefreshType : uint8_t
    {
        Unrefreshed,
        InProcess,
        NotChanged,
        Changed,
    };

    fs::file_time_type last_write_time = fs::file_time_type::min();
    //int64_t size = -1;
    //String hash;
    //SomeFlags flags;
    bool generated = false;

    // downloaded etc.
    // we cut DAG below commands with all such outputs
    //bool provided = false;

    // if file info is updated during this run
    std::atomic<RefreshType> refreshed{ RefreshType::Unrefreshed };
    //mutable std::mutex m;

    FileData() = default;
    FileData(const FileData &);
    FileData &operator=(const FileData &rhs);

    void reset();
    void refresh(const path &file);
};

struct SW_BUILDER_API File : virtual ICastable
{
    path file;

    File() = default;
    File(const path &p, FileStorage &s);
    virtual ~File() = default;

    path getPath() const;

    FileData &getFileData();
    const FileData &getFileData() const;

    bool empty() const { return file.empty(); }

    bool isChanged() const;
    std::optional<String> isChanged(const fs::file_time_type &t, bool throw_on_missing);

    bool isGenerated() const;
    void setGenerated(bool g = true);

private:
    mutable FileData *data = nullptr;
};

#define EXPLAIN_OUTDATED(subject, outdated, reason, name) \
    explainMessage(subject, outdated, reason, name)

void explainMessage(const String &subject, bool outdated, const String &reason, const String &name);

}
