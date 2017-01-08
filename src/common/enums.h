/*
 * Copyright (C) 2016-2017, Egor Pugin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "cppan_string.h"

#include <bitset>

enum class ProjectType
{
    None,
    Library,
    Executable,
    RootProject,
    Directory,
};

enum class LibraryType
{
    Static,
    Shared,
    Module, // from CMake
};

enum class ExecutableType
{
    Default,
    Win32,
};

// append only!
enum ProjectFlag
{
    // version flag    = vf
    // project flag    = pf
    // dependency flag = df

    pfHeaderOnly                = 0,    // vf
    //pfUnstable                  = 1,    // vf, unused, not pfFixed
    //pfNonsecure                 = 2,    // vf, unused, pf, show warning, security flaws
    //pfOutdated                  = 3,    // vf, unused, project is considered for removal
    //pfNonOfficial               = 4,    // pf, unused,
    //pfFixed                     = 5,    // vf, unused, version is fixed and cannot be removed
    pfExecutable                = 6,    // pf
    //pfEmpty                     = 7,    // vf, unused, can be used to load & include cmake packages
    pfPrivateDependency         = 8,    // df, private dependency
    pfDirectDependency          = 9,    // vf, response only
    pfIncludeDirectoriesOnly    = 10,   // df, take include dirs from this dep
    pfLocalProject              = 11,   // vf, not from server, local bs project

    //pfPreferBinary,   //pf, if binaries are available, do not build the project, use precompiled

    //pfLibrary?,                  // pf
    //pfRootProject?,              // pf
    //pfDirectory?,                // pf

    //pfOptional?                  // df
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

using ProjectFlags = std::bitset<sizeof(uint64_t) * 8>;

template <typename E>
constexpr std::underlying_type_t<E> toIndex(E e)
{
    return static_cast<std::underlying_type_t<E>>(e);
}

String toString(ProjectType e);
String toString(SettingsType e);

String getFlagsString(const ProjectFlags &flags);
