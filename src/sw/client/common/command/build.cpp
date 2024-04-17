// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>

#include "../commands.h"

#include <sw/builder/execution_plan.h>
#include <sw/core/input.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "build");

static decltype(auto) getInput(sw::SwBuild &b)
{
    return b.addInput(fs::current_path());
}

static void isolated_build(SwClientContext &swctx)
{
    // get targets
    // create dirs

    LOG_INFO(logger, "Determining targets");

    auto b1 = swctx.createBuild();
    auto &b = *b1;

    auto ts = swctx.createInitialSettings();
    for (auto &ii : getInput(b))
    {
        sw::InputWithSettings i(ii);
        i.addSettings(ts);
        b.addInput(i);
    }
    b.loadInputs();
    b.setTargetsToBuild();
    b.resolvePackages();
    b.loadPackages();
    b.prepare();

    // get sources to pass them into getPackages()
    sw::support::SourceDirMap srcs;
    for (const auto &[pkg, tgts] : b.getTargetsToBuild())
    {
        if (tgts.empty())
            throw SW_RUNTIME_ERROR("Empty targets");

        auto &t = **tgts.begin();
        auto s = t.getSource().clone(); // make a copy!
        s->applyVersion(pkg.getVersion());
        if (srcs.find(s->getHash()) != srcs.end())
            continue;
        srcs[s->getHash()].requested_dir = fs::current_path();
    }

    LOG_INFO(logger, "Copying files");

    auto m = getPackages(b, srcs);
    auto d = b.getBuildDirectory() / "isolated";

    for (const auto &[pkg, tgts] : b.getTargetsToBuild())
    {
        if (tgts.empty())
            throw SW_RUNTIME_ERROR("Empty targets");

        auto dir = d / pkg.toString();
        for (auto &[from, to] : m[pkg]->files_map)
        {
            fs::create_directories((dir / to).parent_path());
            fs::copy_file(from, dir / to, fs::copy_options::update_existing);
        }

        ts["driver"]["source-dir-for-package"][pkg.toString()] = to_string(normalize_path(dir));
    }

    LOG_INFO(logger, "Building in isolated environment");

    //
    {
        auto b1 = swctx.createBuild();
        auto &b = *b1;

        for (auto &ii : getInput(b))
        {
            sw::InputWithSettings i(ii);
            i.addSettings(ts);
            b.addInput(i);
        }
        b.build();
    }
}

SUBCOMMAND_DECL(build)
{
    if (getOptions().options_build.build_explan_last)
    {
        auto b = createBuild();
        b->setExecutionPlanFiles(getOptions().options_build.file);
        b->runSavedExecutionPlan(read_file(".sw/last_ep.txt"));
        return;
    }
    if (!getOptions().options_build.build_explan.empty())
    {
        auto b = createBuild();
        b->setExecutionPlanFiles(getOptions().options_build.file);
        b->runSavedExecutionPlan(getOptions().options_build.build_explan);
        return;
    }

    if (getOptions().options_build.build_fetch)
    {
        getOptions().options_fetch.build_after_fetch = true;
        return command_fetch();
    }

    if (getOptions().options_build.isolated_build)
    {
        isolated_build(*this);
        return;
    }

    if (getOptions().list_targets)
    {
        auto b = createBuildWithDefaultInputs();
        b->loadInputs();
        b->setTargetsToBuild(); // or take normal tgts without this step?
        for (auto &&[tgt,_] : b->getTargetsToBuild()) {
            // logger outputs into stderr, but we want stdout here
            std::cout << tgt.toString() << "\n";
        }
        return;
    }

    // defaults or only one of build_arg and -S specified
    //  -S == build_arg
    //  -B == fs::current_path()

    // if -S and build_arg specified:
    //  source dir is taken as -S, config dir is taken as build_arg

    // if -B specified, it is used as is

    auto b = createBuildWithDefaultInputs();
    if (getOptions().options_build.build_default_explan)
    {
        //b->loadInputs();
        b->overrideBuildState(sw::BuildState::InputsLoaded);
        //getContext().clearFileStorages();
        b->setExecutionPlanFiles(getOptions().options_build.file);
        b->runSavedExecutionPlan();
        return;
    }
    b->build();

    // handle ide_fast_path
    if (!getOptions().options_build.ide_fast_path.empty()) {
        String s;
        for (auto &f : b->fast_path_files)
            s += to_string(normalize_path(f)) + "\n";
        write_file(getOptions().options_build.ide_fast_path, s);

        uint64_t mtime = 0;
        for (auto &f : b->fast_path_files)
        {
            auto lwt = fs::last_write_time(f);
            mtime ^= file_time_type2time_t(lwt);
        }
        path fmtime = getOptions().options_build.ide_fast_path;
        fmtime += ".t";
        write_file(fmtime, std::to_string(mtime));
        return;
    }
}
