/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

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
