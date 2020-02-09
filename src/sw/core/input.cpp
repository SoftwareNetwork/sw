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

path RawInput::getPath() const
{
    return p;
}

bool RawInput::operator==(const RawInput &rhs) const
{
    return std::tie(type, p) == std::tie(rhs.type, rhs.p);
}

bool RawInput::operator<(const RawInput &rhs) const
{
    return std::tie(type, p) < std::tie(rhs.type, rhs.p);
}

/*Input::Input(IDriver &driver, const path &p)
    : driver(driver)
{
    if (p.empty())
        throw SW_RUNTIME_ERROR("empty path");
    this->p = p;
    //init(p, swctx);
}*/

/*Input::Input(const LocalPackage &p, const SwContext &swctx)
{
    init(p, swctx);
}*/

Input::Input(const IDriver &driver, const path &p, InputType t)
    : driver(driver)
{
    if (p.empty())
        throw SW_RUNTIME_ERROR("empty path");
    this->p = p;
    type = t;
    /*path p = in;
    if (!p.is_absolute())
        p = fs::absolute(p);
    this->p = p;
    type = t;
    if (!findDriver(type, swctx))
        throw SW_RUNTIME_ERROR("Cannot find suitable driver for " + normalize_path(in));*/
}

bool Input::findDriver(InputType t, const SwContext &swctx)
{
    type = t;
    /*for (auto &[dp, d] : swctx.getDrivers())
    {
        auto files = d->canLoadInput(*this);
        if (!files.empty())
        {
            SW_UNIMPLEMENTED;
            if (*r != getPath())
            {
                if (fs::is_regular_file(*r))
                {
                    type = InputType::SpecificationFile;
                    data = *r;
                }
                else if (fs::is_directory(*r))
                {
                    type = InputType::Directory;
                    data = *r;
                }
                else
                {
                    // what is it?
                    throw SW_RUNTIME_ERROR("Unknown file type: " + normalize_path(*r));
                }
            }
            driver = d.get();
            LOG_DEBUG(logger, "Selecting driver " + dp.toString() + " for input " + normalize_path(*r));
            return true;
        }
    }*/
    return false;
}

void Input::init(const path &in, const SwContext &swctx)
{
    SW_UNIMPLEMENTED;
    /*path p = in;
    if (!p.is_absolute())
        p = fs::absolute(p);

    auto status = fs::status(p);
    if (status.type() != fs::file_type::regular &&
        status.type() != fs::file_type::directory)
    {
        throw SW_RUNTIME_ERROR("Bad file type: " + normalize_path(p));
    }

    data = fs::u8path(normalize_path(primitives::filesystem::canonical(p)));

    // spec or regular file
    if (status.type() == fs::file_type::regular)
    {
        if (findDriver(InputType::SpecificationFile, swctx) ||
            findDriver(InputType::InlineSpecification, swctx))
            return;

        // find in file first: 'sw driver package-id', call that driver on whole file
        auto f = read_file(p);

        static const std::regex r("sw\\s+driver\\s+(\\S+)");
        std::smatch m;
        if (std::regex_search(f, m, r))
        {
            SW_UNIMPLEMENTED;

            //- install driver
            //- load & register it
            //- re-run this ctor

            auto driver_pkg = swctx.install({ m[1].str() }).find(m[1].str());
            return;
        }
    }
    else
    {
        if (findDriver(InputType::DirectorySpecificationFile, swctx) ||
            findDriver(InputType::Directory, swctx))
            return;
    }*/

    throw SW_RUNTIME_ERROR("Cannot select driver for " + normalize_path(p));
}

/*void Input::init(const LocalPackage &p, const SwContext &swctx)
{
    data = p;
    type = InputType::InstalledPackage;
    auto &d = *swctx.getDrivers().begin();
    driver = d.second.get();

    LOG_TRACE(logger, "Selecting driver " + d.first.toString() + " for input " + p.toString());
}*/

//bool Input::operator==(const Input &rhs) const
//{
    //SW_UNIMPLEMENTED;
    /*if (gn == 0)
        return data == rhs.data;
    return std::tie(gn, data) == std::tie(rhs.gn, rhs.data);*/
//}

//bool Input::operator<(const Input &rhs) const
//{
    //SW_UNIMPLEMENTED;
    /*if (gn == 0)
        return data < rhs.data;
    return std::tie(gn, data) < std::tie(rhs.gn, rhs.data);*/
//}

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

void Input::addEntryPoints(const std::vector<TargetEntryPointPtr> &e)
{
    if (isLoaded())
        throw SW_RUNTIME_ERROR("Can add eps only once");
    if (e.empty())
        throw SW_RUNTIME_ERROR("Empty entry points");
    eps = e;
}

bool Input::isLoaded() const
{
    return !eps.empty();
}

std::unique_ptr<Specification> Input::getSpecification() const
{
    return driver.getSpecification(*this);
}

PackageVersionGroupNumber Input::getGroupNumber() const
{
    return driver.getGroupNumber(*this);
}

InputWithSettings::InputWithSettings(const Input &i)
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
    if (!i.isLoaded())
        throw SW_RUNTIME_ERROR("Input is not loaded");

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
        for (const auto &[pkg, tgts] : b.getTargets())
        {
            if (old.find(pkg) != old.end())
                continue;
            b.getContext().setEntryPoint(pkg, ep);
        }
    }
    return tgts;
}

}
