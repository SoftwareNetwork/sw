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

#include <nlohmann/json.hpp>

#include <iostream>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "override");

DEFINE_SUBCOMMAND(override, "Override packages locally.");

static ::cl::opt<String> prefix(::cl::Positional, ::cl::value_desc("prefix"), ::cl::sub(subcommand_override));
static ::cl::opt<bool> list_overridden_packages("l", ::cl::desc("List overridden packages"), ::cl::sub(subcommand_override));
static ::cl::opt<bool> delete_overridden_package("d", ::cl::desc("Delete overridden packages from index"), ::cl::sub(subcommand_override));
static ::cl::opt<path> delete_overridden_package_dir("dd", ::cl::value_desc("sdir"), ::cl::desc("Delete overridden dir packages"), ::cl::sub(subcommand_override));
static ::cl::opt<path> load_overridden_packages_from_file("load", ::cl::value_desc("fn"), ::cl::desc("Load overridden packages desc from file and apply it."), ::cl::sub(subcommand_override));
static ::cl::opt<path> save_overridden_packages_to_file("save", ::cl::value_desc("fn"), ::cl::desc("Save overridden packages desc to file."), ::cl::sub(subcommand_override));

static void override_package_perform(sw::SwContext &swctx, sw::PackagePath prefix)
{
    auto dir = fs::absolute(".");
    sw::PackageDescriptionMap pm;

    auto override_packages = [&](sw::PackageVersionGroupNumber gn)
    {
        for (auto &[pkg, desc] : pm)
        {
            sw::PackageId pkg2{ prefix / pkg.getPath(), pkg.getVersion() };
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
    };

    if (!load_overridden_packages_from_file.empty())
    {
        auto j = nlohmann::json::parse(read_file(load_overridden_packages_from_file));
        dir = j["sdir"].get<String>();
        prefix = j["prefix"].get<String>();
        for (auto &[k,v] : j["packages"].items())
            pm[k] = std::make_unique<sw::JsonPackageDescription>(v.dump());
        override_packages(j["group_number"].get<sw::PackageVersionGroupNumber>());
        return;
    }

    auto b = swctx.createBuild();
    sw::InputWithSettings i(swctx.addInput(fs::current_path()));
    auto ts = b->getContext().getHostSettings();
    i.addSettings(ts);
    b->addInput(i);
    b->loadInputs();
    pm = getPackages(*b);

    if (!save_overridden_packages_to_file.empty())
    {
        nlohmann::json j;
        j["sdir"] = normalize_path(dir);
        j["prefix"] = prefix.toString();
        j["group_number"] = std::hash<String>()(read_file(i.getInput().getPath() / "sw.cpp"));
        for (auto &[pkg, desc] : pm)
            j["packages"][pkg.toString()] = nlohmann::json::parse(desc->getString());
        write_file(save_overridden_packages_to_file, j.dump(4));
        return;
    }

    override_packages(std::hash<String>()(read_file(i.getInput().getPath() / "sw.cpp")));
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

    if (prefix.empty() && load_overridden_packages_from_file.empty())
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
