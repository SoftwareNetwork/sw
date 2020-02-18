// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "input.h"

#include "build.h"
#include "driver.h"
#include "sw_context.h"

#include <regex>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "input");

namespace sw
{

Input::Input(const IDriver &driver, const path &p, InputType t)
    : driver(driver)
{
    if (p.empty())
        throw SW_RUNTIME_ERROR("empty path");
    this->p = p;
    type = t;
}

Input::~Input()
{
}

path Input::getPath() const
{
    return p;
}

bool Input::operator==(const Input &rhs) const
{
    return getHash() == rhs.getHash();
}

bool Input::operator<(const Input &rhs) const
{
    return getHash() < rhs.getHash();
}

void Input::load(SwContext &swctx)
{
    if (isLoaded())
        return;
    eps = load1(swctx);
    if (eps.empty())
        throw SW_RUNTIME_ERROR("Empty entry points");
}

bool Input::isChanged() const
{
    SW_UNIMPLEMENTED;
    return true;

    /*switch (getType())
    {
    //case InputType::SpecificationFile:
        //return true;
    default:
        SW_UNIMPLEMENTED;
    }*/
}

bool Input::isLoaded() const
{
    return !eps.empty();
}

size_t Input::getHash() const
{
    SW_ASSERT(hash, "Hash was not set");
    return hash;
}

void Input::setHash(size_t h)
{
    hash = h;
}

/*std::unique_ptr<Specification> Input::getSpecification() const
{
    return driver.getSpecification(*this);
}*/

/*PackageVersionGroupNumber Input::getGroupNumber() const
{
    return driver.getGroupNumber(*this);
}*/

const Input::EntryPointsVector &Input::getEntryPoints() const
{
    if (!isLoaded())
        throw SW_RUNTIME_ERROR("Input is not loaded");
    return eps;
}

void Input::setEntryPoints(const EntryPointsVector &in)
{
    SW_ASSERT(!in.empty(), "No entry points provided");
    SW_ASSERT(!isLoaded(), "Input already loaded");
    eps = in;
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
    s = normalize_path(i.getPath());
    for (auto &ss : settings)
        s += ss.getHash();
    return s;
}

std::vector<ITargetPtr> InputWithSettings::loadTargets(SwBuild &b) const
{
    std::vector<ITargetPtr> tgts;

    /*if (i.getType() == InputType::InstalledPackage)
    {
        for (auto &ep : i.getEntryPoints())
        {
            for (auto &s : settings)
            {
                LOG_TRACE(logger, "Loading input " << i.getPackageId().toString() << ", settings = " << s.toString());

                // load only this pkg
                auto pp = i.getPackageId().getPath().slice(0, LocalPackage(b.getContext().getLocalStorage(), i.getPackageId()).getData().prefix);
                auto t = ep->loadPackages(b, s, { i.getPackageId() }, pp);
                tgts.insert(tgts.end(), t.begin(), t.end());
            }
        }
        return tgts;
    }*/

    // for non installed packages we do special handling
    // we register their entry points in swctx
    // because up to this point this is not done

    for (auto &ep : i.getEntryPoints())
    {
        // find difference to set entry points
        auto old = b.getTargets();

        for (auto &s : settings)
        {
            LOG_TRACE(logger, "Loading input " << i.getPath() << ", settings = " << s.toString());

            // load all packages here
            auto t = ep->loadPackages(b, s, {}, {});
            tgts.insert(tgts.end(), t.begin(), t.end());
        }

        // don't forget to set EPs for loaded targets
        for (const auto &t : tgts)
        {
            if (old.find(t->getPackage()) != old.end())
                continue;
            b.setEntryPoint(t->getPackage(), ep);
        }
    }
    return tgts;
}

}
