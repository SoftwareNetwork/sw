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

std::vector<ITargetPtr> Input::loadPackages(SwBuild &b, const TargetSettings &s, const PackageIdSet &allowed_packages, const PackagePath &prefix) const
{
    // maybe save all targets on load?

    if (!isLoaded())
        throw SW_RUNTIME_ERROR("Input is not loaded");

    LOG_TRACE(logger, "Loading input " << getName() << ", settings = " << s.toString());

    // are we sure that load package can return dry-run?
    // if it cannot return dry run packages, we cannot remove this wrapper
    std::vector<ITargetPtr> tgts;
    auto t = ep->loadPackages(b, s, allowed_packages, prefix);
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

BuildInput::BuildInput(Input &i)
    : i(i)
{
}

void BuildInput::addPackage(const PackageId &in, const PackagePath &in_prefix)
{
    if (prefix && in_prefix != getPrefix())
        throw SW_RUNTIME_ERROR("Trying to add different prefix");
    prefix = in_prefix;
    pkgs.insert(in);
}

std::vector<ITargetPtr> BuildInput::loadPackages(SwBuild &b, const TargetSettings &s, const PackageIdSet &allowed_packages) const
{
    return i.loadPackages(b, s, allowed_packages.empty() ? pkgs : allowed_packages, getPrefix());
}

PackageIdSet BuildInput::listPackages(SwContext &swctx) const
{
    auto b = swctx.createBuild();
    auto tgts = loadPackages(*b, swctx.getHostSettings());
    PackageIdSet s;
    for (auto &t : tgts)
        s.insert(t->getPackage());
    return s;
}

bool BuildInput::operator==(const BuildInput &rhs) const
{
    auto h = i.getHash();
    auto rh = rhs.i.getHash();
    // macos compilation issue
    //return std::tie(pkgs, prefix, h) == std::tie(rhs.pkgs, rhs.prefix, rh);
    if (std::tie(pkgs, h) != std::tie(rhs.pkgs, rh))
        return false;
    if (!prefix && !rhs.prefix)
        return true;
    if (prefix && rhs.prefix && *prefix == *rhs.prefix)
        return true;
    return false;
}

InputWithSettings::InputWithSettings(const BuildInput &i)
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
    s = std::to_string(i.getInput().getHash());
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

    for (auto &s : settings)
    {
        LOG_TRACE(logger, "Loading input " << i.getInput().getName() << ", settings = " << s.toString());

        auto t = i.loadPackages(b, s);
        tgts.insert(tgts.end(), t.begin(), t.end());
    }
    return tgts;
}

}
