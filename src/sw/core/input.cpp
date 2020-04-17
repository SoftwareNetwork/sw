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

#include "input.h"

#include "build.h"
#include "driver.h"
#include "specification.h"
#include "sw_context.h"

#include <regex>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "input");

namespace sw
{

Input::Input(SwContext &swctx, const IDriver &driver, std::unique_ptr<Specification> spec)
    : swctx(swctx), driver(driver), specification(std::move(spec))
{
    if (!specification)
        throw SW_LOGIC_ERROR("Empty spec");
}

Input::~Input()
{
}

void Input::load()
{
    if (isLoaded())
        return;
    eps = load1(swctx);
    if (eps.empty())
        throw SW_RUNTIME_ERROR("Empty entry points");
}

bool Input::isLoaded() const
{
    return !eps.empty();
}

bool Input::isOutdated(const fs::file_time_type &t) const
{
    return getSpecification().isOutdated(t);
}

String Input::getName() const
{
    // maybe print current packages?
    return getSpecification().getName();
}

size_t Input::getHash() const
{
    return getSpecification().getHash(swctx.getInputDatabase());
}

const Specification &Input::getSpecification() const
{
    return *specification;
}

std::vector<TargetEntryPoint*> Input::getEntryPoints() const
{
    if (!isLoaded())
        throw SW_RUNTIME_ERROR("Input is not loaded");
    std::vector<TargetEntryPoint *> v;
    for (auto &e : eps)
        v.push_back(e.get());
    return v;
}

void Input::setEntryPoints(EntryPointsVector &&in)
{
    SW_ASSERT(!in.empty(), "No entry points provided");
    SW_ASSERT(in.size() == 1, "EP > 1 not implemented for now");
    SW_ASSERT(!isLoaded(), "Input already loaded");
    eps = std::move(in);
}

void Input::addPackage(const LocalPackage &in)
{
    if (prefix != -1 && in.getData().prefix != prefix)
        throw SW_RUNTIME_ERROR("Trying to add different prefix");
    prefix = in.getData().prefix;
    pkgs.insert(in);
}

std::vector<ITargetPtr> Input::loadPackages(SwBuild &b, const TargetSettings &s, const PackageIdSet &allowed_packages, const PackagePath &prefix) const
{
    SW_CHECK(eps.size() == 1);

    LOG_TRACE(logger, "Loading input " << getName() << ", settings = " << s.toString());

    // are we sure that load package can return dry-run?
    // if it cannot return dry run packages, we cannot remove this wrapper
    std::vector<ITargetPtr> tgts;
    auto t = eps[0]->loadPackages(b, s, allowed_packages, prefix);
    for (auto &tgt : t)
    {
        if (tgt->getSettings()["dry-run"] == "true")
            continue;
        tgts.push_back(tgt);
    }
    // it is possible to get all targets dry run for some reason
    // why?
    return tgts;
}

InputWithSettings::InputWithSettings(Input &i)
    : i(i)
{
}

const std::set<TargetSettings> &InputWithSettings::getSettings() const
{
    if (settings.empty())
        throw SW_RUNTIME_ERROR("No input settings provided");
    return settings;
}

void InputWithSettings::addSettings(const TargetSettings &s)
{
    settings.insert(s);
}

String InputWithSettings::getHash() const
{
    String s;
    s = std::to_string(i.getHash());
    for (auto &ss : settings)
        s += ss.getHash();
    return s;
}

std::vector<ITargetPtr> InputWithSettings::loadTargets(SwBuild &b) const
{
    std::vector<ITargetPtr> tgts;

    // for non installed packages we do special handling
    // we register their entry points in swctx
    // because up to this point it is not done

    //for (auto ep : i.getEntryPoints())
    {
        for (auto &s : settings)
        {
            LOG_TRACE(logger, "Loading input " << i.getName() << ", settings = " << s.toString());

            PackagePath prefix;
            auto pkgs = i.getPackages();
            if (!pkgs.empty())
                prefix = pkgs.begin()->getPath().slice(0, i.getPrefix());

            //
            auto t = i.loadPackages(b, s, pkgs, prefix);
            tgts.insert(tgts.end(), t.begin(), t.end());
        }
    }
    return tgts;
}

}
