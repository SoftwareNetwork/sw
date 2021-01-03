// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "base.h"

namespace sw
{

namespace detail
{

#define STD_MACRO(x, p) static struct __sw_ ## p ## x {} p ## x;
#include "std.inl"
#undef STD_MACRO

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
struct SW_DRIVER_CPP_API NativeTarget
    : Target
    //,protected NativeOptions
{
    NativeTarget(TargetBase &parent, const PackageName &);
    ~NativeTarget();

    virtual path getOutputFile() const;

    //
    virtual void setupCommand(builder::Command &c) const {}
    // move to runnable target? since we might have data only targets
    virtual void setupCommandForRun(builder::Command &c) const { setupCommand(c); } // for Launch?

protected:
    path OutputDir; // output subdir

    virtual path getOutputFileName(const path &root) const;
    virtual path getOutputFileName2(const path &subdir) const;

    virtual void setOutputFile();
    virtual bool isStaticLibrary() const = 0;
};

}
