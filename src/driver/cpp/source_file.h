// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <compiler.h>
#include <language.h>
#include <node.h>
#include <types.h>

#include <memory>

namespace sw
{

struct Language;
struct LanguageStorage;
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

    size_t sizeKnown() const;
    size_t sizeSkipped() const;

    void resolve();
    //void resolveRemoved();
    void startAssignOperation();

    SourceFile &operator[](path F);

    // for option groups
    void merge(const SourceFileStorage &v, const GroupSettings &s = GroupSettings());

    bool check_absolute(path &file, bool ignore_errors = false) const;

protected:
    bool autodetect = false;

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
    void op(const FileRegex &r, Op f);
};

// other files can be source files, but not compiled files
// they'll be processed with other tools
// so we cannot replace or inherit SourceFile from Compiler
struct SW_DRIVER_CPP_API SourceFile : File
{
    bool created = true;
    bool skip = false;

    SourceFile(const path &input);
    SourceFile(const SourceFile &) = default;
    virtual ~SourceFile() = default;

    virtual std::shared_ptr<builder::Command> getCommand() const { return nullptr; }
    virtual Files getGeneratedDirs() const { return Files(); }
    virtual std::shared_ptr<SourceFile> clone() const { return std::make_shared<SourceFile>(*this); }
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

    NativeSourceFile(const path &input, const path &output, NativeCompiler *c);
    NativeSourceFile(const NativeSourceFile &rhs);
    virtual ~NativeSourceFile();

    virtual std::shared_ptr<builder::Command> getCommand() const override;
    virtual Files getGeneratedDirs() const override;
    void setSourceFile(const path &input, const path &output);
    void setOutputFile(const path &output);
};

struct SW_DRIVER_CPP_API ASMSourceFile : NativeSourceFile
{
    ASMSourceFile(const path &input, const path &output, ASMCompiler *c);

    virtual ~ASMSourceFile() = default;

    virtual std::shared_ptr<SourceFile> clone() const override;
};

struct SW_DRIVER_CPP_API CSourceFile : NativeSourceFile
{
    CSourceFile(const path &input, const path &output, CCompiler *c);

    virtual ~CSourceFile() = default;

    virtual std::shared_ptr<SourceFile> clone() const override;
};

struct SW_DRIVER_CPP_API CPPSourceFile : NativeSourceFile
{
    CPPSourceFile(const path &input, const path &output, CPPCompiler *c);

    virtual ~CPPSourceFile() = default;

    virtual std::shared_ptr<SourceFile> clone() const override;
};

struct PrecompiledHeader
{
    path header;
    path source;
    // path pch; // file itself
};

}
