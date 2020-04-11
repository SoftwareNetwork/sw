/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2019 Egor Pugin
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

#include "sw_context.h"

#include <primitives/sw/cl.h>

#include <sw/core/build.h>
#include <sw/core/sw_context.h>
#include <sw/manager/package_data.h>

#define SW_DOC_URL "https://software-network.org/client/sw.pdf"

namespace sw
{
struct StorageWithPackagesDatabase;
}

#include <cl.llvm.h>

#define SUBCOMMAND_DECL(n) void SwClientContext::command_##n()

sw::PackageDescriptionMap getPackages(const sw::SwBuild &, const sw::SourceDirMap & = {});
std::map<sw::PackagePath, sw::VersionSet> getMatchingPackages(const sw::StorageWithPackagesDatabase &, const String &unresolved_arg);

// create command
struct ProjectTemplate
{
    String name;
    String desc;
    String target;
    FilesMap config;
    FilesMap files;
    StringSet dependencies;
};

struct ProjectTemplates
{
    std::map<String, ProjectTemplate> templates;
    std::map<path, String> files;
};

const ProjectTemplates &getProjectTemplates();
