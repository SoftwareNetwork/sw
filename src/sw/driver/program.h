// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2019-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <sw/builder/program.h>

namespace sw
{

struct Build;
struct SourceFile;
struct Target;

/*enum class TransformType
{
    Unspecified,

    FileToFile,     // 1-to-1    (compilers)
    FilesToFile,    // many-to-1 (linkers)
};*/

struct SW_DRIVER_CPP_API TransformProgram : Program
{
    //TransformType type = TransformType::Unspecified;
    //StringSet input_extensions;

    using Program::Program;
};

struct SW_DRIVER_CPP_API FileToFileTransformProgram : TransformProgram
{
    using TransformProgram::TransformProgram;

    virtual std::shared_ptr<SourceFile> createSourceFile(const Target &t, const path &input) const = 0;
};

using FileToFileTransformProgramPtr = std::shared_ptr<FileToFileTransformProgram>;

struct ProgramGroup : Program
{
    using Program::Program;

    std::shared_ptr<builder::Command> getCommand() const override { return nullptr; }
    //Version &getVersion() override { SW_UNIMPLEMENTED; }

    virtual void activate(Build &s) const = 0;
};

using ProgramGroupPtr = std::shared_ptr<ProgramGroup>;

}
