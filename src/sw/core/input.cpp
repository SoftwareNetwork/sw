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
        tgts.push_back(std::move(tgt));
    }
    // it is possible to get all targets dry run for some reason
    // why?
    return tgts;
}

LogicalInput::LogicalInput(Input &i, const PackagePath &in_prefix)
    : i(i)
    , prefix(in_prefix)
{
}

void LogicalInput::addPackage(const PackageId &in)
{
    if (!prefix.empty() && in.getPath().slice(0, prefix.size()) != prefix)
        throw SW_RUNTIME_ERROR("Trying to add different prefix");
    pkgs.insert(in);
}

std::vector<ITarget*> LogicalInput::loadPackages(SwBuild &b, const PackageSettings &s, const AllowedPackages &allowed_packages)
{
    auto &inpkgs = allowed_packages.empty() ? pkgs : allowed_packages;
    auto tgts = i.loadPackages(b, s, inpkgs, getPrefix());
    if (tgts.empty())
        throw SW_RUNTIME_ERROR("No packages loaded");
    std::vector<ITarget *> r;
    for (auto &tgt : tgts)
    {
        r.push_back(tgt.get());
        targets[tgt->getPackage()][tgt->getSettings()] = std::move(tgt);
    }
    return r;
}

std::vector<ITarget *> LogicalInput::loadPackages(SwBuild &b, const PackageId &p, const PackageSettings &s)
{
    auto find_suitable = [&s](auto &container)
    {
        return std::find_if(container.begin(), container.end(), [&s](const auto &t)
        {
            return t.first.isSubsetOf(s);
        });
    };

    if (auto i = find_suitable(targets[p]); i != targets[p].end())
    {
        std::vector<ITarget *> r;
        r.push_back(i->second.get());
        return r;
    }
    return loadPackages(b, s, UnresolvedPackages{p});
}

/*PackageIdSet LogicalInput::listPackages(SwContext &swctx) const
{
    auto b = swctx.createBuild();
    auto tgts = loadPackages(*b, swctx.getHostSettings());
    PackageIdSet s;
    for (auto &t : tgts)
        s.insert(t->getPackage());
    return s;
}*/

bool LogicalInput::operator==(const LogicalInput &rhs) const
{
    auto h = i.getHash();
    auto rh = rhs.i.getHash();
    return std::tie(pkgs, h, prefix) == std::tie(rhs.pkgs, rh, rhs.prefix);
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

std::vector<ITarget*> UserInput::loadTargets(SwBuild &b) const
{
    std::vector<ITarget*> tgts;
    for (auto &s : settings)
    {
        if (s.empty())
        {
            throw SW_RUNTIME_ERROR("Empty settings requested");
            //SW_UNIMPLEMENTED;
            //continue;
        }

        LOG_TRACE(logger, "Loading input " << i.getName() << ", settings = " << s.toString());

        for (auto &&t : b.getInput(i).loadPackages(b, s))
            tgts.emplace_back(t);
    }
    return tgts;
}

}
