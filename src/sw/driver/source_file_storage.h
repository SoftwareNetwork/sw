// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "compiler.h"
#include "types.h"

#include <sw/builder/node.h>

#include <memory>

namespace sw
{

struct SourceFile;
struct Target;

template <class T>
using SourceFileMap = std::unordered_map<path, std::shared_ptr<T>>;

/**
 * \brief Keeps target files.
 *
 *  There are 3 cases for source file:
 *   1. no file at all
 *   2. file present but empty (unknown ext)
 *   3. file present and has known ext
 *
 *  There are 4 cases for source files:
 *   1. no files at all         = autodetection
 *   2. all files are skipped   = autodetection
 *   3. mix of skipped and normal files
 *   4. all files are not skipped
 *
 */
 //template <class T>
struct SW_DRIVER_CPP_API SourceFileStorage : protected SourceFileMap<SourceFile>
{
public:
    using SourceFileMapThis = SourceFileMap<SourceFile>;
    using SourceFileMapThis::begin;
    using SourceFileMapThis::end;
    using SourceFileMapThis::empty;
    using SourceFileMapThis::size;

public:
    Target *target = nullptr;

    SourceFileStorage();
    virtual ~SourceFileStorage();

    //void add(const String &file) { add(path(file)); }
    void add(const path &file);
    void add(const Files &files);
    void add(const FileRegex &r);
    void add(const path &root, const FileRegex &r);

    //void remove(const String &file) { remove(path(file)); }
    void remove(const path &file);
    void remove(const Files &files);
    void remove(const FileRegex &r);
    void remove(const path &root, const FileRegex &r);

    //void remove_exclude(const String &file) { remove(path(file)); }
    void remove_exclude(const path &file);
    void remove_exclude(const Files &files);
    void remove_exclude(const FileRegex &r);
    void remove_exclude(const path &root, const FileRegex &r);

    size_t sizeKnown() const;
    size_t sizeSkipped() const;

    void resolve();
    //void resolveRemoved();

    SourceFile &operator[](path F);
    SourceFileMap<SourceFile> operator[](const FileRegex &r) const;

    // for option groups
    void merge(const SourceFileStorage &v, const GroupSettings &s = GroupSettings());

    bool check_absolute(path &file, bool ignore_errors = false, bool *source_dir = nullptr) const;

    // internal
    mutable std::unordered_map<path, std::map<bool /* recursive */, Files>> glob_cache;
    mutable FilesMap files_cache;

protected:
    bool autodetect = false;

    void clearGlobCache();
    void remove_full(const path &file);

private:
    struct FileOperation
    {
        std::variant<path, FileRegex> op;
        bool add = true;
    };
    using Op = void (SourceFileStorage::*)(const path &);

    std::vector<FileOperation> file_ops;

    void add_unchecked(const path &f, bool skip = false);
    void add1(const FileRegex &r);
    void remove1(const FileRegex &r);
    void remove_full1(const FileRegex &r);
    void op(const FileRegex &r, Op f);

    SourceFileMap<SourceFile> enumerate_files(const FileRegex &r) const;
};

}
