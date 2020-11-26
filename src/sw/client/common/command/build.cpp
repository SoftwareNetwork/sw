// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>

#include "../commands.h"

#include <sw/builder/execution_plan.h>
#include <sw/core/input.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "build");

static void isolated_build(SwClientContext &swctx)
{
    // get targets
    // create dirs

    LOG_INFO(logger, "Determining targets");

    auto b1 = swctx.createBuild();
    auto &b = *b1;

    auto ts = swctx.createInitialSettings();
    for (auto &i : swctx.makeCurrentPathInputs())
    {
        i.addSettings(ts);
        b.addInput(i);
    }
    b.loadInputs();
    SW_UNIMPLEMENTED;
    //b.setTargetsToBuild();
    b.resolvePackages();
    //b.loadPackages();
    b.prepare();

    // get sources to pass them into getPackages()
    sw::support::SourceDirMap srcs;
    SW_UNIMPLEMENTED;
    /*for (const auto &[pkg, tgts] : b.getTargetsToBuild())
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
    }*/

    LOG_INFO(logger, "Building in isolated environment");

    //
    {
        auto b1 = swctx.createBuild();
        auto &b = *b1;

        for (auto &i : swctx.makeCurrentPathInputs())
        {
            i.addSettings(ts);
            b.addInput(i);
        }
        b.build();
    }
}

SUBCOMMAND_DECL(build)
{
    if (!getOptions().options_build.build_explan.empty())
    {
        auto b = createBuild();
        b->overrideBuildState(sw::BuildState::Prepared);
        auto cmds = sw::ExecutionPlan::load(getOptions().options_build.build_explan, *b);
        auto p = sw::ExecutionPlan::create(cmds);
        b->execute(*p);
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

    // defaults or only one of build_arg and -S specified
    //  -S == build_arg
    //  -B == fs::current_path()

    // if -S and build_arg specified:
    //  source dir is taken as -S, config dir is taken as build_arg

    // if -B specified, it is used as is

    auto b = createBuildWithDefaultInputs();
    if (getOptions().options_build.build_default_explan)
    {
        b->loadInputs();
        //getContext().clearFileStorages();
        b->runSavedExecutionPlan();
        return;
    }
    b->build();
}
