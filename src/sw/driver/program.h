/*
 * SW - Build System and Package Manager
 * Copyright (C) 2019-2020 Egor Pugin
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
