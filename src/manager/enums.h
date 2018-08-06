// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
    pfDirectDependency          = 9,    // vf, response only
    //pfIncludeDirectoriesOnly    = 10,   // df, take include dirs from this dep
    pfLocalProject              = 11,   // vf, not from server, local bs project, remove?

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

enum class SettingsType
{
    None,   // use default (user)
    Local,  // in current project, dir: cppan
    User,   // in user package store
    System, // in system package store
    Max,
};

using SomeFlags = std::bitset<sizeof(uint64_t) * 8>;

String toString(SettingsType e);
