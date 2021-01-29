// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>

#include "../commands.h"

#include <sw/core/driver.h>
//#include <sw/core/input.h>
#include <sw/manager/database.h>
#include <sw/manager/storage.h>

#include <nlohmann/json.hpp>

#include <iostream>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "override");

static void override_package_perform(SwClientContext &swctx, sw::PackagePath prefix)
{
    auto dir = fs::canonical(".");
    SW_UNIMPLEMENTED;
    //sw::PackageDescriptionMap pm;

    auto override_packages = [&]()
    {
        //for (auto &[pkg, desc] : pm)
        {
            SW_UNIMPLEMENTED;
            /*sw::PackageName pkg2{ prefix / pkg.getPath(), pkg.getVersion() };
            // fix deps' prefix
            sw::UnresolvedPackages deps;
            for (auto &d : desc->dependencies)
            {
                if (d.getPath().isAbsolute())
                    deps.insert(d);
                else
                    deps.insert({ prefix / d.getPath(), d.getRange() });
            }*/
            SW_UNIMPLEMENTED;
            /*sw::LocalPackage lp(swctx.getContext().getLocalStorage(), pkg2);
            sw::PackageData d;
            d.sdir = dir;
            d.dependencies = deps;
            d.prefix = (int)prefix.size();
            LOG_INFO(logger, "Overriding " + pkg2.toString() + " to " + to_string(d.sdir.u8string()));
            swctx.getContext().getLocalStorage().getOverriddenPackagesStorage().install(lp, d);*/
        }
    };

    /*if (!swctx.getOptions().options_override.load_overridden_packages_from_file.empty())
    {
        auto j = nlohmann::json::parse(read_file(swctx.getOptions().options_override.load_overridden_packages_from_file));
        dir = j["sdir"].get<String>();
        prefix = j["prefix"].get<String>();
        for (auto &[k,v] : j["packages"].items())
            pm.emplace(k, std::make_unique<sw::PackageDescription>(v));
        return;
    }

    auto b = swctx.createBuild();
    auto inputs = swctx.makeCurrentPathInputs();
    auto ts = b->getContext().getHostSettings();
    for (auto &i : inputs)
    {
        i.addSettings(ts);
        b->addInput(i);
        // take only first for now
        if (inputs.size() != 1)
        {
            LOG_WARN(logger, "Multiple inputs detected. Taking the first one.");
            break;
        }
    }
    b->loadInputs();
    pm = getPackages(*b);

    if (!swctx.getOptions().options_override.save_overridden_packages_to_file.empty())
    {
        nlohmann::json j;
        j["sdir"] = to_string(normalize_path(dir));
        j["prefix"] = prefix.toString();
        for (auto &[pkg, desc] : pm)
            j["packages"][pkg.toString()] = desc->toJson();
        write_file(swctx.getOptions().options_override.save_overridden_packages_to_file, j.dump(4));
        return;
    }

    override_packages();*/
}

SUBCOMMAND_DECL(override)
{
    if (getOptions().options_override.list_overridden_packages)
    {
        // sort
        std::set<sw::LocalPackage> pkgs;
        SW_UNIMPLEMENTED;
        /*for (auto &p : getContext().getLocalStorage().getOverriddenPackagesStorage().getPackages())
            pkgs.emplace(p);
        /*for (auto &p : pkgs)
            std::cout << p.toString() << " " << *p.getOverriddenDir() << "\n";*/
        return;
    }

    if (!getOptions().options_override.delete_overridden_package_dir.empty())
    {
        LOG_INFO(logger, "Delete override for sdir " + to_string(getOptions().options_override.delete_overridden_package_dir.u8string()));

        auto d = primitives::filesystem::canonical(getOptions().options_override.delete_overridden_package_dir);

        std::set<sw::LocalPackage> pkgs;
        SW_UNIMPLEMENTED;
        /*for (auto &p : getContext().getLocalStorage().getOverriddenPackagesStorage().getPackages())
        {
            if (*p.getOverriddenDir() == d)
                pkgs.emplace(p);
        }
        for (auto &p : pkgs)
            std::cout << "Deleting " << p.toString() << "\n";

        getContext().getLocalStorage().getOverriddenPackagesStorage().deletePackageDir(d);*/
        return;
    }

    if (getOptions().options_override.prefix.empty() && getOptions().options_override.load_overridden_packages_from_file.empty())
        throw SW_RUNTIME_ERROR("Empty prefix");

    if (getOptions().options_override.delete_overridden_package)
    {
        sw::PackageId pkg{ getOptions().options_override.prefix };
        LOG_INFO(logger, "Delete override for " + pkg.toString());
        SW_UNIMPLEMENTED;
        //getContext().getLocalStorage().getOverriddenPackagesStorage().deletePackage(pkg);
        return;
    }

    override_package_perform(*this, getOptions().options_override.prefix);
}
