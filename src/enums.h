/*
 * C++ Archive Network Client
 * Copyright (C) 2016 Egor Pugin
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <bitset>
#include <string>

enum class ProjectType
{
    None,
    Library,
    Executable,
    RootProject,
    Directory,
};

enum class ProjectPathNamespace
{
    None,          // invalid

    com = 1, // closed-source
    org, // open-source
    pvt, // users' packages
};

enum ProjectFlag
{
    // version flag = vf
    // project flag = pf
    pfHeaderOnly, // vf
    pfUnstable,
    pfNonsecure,
    pfOutdated,
    pfNonOfficial,
    pfFixed, // vf
    pfExecutable, // vf
    pfEmpty, // vf - can be used to load & include cmake packages
};

enum class PackagesDirType
{
    None, // use default (user)
    Local, // in current project, dir: cppan
    User, // in user package store
    System, // in system package store
};

using ProjectFlags = std::bitset<sizeof(uint64_t) * 8>;
using UserFlags = std::bitset<sizeof(uint64_t) * 8>;

template <typename E>
constexpr std::size_t toIndex(E e)
{
    return static_cast<std::size_t>(e);
}

std::string toString(ProjectType e);
std::string toString(ProjectPathNamespace e);
std::string toString(PackagesDirType e);

