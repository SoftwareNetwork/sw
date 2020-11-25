// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <primitives/filesystem.h>
#include <sw/core/input.h>
#include <sw/core/settings.h>
#include <sw/support/package_path.h>
#include <sw/support/source.h>

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
    std::vector<std::pair<sw::PackageSettings, String>> input_pairs;
};

// not thread safe
struct SW_CLIENT_COMMON_API SwClientContext
{
    using Base = sw::SwContext;

    SwClientContext(const Options &options);
    virtual ~SwClientContext();

    sw::SwContext &getContext(bool allow_network = true);
    void resetContext();
    bool hasContext() const { return !!swctx_; }

    Options &getOptions() { return *options; }
    const Options &getOptions() const { return *options; }

    void initNetwork();

    std::unique_ptr<sw::SwBuild> createBuild();
    std::unique_ptr<sw::SwBuild> createBuildInternal();
    std::unique_ptr<sw::SwBuild> createBuildWithDefaultInputs();
    std::unique_ptr<sw::SwBuild> createBuild(const Inputs &);
    std::unique_ptr<sw::SwBuild> createBuildAndPrepare(const Inputs &);

    sw::PackageSettings createInitialSettings();
    std::vector<sw::PackageSettings> createSettings();

    void addInputs(sw::SwBuild &b, const Inputs &i);
    Strings &getInputs();
    const Strings &getInputs() const;

    //
    String listPredefinedTargets();
    String listPrograms();
    Programs listCompilers();

    // main commands
#define SUBCOMMAND(n) virtual void command_##n();
#include "commands.inl"
#undef SUBCOMMAND

    // extensions
    std::pair<sw::support::SourceDirMap, std::vector<sw::LogicalInput>> fetch();
    std::pair<sw::support::SourceDirMap, std::vector<sw::LogicalInput>> fetch(sw::SwBuild &);
    void run(const sw::PackageId &pkg, primitives::Command &c);
    static Strings getAliasArguments(const String &aliasname);

private:
    path local_storage_root_dir;
    std::unique_ptr<Executor> executor;
    std::unique_ptr<sw::SwContext> swctx_;
    // we can copy options into unique ptr also
    std::unique_ptr<Options> options;
    std::optional<sw::TargetMap> tm;

    const sw::TargetMap &getPredefinedTargets(sw::SwContext &swctx);
    static StringSet listCommands();
};

void setHttpSettings(const Options &);
void setupLogger(const std::string &log_level, const Options &options, bool simple = true);
