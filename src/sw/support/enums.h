// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <primitives/string.h>
#include <primitives/enums.h>

#include <bitset>

// append only!
enum PackageFlag
{
    // version flag    = vf
    // project flag    = pf
    // dependency flag = df

    //pfHeaderOnly                = 0,    // vf, replaced with language specific flags
    //pfUnstable                  = 1,    // vf, unused, not pfFixed
    //pfNonsecure                 = 2,    // vf, unused, pf, show warning, security flaws
    //pfOutdated                  = 3,    // vf, unused, project is considered for removal
    //pfNonOfficial               = 4,    // pf, unused,
    //pfFixed                     = 5,    // vf, unused, version is fixed and cannot be removed
    //pfExecutable                = 6,    // pf, replaced with project type
    //pfEmpty                     = 7,    // vf, unused, can be used to load & include cmake packages
    //pfPrivateDependency         = 8,    // df, private dependency
    //pfDirectDependency          = 9,    // vf, response only
    //pfIncludeDirectoriesOnly    = 10,   // df, take include dirs from this dep
    //pfLocalProject              = 11,   // vf, not from server, local bs project, remove?

    //pfPreferBinary,   //pf, if binaries are available, do not build the project, use precompiled

    //pfLibrary?,                  // pf
    //pfRootProject?,              // pf
    //pfDirectory?,                // pf

    //pfOptional?                  // df
};

// append only!
enum NativePackageFlag
{
    //pfBuilt                     = 0,
    //pfHeaderOnly                = 1,    // vf
};

enum class NotificationType
{
    None,
    Message,
    Success,
    Warning,
    Error,
};

using SomeFlags = std::bitset<sizeof(uint64_t) * 8>;

namespace sw
{

/*enum class StorageFileType
{
    /// all input (for creating an input package) non-generated files under base source dir
    SourceArchive = 1,

    // everything below is not stable yet

    // binary archive must be always stripped if possible
    // debug symbols to be in separate archive

    // binaries + runtime data
    // or split?
    RuntimeArchive = 2,
    BinaryArchive = RuntimeArchive, // better name?

    RuntimeDataArchive,

    // RuntimeArchive + headers + implib
    DevelopmentArchive,

    // symbols, pdb, dbg info
    SymbolArchive,

    // data files
    // config files
    // used files
};

SW_SUPPORT_API
String toString(StorageFileType);*/

}
