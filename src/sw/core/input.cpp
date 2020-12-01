// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

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
    ep = load1(swctx);
    if (!ep)
        throw SW_RUNTIME_ERROR("Empty entry point");
}

bool Input::isLoaded() const
{
    return !!ep;
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

Specification &Input::getSpecification()
{
    return *specification;
}

const Specification &Input::getSpecification() const
{
    return *specification;
}

void Input::setEntryPoint(EntryPointPtr in)
{
    if (isLoaded())
        throw SW_RUNTIME_ERROR("Input already loaded");
    SW_ASSERT(in, "No entry points provided");
    ep = std::move(in);
}

std::vector<ITargetPtr> Input::loadPackages(SwBuild &b, const PackageSettings &s, const AllowedPackages &allowed_packages, const PackagePath &prefix) const
{
    // maybe save all targets on load?

    // 1. If we load all installed packages, we might spend a lot of time here,
    //    in case if all of the packages are installed and config is huge (aws, qt).
    //    Also this might take a lot of memory.
    //
    // 2. If we load package by package we might spend a lot of time in subsequent loads.
    //

    if (!isLoaded())
        throw SW_RUNTIME_ERROR("Input is not loaded: " + std::to_string(getHash()));

    LOG_TRACE(logger, "Loading input " << getName() << ", settings = " << s.toString());

    // are we sure that load package can return dry-run?
    // if it cannot return dry run packages, we cannot remove this wrapper
    std::vector<ITargetPtr> tgts;
    auto t = ep->loadPackages(b, s, allowed_packages, prefix);
    for (auto &tgt : t)
    {
        if (tgt->getSettings()["dry-run"] == "true")
            continue;
        tgts.push_back(std::move(tgt));
    }
    // it is possible to get all targets dry run for some reason
    // why?
    return tgts;
}

UserInput::UserInput(Input &i)
    : i(i)
{
}

const std::set<PackageSettings> &UserInput::getSettings() const
{
    if (settings.empty())
        throw SW_RUNTIME_ERROR("No input settings provided");
    return settings;
}

void UserInput::addSettings(const PackageSettings &s)
{
    settings.insert(s);
}

String UserInput::getHash() const
{
    String s;
    s = std::to_string(i.getHash());
    for (auto &ss : settings)
        s += ss.getHash();
    return s;
}

/*std::vector<ITargetPtr> UserInput::loadPackages(SwBuild &b) const
{
    std::vector<ITargetPtr> tgts;
    for (auto &s : settings)
    {
        if (s.empty())
        {
            //throw SW_RUNTIME_ERROR("Empty settings requested");
            SW_UNIMPLEMENTED;
            //continue;
        }

        LOG_TRACE(logger, "Loading input " << i.getName() << ", settings = " << s.toString());

        for (auto &&t : i.loadPackages(b, s, {}, {}))
            tgts.emplace_back(std::move(t));
    }
    return tgts;
}*/

}
