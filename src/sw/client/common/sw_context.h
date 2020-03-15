/*
 * SW - Build System and Package Manager
 * Copyright (C) 2020 Egor Pugin
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

#include <primitives/filesystem.h>
#include <sw/core/input.h>
#include <sw/core/settings.h>
#include <sw/manager/source.h>

namespace sw
{
struct SwContext;
struct SwBuild;
}

struct Executor;
struct Options;

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

// not thread safe
struct SwClientContext
{
    using Base = sw::SwContext;

    SwClientContext(const path &local_storage_root_dir, const Options &options);

    sw::SwContext &getContext();

    Options &getOptions() { return *options; }
    const Options &getOptions() const { return *options; }

    std::unique_ptr<sw::SwBuild> createBuild();
    std::unique_ptr<sw::SwBuild> createBuild(const Inputs &);
    std::unique_ptr<sw::SwBuild> createBuildAndPrepare(const Inputs &);

    sw::TargetSettings createInitialSettings();
    std::vector<sw::TargetSettings> createSettings();

    void addInputs(sw::SwBuild &b, const Inputs &i);

    // main commands
#define SUBCOMMAND(n) void command_##n();
#include "commands.inl"
#undef SUBCOMMAND

    // extensions
    std::pair<sw::SourceDirMap, std::vector<sw::Input*>> fetch();
    std::pair<sw::SourceDirMap, std::vector<sw::Input*>> fetch(sw::SwBuild &);
    void run(const sw::PackageId &pkg, primitives::Command &c);

private:
    path local_storage_root_dir;
    std::unique_ptr<Executor> executor;
    std::unique_ptr<sw::SwContext> swctx;
    // we can copy options into unique ptr also
    std::unique_ptr<Options> options;
};
