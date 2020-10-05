// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "compiler/compiler.h"
#include "types.h"

#include <sw/builder/node.h>

#include <memory>

#include "source_file_storage.h"

namespace sw
{

struct SourceFile;
struct Target;

// other files can be source files, but not compiled files
// they'll be processed with other tools
// so we cannot replace or inherit SourceFile from Compiler
struct SW_DRIVER_CPP_API SourceFile : /*TargetFile, */ICastable
{
    path file;
    //bool created = true;
    bool skip = false;
    //bool postponed = false; // remove later?
    //bool show_in_ide = true;
    path install_dir;
    Strings args; // additional args to job, move to native?
    String fancy_name; // for output
    bool skip_unity_build = false;
    int index; // index of file during addition

    SourceFile(const path &input);
    SourceFile(const SourceFile &) = default;
    virtual ~SourceFile() = default;

    //virtual std::shared_ptr<builder::Command> getCommand(const Target &t) const { return nullptr; }

    bool isActive() const;

    //void showInIde(bool s) { show_in_ide = s; }
    //bool showInIde() { return show_in_ide; }

    //static path getObjectFilename(const Target &t, const path &p);
};

/*struct SW_DRIVER_CPP_API NativeSourceFile : SourceFile
{
    enum BuildAsType
    {
        BasedOnExtension,
        ASM,
        C,
        CPP,
    };

    path output; // object file
    std::unique_ptr<Program> compiler;
    std::unordered_set<SourceFile*> dependencies; // explicit file deps? currently used for pchs
    BuildAsType BuildAs = BuildAsType::BasedOnExtension;
    bool skip_linking = false; // produce object file only
    bool skip_pch = false;

    NativeSourceFile(const NativeCompiler &c, const path &input, const path &output);
    NativeSourceFile(const NativeSourceFile &rhs);
    virtual ~NativeSourceFile();

    //std::shared_ptr<builder::Command> getCommand(const Target &t) const override;
    NativeCompiler &getCompiler() const;

    //void setSourceFile(const path &input, const path &output);
    void setOutputFile(const Target &t, const path &input, const path &output_dir); // bad name?
    void setOutputFile(const path &output);
    path getObjectFilename(const Target &t, const path &p) const;
};*/

/*struct SW_DRIVER_CPP_API RcToolSourceFile : SourceFile
{
    path output;
    std::unique_ptr<Program> compiler;

    RcToolSourceFile(const RcTool &c, const path &input, const path &output);

    std::shared_ptr<builder::Command> getCommand(const Target &t) const override;
    RcTool &getCompiler() const;
};*/

}
