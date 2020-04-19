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
#include <sw/manager/package_path.h>
#include <sw/manager/source.h>

namespace sw
{
struct SwContext;
struct SwBuild;
}

struct Executor;
struct Options;

struct Program
{
    sw::PackagePath ppath;
    String desc;

    struct data
    {
        sw::TargetContainer *c = nullptr;
    };

    using Container = sw::PackageVersionMapBase<data, std::unordered_map, primitives::version::VersionMap>;

    Container releases;
    Container prereleases;
};

using Programs = std::vector<Program>;

struct Inputs
{
    Inputs() = default;
    Inputs(const String &s);
    Inputs(const Strings &s);
    Inputs(const Strings &s, const Strings &input_pairs);

    const auto &getInputs() const { return inputs; }
    const auto &getInputPairs() const { return input_pairs; }

private:
    mutable Strings inputs;
    std::vector<std::pair<sw::TargetSettings, String>> input_pairs;
};

// not thread safe
struct SwClientContext
{
    using Base = sw::SwContext;

    SwClientContext();
    SwClientContext(const Options &options);
    virtual ~SwClientContext();

    sw::SwContext &getContext();
    void resetContext();

    Options &getOptions() { return *options; }
    const Options &getOptions() const { return *options; }

    std::unique_ptr<sw::SwBuild> createBuild();
    std::unique_ptr<sw::SwBuild> createBuildInternal();
    std::unique_ptr<sw::SwBuild> createBuildWithDefaultInputs();
    std::unique_ptr<sw::SwBuild> createBuild(const Inputs &);
    std::unique_ptr<sw::SwBuild> createBuildAndPrepare(const Inputs &);

    sw::TargetSettings createInitialSettings();
    std::vector<sw::TargetSettings> createSettings();

    void addInputs(sw::SwBuild &b, const Inputs &i);
    Strings &getInputs() const;

    //
    String listPredefinedTargets();
    String listPrograms();
    Programs listCompilers();

    // main commands
#define SUBCOMMAND(n) virtual void command_##n();
#include "commands.inl"
#undef SUBCOMMAND

    // extensions
    std::pair<sw::SourceDirMap, std::vector<sw::BuildInput>> fetch();
    std::pair<sw::SourceDirMap, std::vector<sw::BuildInput>> fetch(sw::SwBuild &);
    void run(const sw::PackageId &pkg, primitives::Command &c);

private:
    path local_storage_root_dir;
    std::unique_ptr<Executor> executor;
    std::unique_ptr<sw::SwContext> swctx_;
    // we can copy options into unique ptr also
    std::unique_ptr<Options> options;
    std::optional<sw::TargetMap> tm;

    const sw::TargetMap &getPredefinedTargets(sw::SwContext &swctx);
};

void setHttpSettings(const Options &);
void setupLogger(const std::string &log_level, const Options &options, bool simple = true);
