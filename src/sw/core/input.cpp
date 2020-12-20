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
    SW_UNIMPLEMENTED;
    /*if (isLoaded())
        return;
    ep = load1(swctx);
    if (!ep)
        throw SW_RUNTIME_ERROR("Empty entry point");*/
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

UserInput::UserInput(Input &i)
    : i(i)
{
}

const std::unordered_set<PackageSettings> &UserInput::getSettings() const
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
