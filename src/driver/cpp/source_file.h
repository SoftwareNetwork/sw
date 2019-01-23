// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <compiler.h>
#include <language_type.h>
#include <node.h>
#include <types.h>

#include <memory>

namespace sw
{

struct Language;
struct SourceFile;
struct Target;
struct TargetBase;

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
struct SW_DRIVER_CPP_API SourceFileStorage : protected SourceFileMap<SourceFile>,
    IterableOptions<SourceFileStorage>
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

protected:
    bool autodetect = false;

    void clearGlobCache() { glob_cache.clear(); }
    void remove_full(const path &file);

    std::optional<PackageId> findPackageIdByExtension(const String &e) const;
    Program *findProgramByExtension(const String &e) const;
    Language *findLanguageByPackageId(const PackageId &) const;
    Language *findLanguageByExtension(const String &e) const;

private:
    struct FileOperation
    {
        std::variant<path, FileRegex> op;
        bool add = true;
    };
    using Op = void (SourceFileStorage::*)(const path &);

    std::vector<FileOperation> file_ops;
    mutable std::unordered_map<path, std::map<bool /* recursive */, Files>> glob_cache;

    void add_unchecked(const path &f, bool skip = false);
    void add1(const FileRegex &r);
    void remove1(const FileRegex &r);
    void remove_full1(const FileRegex &r);
    void op(const FileRegex &r, Op f);

    SourceFileMap<SourceFile> enumerate_files(const FileRegex &r) const;
};

// other files can be source files, but not compiled files
// they'll be processed with other tools
// so we cannot replace or inherit SourceFile from Compiler
struct SW_DRIVER_CPP_API SourceFile : File
{
    bool created = true;
    bool skip = false;
    bool postponed = false; // remove later?
    bool show_in_ide = true;
    path install_dir;
    Strings args; // additional args to job, move to native?
    String fancy_name; // for output

    SourceFile(const Target &t, const path &input);
    SourceFile(const SourceFile &) = default;
    virtual ~SourceFile() = default;

    virtual std::shared_ptr<builder::Command> getCommand(const TargetBase &t) const { return nullptr; }
    //virtual Files getGeneratedDirs() const { return Files(); }
    virtual std::shared_ptr<SourceFile> clone() const { return std::make_shared<SourceFile>(*this); }

    bool isActive() const;

    void showInIde(bool s) { show_in_ide = s; }
    bool showInIde() { return show_in_ide; }

    static String getObjectFilename(const TargetBase &t, const path &p);
};

struct SW_DRIVER_CPP_API NativeSourceFile : SourceFile
{
    enum BuildAsType
    {
        BasedOnExtension,
        ASM = (int)LanguageType::ASM,
        C = (int)LanguageType::C,
        CPP = (int)LanguageType::CPP,
    };

    File output;
    std::shared_ptr<NativeCompiler> compiler;
    std::unordered_set<SourceFile*> dependencies;
    BuildAsType BuildAs = BuildAsType::BasedOnExtension;

    NativeSourceFile(const Target &t, NativeCompiler *c, const path &input, const path &output);
    NativeSourceFile(const NativeSourceFile &rhs);
    virtual ~NativeSourceFile();

    std::shared_ptr<builder::Command> getCommand(const TargetBase &t) const override;
    //virtual Files getGeneratedDirs() const override;
    //void setSourceFile(const path &input, const path &output);
    void setOutputFile(const TargetBase &t, const path &input, const path &output_dir); // bad name?
    void setOutputFile(const path &output);
    String getObjectFilename(const TargetBase &t, const path &p);
};

struct PrecompiledHeader
{
    path header;
    path source;
    // path pch; // file itself
    bool force_include_pch = false;

    // internal
    bool created = false;
};

struct SW_DRIVER_CPP_API RcToolSourceFile : SourceFile
{
    File output;
    std::shared_ptr<RcTool> compiler;

    RcToolSourceFile(const Target &t, RcTool *c, const path &input, const path &output);

    std::shared_ptr<builder::Command> getCommand(const TargetBase &t) const override;
};

struct SW_DRIVER_CPP_API CSharpSourceFile : SourceFile
{
    using SourceFile::SourceFile;
};

struct SW_DRIVER_CPP_API RustSourceFile : SourceFile
{
    using SourceFile::SourceFile;
};

struct SW_DRIVER_CPP_API GoSourceFile : SourceFile
{
    using SourceFile::SourceFile;
};

struct SW_DRIVER_CPP_API FortranSourceFile : SourceFile
{
    using SourceFile::SourceFile;
};

struct SW_DRIVER_CPP_API JavaSourceFile : SourceFile
{
    using SourceFile::SourceFile;
};

struct SW_DRIVER_CPP_API KotlinSourceFile : SourceFile
{
    using SourceFile::SourceFile;
};

struct SW_DRIVER_CPP_API DSourceFile : SourceFile
{
    using SourceFile::SourceFile;
};

// TODO: maybe use virtual function with enum instead of different types for SFs?

}
