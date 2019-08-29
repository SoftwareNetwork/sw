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

#include "commands.h"

#include <sw/core/input.h>
#include <sw/manager/database.h>
#include <sw/manager/storage.h>

#include <iostream>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "override");

DEFINE_SUBCOMMAND(override, "Override packages locally.");

static ::cl::opt<String> prefix(::cl::Positional, ::cl::value_desc("prefix"), ::cl::sub(subcommand_override));
static ::cl::opt<bool> list_overridden_packages("l", ::cl::desc("List overridden packages"), ::cl::sub(subcommand_override));
static ::cl::opt<bool> delete_overridden_package("d", ::cl::desc("Delete overridden packages from index"), ::cl::sub(subcommand_override));
static ::cl::opt<path> delete_overridden_package_dir("dd", ::cl::value_desc("sdir"), ::cl::desc("Delete overridden dir packages"), ::cl::sub(subcommand_override));

void override_package_perform(sw::SwContext &swctx, const sw::PackagePath &prefix)
{
    auto b = swctx.createBuild();
    sw::InputWithSettings i(swctx.addInput(fs::current_path()));
    auto ts = b->getContext().getHostSettings();
    i.addSettings(ts);
    b->addInput(i);
    b->loadInputs();

    // one prepare step will find sources
    // maybe add explicit enum value
    //swctx.prepareStep();

    auto gn = swctx.getLocalStorage().getOverriddenPackagesStorage().getPackagesDatabase().getMaxGroupNumber() + 1;
    for (auto &[pkg, desc] : getPackages(*b))
    {
        sw::PackageId pkg2{ prefix / pkg.getPath(), pkg.getVersion() };
        auto dir = fs::absolute(".");
        LOG_INFO(logger, "Overriding " + pkg2.toString() + " to " + dir.u8string());
        // fix deps' prefix
        sw::UnresolvedPackages deps;
        for (auto &d : desc->getData().dependencies)
        {
            if (d.ppath.isAbsolute())
                deps.insert(d);
            else
                deps.insert({ prefix / d.ppath, d.range });
        }
        sw::LocalPackage lp(swctx.getLocalStorage(), pkg2);
        sw::PackageData d;
        d.sdir = dir;
        d.dependencies = deps;
        d.group_number = gn;
        d.prefix = (int)prefix.size();
        swctx.getLocalStorage().getOverriddenPackagesStorage().install(lp, d);
    }
}

SUBCOMMAND_DECL(override)
{
    if (list_overridden_packages)
    {
        auto swctx = createSwContext();
        // sort
        std::set<sw::LocalPackage> pkgs;
        for (auto &p : swctx->getLocalStorage().getOverriddenPackagesStorage().getPackages())
            pkgs.emplace(p);
        for (auto &p : pkgs)
            std::cout << p.toString() << " " << *p.getOverriddenDir() << "\n";
        return;
    }

    if (!delete_overridden_package_dir.empty())
    {
        LOG_INFO(logger, "Delete override for sdir " + delete_overridden_package_dir.u8string());

        auto d = primitives::filesystem::canonical(delete_overridden_package_dir);

        auto swctx = createSwContext();
        std::set<sw::LocalPackage> pkgs;
        for (auto &p : swctx->getLocalStorage().getOverriddenPackagesStorage().getPackages())
        {
            if (*p.getOverriddenDir() == d)
                pkgs.emplace(p);
        }
        for (auto &p : pkgs)
            std::cout << "Deleting " << p.toString() << "\n";

        swctx->getLocalStorage().getOverriddenPackagesStorage().deletePackageDir(d);
        return;
    }

    if (prefix.empty())
        throw SW_RUNTIME_ERROR("Empty prefix");

    if (delete_overridden_package)
    {
        auto swctx = createSwContext();
        sw::PackageId pkg{ prefix };
        LOG_INFO(logger, "Delete override for " + pkg.toString());
        swctx->getLocalStorage().getOverriddenPackagesStorage().deletePackage(pkg);
        return;
    }

    auto swctx = createSwContext();
    override_package_perform(*swctx, prefix);
    return;
}
