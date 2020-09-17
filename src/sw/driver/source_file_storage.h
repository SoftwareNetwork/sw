// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "compiler/compiler.h"
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
struct SW_DRIVER_CPP_API SourceFileStorage
{
private:
    Target &target;

public:
    // internal, move to target map?
    // but we have two parts: stable for sdir files and unknown for bdir files (config specific)
    mutable std::unordered_map<path, std::map<bool /* recursive */, Files>> glob_cache;
    mutable FilesMap files_cache;

public:
    SourceFileStorage(Target &);
    virtual ~SourceFileStorage();

    //void add(const String &file) { add(path(file)); }
    void add(const std::shared_ptr<SourceFile> &);
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

    SourceFile &operator[](path F);
    SourceFileMap<SourceFile> operator[](const FileRegex &r) const;

    // for option groups
    void merge(const SourceFileStorage &v, const GroupSettings &s = GroupSettings());
    void mergeFiles(const SourceFileStorage &v, const GroupSettings &s = GroupSettings());

    bool check_absolute(path &file, bool ignore_errors = false, bool *source_dir = nullptr) const;

    // redirected ops
    auto begin() { return source_files.begin(); }
    auto end() { return source_files.end(); }
    auto begin() const { return source_files.begin(); }
    auto end() const { return source_files.end(); }
    auto empty() const { return source_files.empty(); }
    auto size() const { return source_files.size(); }
    void clear() { source_files.clear(); }

    std::shared_ptr<SourceFile> getFileInternal(const path &) const;

    Target &getTarget() { return target; }
    const Target &getTarget() const { return target; }

protected:
    bool autodetect = false;

    void clearGlobCache();
    void remove_full(const path &file);

    // redirected ops2
    void addFile(const path &, const std::shared_ptr<SourceFile> &);
    bool hasFile(const path &) const;
    void removeFile(const path &);

private:
    using Op = void (SourceFileStorage::*)(const path &);

    SourceFileMap<SourceFile> source_files;
    int index = 0;

    void add_unchecked(const path &f, bool skip = false);
    void add1(const FileRegex &r);
    void remove1(const FileRegex &r);
    void remove_full1(const FileRegex &r);
    void op(const FileRegex &r, Op f);

    SourceFileMap<SourceFile> enumerate_files(const FileRegex &r, bool allow_empty = false) const;
};

}
