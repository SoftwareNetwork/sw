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

#include <sw/core/driver.h>
#include <sw/core/input.h>
#include <sw/manager/database.h>
#include <sw/manager/storage.h>

#include <nlohmann/json.hpp>

#include <iostream>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "override");

static void override_package_perform(SwClientContext &swctx, sw::PackagePath prefix)
{
    auto dir = fs::canonical(".");
    sw::PackageDescriptionMap pm;

    auto override_packages = [&]()
    {
        for (auto &[pkg, desc] : pm)
        {
            sw::PackageId pkg2{ prefix / pkg.getPath(), pkg.getVersion() };
            LOG_INFO(logger, "Overriding " + pkg2.toString() + " to " + dir.u8string());
            // fix deps' prefix
            sw::UnresolvedPackages deps;
            for (auto &d : desc->dependencies)
            {
                if (d.ppath.isAbsolute())
                    deps.insert(d);
                else
                    deps.insert({ prefix / d.ppath, d.range });
            }
            sw::LocalPackage lp(swctx.getContext().getLocalStorage(), pkg2);
            sw::PackageData d;
            d.sdir = dir;
            d.dependencies = deps;
            d.prefix = (int)prefix.size();
            swctx.getContext().getLocalStorage().getOverriddenPackagesStorage().install(lp, d);
        }
    };

    if (!swctx.getOptions().options_override.load_overridden_packages_from_file.empty())
    {
        auto j = nlohmann::json::parse(read_file(swctx.getOptions().options_override.load_overridden_packages_from_file));
        dir = j["sdir"].get<String>();
        prefix = j["prefix"].get<String>();
        for (auto &[k,v] : j["packages"].items())
            pm.emplace(k, std::make_unique<sw::PackageDescription>(v));
        return;
    }

    auto b = swctx.createBuild();
    auto inputs = b->addInput(fs::current_path());
    SW_CHECK(inputs.size() == 1); // for now
    for (auto &i : inputs)
    {
        sw::InputWithSettings ii(i);
        auto ts = b->getContext().getHostSettings();
        ii.addSettings(ts);
        b->addInput(ii);
    }
    b->loadInputs();
    pm = getPackages(*b);

    if (!swctx.getOptions().options_override.save_overridden_packages_to_file.empty())
    {
        nlohmann::json j;
        j["sdir"] = normalize_path(dir);
        j["prefix"] = prefix.toString();
        for (auto &[pkg, desc] : pm)
            j["packages"][pkg.toString()] = desc->toJson();
        write_file(swctx.getOptions().options_override.save_overridden_packages_to_file, j.dump(4));
        return;
    }

    override_packages();
}

SUBCOMMAND_DECL(override)
{
    if (getOptions().options_override.list_overridden_packages)
    {
        // sort
        std::set<sw::LocalPackage> pkgs;
        for (auto &p : getContext().getLocalStorage().getOverriddenPackagesStorage().getPackages())
            pkgs.emplace(p);
        for (auto &p : pkgs)
            std::cout << p.toString() << " " << *p.getOverriddenDir() << "\n";
        return;
    }

    if (!getOptions().options_override.delete_overridden_package_dir.empty())
    {
        LOG_INFO(logger, "Delete override for sdir " + getOptions().options_override.delete_overridden_package_dir.u8string());

        auto d = primitives::filesystem::canonical(getOptions().options_override.delete_overridden_package_dir);

        std::set<sw::LocalPackage> pkgs;
        for (auto &p : getContext().getLocalStorage().getOverriddenPackagesStorage().getPackages())
        {
            if (*p.getOverriddenDir() == d)
                pkgs.emplace(p);
        }
        for (auto &p : pkgs)
            std::cout << "Deleting " << p.toString() << "\n";

        getContext().getLocalStorage().getOverriddenPackagesStorage().deletePackageDir(d);
        return;
    }

    if (getOptions().options_override.prefix.empty() && getOptions().options_override.load_overridden_packages_from_file.empty())
        throw SW_RUNTIME_ERROR("Empty prefix");

    if (getOptions().options_override.delete_overridden_package)
    {
        sw::PackageId pkg{ getOptions().options_override.prefix };
        LOG_INFO(logger, "Delete override for " + pkg.toString());
        getContext().getLocalStorage().getOverriddenPackagesStorage().deletePackage(pkg);
        return;
    }

    override_package_perform(*this, getOptions().options_override.prefix);
}
