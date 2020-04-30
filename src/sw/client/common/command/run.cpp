/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
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

bool gRunAppInContainer = false;

void run1(const sw::LocalPackage &pkg, primitives::Command &c);

#ifndef _WIN32
void run1(const sw::LocalPackage &pkg, primitives::Command &c)
{
    throw SW_RUNTIME_ERROR("not implemented");
}
#endif

static void run(sw::SwBuild &b, const sw::PackageId &pkg, primitives::Command &c)
{
    if (b.getTargetsToBuild()[pkg].empty())
        throw SW_RUNTIME_ERROR("No such target: " + pkg.toString());

    // take the last target
    auto i = b.getTargetsToBuild()[pkg].end() - 1;
    auto &s = (*i)->getInterfaceSettings();
    if (!s["run_command"])
        throw SW_RUNTIME_ERROR("Target is not runnable: " + pkg.toString());
    auto &sc = s["run_command"].getMap();

    c.setProgram(sc["program"].getValue());
    if (sc["arguments"])
    {
        for (auto &a : sc["arguments"].getArray())
            c.push_back(std::get<sw::TargetSetting::Value>(a));
    }
    if (sc["environment"])
    {
        for (auto &[k, v] : sc["environment"].getMap())
            c.environment[k] = v.getValue();
    }
    //if (sc["create_new_console"] && sc["create_new_console"] == "true")
    //c.create_new_console = true;

    sw::LocalPackage p(b.getContext().getLocalStorage(), pkg);
    run1(p, c);
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

    ::run(*b, pkg, c);
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
        auto tgts = inputs[0].loadTargets(*b);
        // TODO: add better target detection
        // check only for executable targets
        if (tgts.size() != 1)
            throw SW_RUNTIME_ERROR("More than one target provided in input");

        ::run(*b, (*tgts.begin())->getPackage(), c);
        return;
    }

    // resolve
    auto p = getContext().resolve(sw::UnresolvedPackages{ getOptions().options_run.target });
    getOptions().options_run.target = p[getOptions().options_run.target]->toString();

    //
    run(getOptions().options_run.target, c);
}
