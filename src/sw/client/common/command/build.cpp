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
    sw::SourceDirMap srcs;
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
        for (auto &[from, to] : m[pkg]->getData().files_map)
        {
            fs::create_directories((dir / to).parent_path());
            fs::copy_file(from, dir / to, fs::copy_options::update_existing);
        }

        ts["driver"]["source-dir-for-package"][pkg.toString()] = normalize_path(dir);
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

    if (cl_isolated_build)
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
