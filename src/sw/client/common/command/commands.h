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

#define SW_DOC_URL "https://software-network.org/client/sw.pdf"

namespace sw
{

struct StorageWithPackagesDatabase;

}

#include <cl.llvm.h>
#define OPTIONS_ARG Options &options
#define OPTIONS_ARG_CONST const OPTIONS_ARG

#define SUBCOMMAND_DECL(n) void cli_##n(OPTIONS_ARG)
#define SUBCOMMAND_DECL2(n) void cli_##n(sw::SwContext &swctx, OPTIONS_ARG)
#define SUBCOMMAND(n) SUBCOMMAND_DECL(n); SUBCOMMAND_DECL2(n);
#include "commands.inl"
#undef SUBCOMMAND

#define DEFINE_SUBCOMMAND(n, d) ::cl::SubCommand subcommand_##n(#n, d)

#define DEFINE_SUBCOMMAND_ALIAS(command, alias)          \
    DEFINE_SUBCOMMAND(alias, "Alias for " #command "."); \
    SUBCOMMAND_DECL(alias)                               \
    {                                                    \
        cli_##command();                                 \
    }

struct Inputs
{
    Inputs() = default;
    Inputs(const String &s)
    {
        inputs.push_back(s);
    }
    Inputs(const Strings &s)
    {
        inputs = s;
    }

    void addInputPair(const sw::TargetSettings &settings, const String &input)
    {
        input_pairs.push_back({settings, input});
    }

    const auto &getInputs() const
    {
        if (inputs.empty() && input_pairs.empty())
            inputs.push_back(".");
        return inputs;
    }
    const auto &getInputPairs() const { return input_pairs; }

private:
    mutable Strings inputs;
    std::vector<std::pair<sw::TargetSettings, String>> input_pairs;
};

std::unique_ptr<sw::SwContext> createSwContext(OPTIONS_ARG_CONST);

std::unique_ptr<sw::SwBuild> createBuild(sw::SwContext &, OPTIONS_ARG_CONST);
std::unique_ptr<sw::SwBuild> createBuild(sw::SwContext &, const Inputs &, OPTIONS_ARG_CONST);
std::unique_ptr<sw::SwBuild> createBuildAndPrepare(sw::SwContext &, const Inputs &, OPTIONS_ARG_CONST);

sw::TargetSettings createInitialSettings(const sw::SwContext &);
std::vector<sw::TargetSettings> createSettings(sw::SwContext &, OPTIONS_ARG_CONST);

std::pair<sw::SourceDirMap, const sw::Input &> fetch(sw::SwBuild &, OPTIONS_ARG_CONST);
std::pair<sw::SourceDirMap, const sw::Input &> fetch(sw::SwContext &, OPTIONS_ARG_CONST);

sw::PackageDescriptionMap getPackages(const sw::SwBuild &, const sw::SourceDirMap & = {});
std::map<sw::PackagePath, sw::VersionSet> getMatchingPackages(const sw::StorageWithPackagesDatabase &, const String &unresolved_arg);
void run(sw::SwContext &swctx, const sw::PackageId &pkg, primitives::Command &c, OPTIONS_ARG);
