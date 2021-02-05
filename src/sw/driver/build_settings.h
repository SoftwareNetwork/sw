// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "types.h"
//#include "compiler/compiler.h"

#include <sw/builder/os.h>
#include <sw/core/target.h>

namespace sw
{
// toolchain

struct
    //SW_DRIVER_CPP_API
    NativeToolchain
{
    //struct OsSdk SDK;

    // libc, libcpp
    // OS SDK (win sdk, macos sdk, linux headers etc.)
    //std::vector<NativeCompiledTarget*> ForcedDependencies;

    //std::shared_ptr<NativeLinker> Librarian;
    //std::shared_ptr<NativeLinker> Linker;

    // rc (resource compiler)
    // ar, more tools...
    // more native compilers (cuda etc.)
    //LinkerType LinkerType; // rename - use type from selected tool
    BuildLibrariesAs LibrariesType = LibraryType::Shared;
    ::sw::ConfigurationType ConfigurationType = ConfigurationType::Release;

    // win, vs
    bool MT = false;
    // toolset
    // win sdk
    // add XP support
    // more settings

    // misc
    //bool CopySharedLibraries = true;

    // service

    /// set on server to eat everything
    //bool AssignAll = false;

    // members
    //String getConfig() const;

    //bool operator<(const NativeToolchain &) const;
    //bool operator==(const NativeToolchain &) const;
};

struct SW_DRIVER_CPP_API BuildSettings
{
    OS TargetOS;
    NativeToolchain Native;

    // other langs?
    // make polymorphic?

    BuildSettings() = default;
    BuildSettings(const PackageSettings &);

    String getTargetTriplet() const;
    PackageSettings getPackageSettings() const;
};

}
