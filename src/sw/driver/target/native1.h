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

#include "base.h"

namespace sw
{

namespace detail
{

#define STD_MACRO(x, p) static struct __sw_ ## p ## x {} p ## x;
#include "std.inl"
#undef STD_MACRO

struct PrecompiledHeader
{
    path header;
    path source;
    FilesOrdered files;

    //
    path name; // base filename
    String fancy_name;
    //
    path dir;
    path obj; // obj file (msvc)
    path pdb; // pdb file (msvc)
    path pch; // file itself

    path get_base_pch_path() const
    {
        return dir / name;
    }
};

}

enum class ConfigureFlags
{
    Empty = 0x0,

    AtOnly = 0x1, // @
    CopyOnly = 0x2,
    EnableUndefReplacements = 0x4,
    AddToBuild = 0x8,
    ReplaceUndefinedVariablesWithZeros = 0x10,

    Default = Empty, //AddToBuild,
};

/**
* \brief Native Target is a binary target that produces binary files (probably executables).
*/
struct SW_DRIVER_CPP_API NativeTarget : Target
    //,protected NativeOptions
{
    using Target::Target;

    virtual path getOutputFile() const;

    //
    virtual void setupCommand(builder::Command &c) const {}
    // move to runnable target? since we might have data only targets
    virtual void setupCommandForRun(builder::Command &c) const { setupCommand(c); } // for Launch?

protected:
    //
    path OutputDir; // output subdir
    virtual path getOutputFileName(const path &root) const;
    virtual path getOutputFileName2(const path &subdir) const;

    virtual void setOutputFile();
    virtual NativeLinker *getSelectedTool() const = 0;
    virtual bool isStaticLibrary() const = 0;
};

}
