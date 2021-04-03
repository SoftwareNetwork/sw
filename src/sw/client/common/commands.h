// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "sw_context.h"

#include <primitives/sw/cl.h>

#include <sw/core/build.h>
#include <sw/core/sw_context.h>
#include <sw/support/package_data.h>

#define SW_DOC_URL "https://software-network.org/client/sw.pdf"

namespace sw
{
struct StorageWithPackagesDatabase;
}

#include <cl.llvm.h>

#define SUBCOMMAND_DECL(n) void SwClientContext::command_##n()

sw::PackageDescriptionMap getPackages(const sw::SwBuild &, const sw::support::SourceDirMap & = {}, std::map<const sw::Input*, std::vector<sw::PackageId>> * = nullptr);
std::map<sw::PackagePath, sw::VersionSet> getMatchingPackages(const sw::StorageWithPackagesDatabase &, const String &unresolved_arg);

// create command
struct ProjectTemplate
{
    String name;
    String desc;
    String target;
    FilesMap config;
    FilesMap files;
    FilesMap other_files;
    StringSet dependencies;
};

struct ProjectTemplates
{
    std::map<String, ProjectTemplate> templates;
    std::map<path, String> files;
};

SW_CLIENT_COMMON_API
const ProjectTemplates &getProjectTemplates();
