// Copyright (C) 2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

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

struct PredefinedProgram
{
    std::shared_ptr<Program> program;
};

}
