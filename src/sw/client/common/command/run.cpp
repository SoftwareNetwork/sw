// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

/*
TODO:
    - add other OSs
    - add win7
*/

#include "../commands.h"

#include <sw/core/input.h>
#include <sw/manager/storage.h>

#include <primitives/command.h>

#include <vector>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "sw.cli.run");

void run1(const sw::LocalPackage &pkg, primitives::Command &c, bool gRunAppInContainer);

#ifndef _WIN32
void run1(const sw::LocalPackage &pkg, primitives::Command &c, bool gRunAppInContainer)
{
    error_code ec;
    c.execute(ec);

    if (ec)
        throw SW_RUNTIME_ERROR(c.getError());
}
#endif

static void run(sw::SwBuild &b, const sw::PackageId &pkg, primitives::Command &c, bool print, bool gRunAppInContainer)
{
    SW_UNIMPLEMENTED;
    /*if (b.getTargetsToBuild()[pkg].empty())
        throw SW_RUNTIME_ERROR("No such target: " + pkg.toString());

    // take the last target
    auto i = b.getTargetsToBuild()[pkg].end() - 1;
    auto &s = (*i)->getInterfaceSettings();
    if (!s["run_command"])
        throw SW_RUNTIME_ERROR("Target is not runnable: " + pkg.toString());
    auto &sc = s["run_command"].getMap();

    c.setProgram(sc["program"].getPathValue(b.getContext().getLocalStorage()));
    if (sc["arguments"])
    {
        for (auto &a : sc["arguments"].getArray())
            c.push_back(a.getValue());
    }
    if (sc["environment"])
    {
        for (auto &[k, v] : sc["environment"].getMap())
            c.environment[k] = v.getValue();
    }
    //if (sc["create_new_console"] && sc["create_new_console"] == "true")
    //c.create_new_console = true;

    SCOPE_EXIT
    {
        if (print)
            LOG_INFO(logger, c.print());
    };

    sw::LocalPackage p(b.getContext().getLocalStorage(), pkg);
    run1(p, c, gRunAppInContainer);*/
}

void SwClientContext::run(const sw::PackageId &pkg, primitives::Command &c)
{
    getOptions().targets_to_build.push_back(pkg.toString());

    Strings inputs;
    if (pkg.getPath().isRelative())
    {
        if (getOptions().options_run.input.empty())
            inputs.push_back(".");
        else
            inputs.push_back(getOptions().options_run.input);
    }
    else
        inputs.push_back(pkg.toString());

    auto b = createBuildAndPrepare(inputs);
    b->build();

    ::run(*b, pkg, c, getOptions().options_run.print_command, getOptions().options_run.run_app_in_container);
}

SUBCOMMAND_DECL(run)
{
    bool valid_target = true;
    try
    {
        sw::PackageId pkg(getOptions().options_run.target);
    }
    catch (std::exception &)
    {
        valid_target = false;
    }

    // for such commands we inherit them
    // TODO: check for program subsystem later to detach gui apps
    //primitives::Command c;
    auto b = createBuild();
    sw::builder::Command c;
    c.setContext(*b);
    c.always = true;

    c.inherit = true;
    c.in.inherit = true;

    for (auto &a : getOptions().options_run.args)
        c.push_back(a);

    if (!getOptions().options_run.wdir.empty())
        c.working_directory = getOptions().options_run.wdir;

    if (!valid_target && fs::exists((String &)getOptions().options_run.target))
    {
        auto b = createBuildAndPrepare({ getOptions().options_run.target });
        b->build();
        auto inputs = b->getInputs();
        if (inputs.size() != 1)
            throw SW_RUNTIME_ERROR("More than one input provided");
        auto tgts = inputs[0].loadPackages(*b);
        // TODO: add better target detection
        // check only for executable targets
        if (tgts.size() != 1)
            throw SW_RUNTIME_ERROR("More than one target provided in input");

        ::run(*b, (*tgts.begin())->getPackage(), c, getOptions().options_run.print_command, getOptions().options_run.run_app_in_container);
        return;
    }

    // resolve
    try
    {
        SW_UNIMPLEMENTED;
        //auto p = getContext().resolve(sw::UnresolvedPackages{ getOptions().options_run.target });
        //getOptions().options_run.target = p[getOptions().options_run.target]->toString();
    }
    catch (...)
    {
        // local package won't be resolved
    }

    //
    run(getOptions().options_run.target, c);
}
