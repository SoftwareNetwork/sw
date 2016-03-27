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

#include "common.h"
#include "project_path.h"

struct Dependency
{
    using Dependencies = std::map<String, Dependency>;

    ProjectVersionId id;
    ProjectPath package;
    Version version;
    ProjectFlags flags;
    bool direct = false;
    String md5;

    // custom package dir can be used to apply project-wide patches
    // that won't hit any system storage
    PackagesDirType package_dir_type{ PackagesDirType::None };
    path package_dir;
    std::vector<path> patches;
    //

    Dependencies dependencies;

    PackagesDirType get_package_dir_type(PackagesDirType default_type)
    {
        if (package_dir_type != PackagesDirType::None)
            return package_dir_type;
        return default_type;
    }
};

using Dependencies = Dependency::Dependencies;
using DependencyPair = std::pair<String, Version>;
using DependenciesMap = std::map<DependencyPair, Dependency>;
