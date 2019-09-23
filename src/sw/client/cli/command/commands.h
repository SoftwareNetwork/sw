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

#include <primitives/sw/cl.h>

#include <sw/core/build.h>
#include <sw/core/sw_context.h>
#include <sw/manager/package_data.h>

namespace sw
{

struct StorageWithPackagesDatabase;

}

#define SUBCOMMAND_DECL(n) void cli_##n()
#define SUBCOMMAND_DECL2(n) void cli_##n(sw::SwContext &swctx)
#define SUBCOMMAND(n, d) SUBCOMMAND_DECL(n); SUBCOMMAND_DECL2(n);
#include "commands.inl"
#undef SUBCOMMAND

#define DEFINE_SUBCOMMAND(n, d) ::cl::SubCommand subcommand_##n(#n, d)

std::unique_ptr<sw::SwContext> createSwContext();
std::pair<sw::SourceDirMap, const sw::Input &> fetch(sw::SwBuild &);
std::pair<sw::SourceDirMap, const sw::Input &> fetch(sw::SwContext &);
sw::PackageDescriptionMap getPackages(const sw::SwBuild &, const sw::SourceDirMap & = {});
sw::TargetSettings createInitialSettings(const sw::SwContext &);
std::vector<sw::TargetSettings> createSettings(sw::SwContext &);
std::unique_ptr<sw::SwBuild> setBuildArgsAndCreateBuildAndPrepare(sw::SwContext &, const Strings &inputs);
std::unique_ptr<sw::SwBuild> createBuildAndPrepare(sw::SwContext &);
std::map<sw::PackagePath, sw::VersionSet> getMatchingPackages(const sw::StorageWithPackagesDatabase &, const sw::UnresolvedPackage &);
sw::PackageIdSet getMatchingPackagesSet(const sw::StorageWithPackagesDatabase &, const sw::UnresolvedPackage &);
