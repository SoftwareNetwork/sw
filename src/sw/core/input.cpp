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
    return std::get<path>(data);
}

PackageId RawInput::getPackageId() const
{
    return std::get<PackageId>(data);
}

bool RawInput::operator==(const RawInput &rhs) const
{
    return data == rhs.data;
}

bool RawInput::operator<(const RawInput &rhs) const
{
    return data < rhs.data;
}

Input::Input(const path &p, const SwContext &swctx)
{
    if (p.empty())
        throw SW_RUNTIME_ERROR("empty path");
    init(p, swctx);
}

Input::Input(const LocalPackage &p, const SwContext &swctx)
{
    init(p, swctx);
}

Input::Input(const path &in, InputType t, const SwContext &swctx)
{
    path p = in;
    if (!p.is_absolute())
        p = fs::absolute(p);
    data = p;
    type = t;
    if (!findDriver(type, swctx))
        throw SW_RUNTIME_ERROR("Cannot find suitable driver for " + normalize_path(in));
}

bool Input::findDriver(InputType t, const SwContext &swctx)
{
    type = t;
    for (auto &[_, d] : swctx.getDrivers())
    {
        auto r = d->canLoadInput(*this);
        if (r)
        {
            if (*r != getPath())
            {
                type = InputType::SpecificationFile;
                data = *r;
            }
            driver = d.get();
            return true;
        }
    }
    return false;
}

void Input::init(const path &in, const SwContext &swctx)
{
    path p = in;
    if (!p.is_absolute())
        p = fs::absolute(p);

    auto status = fs::status(p);
    if (status.type() != fs::file_type::regular &&
        status.type() != fs::file_type::directory)
    {
        throw SW_RUNTIME_ERROR("Bad file type: " + normalize_path(p));
    }

    data = path(normalize_path(primitives::filesystem::canonical(p)));

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

            /*
            - install driver
            - load & register it
            - re-run this ctor
            */

            auto driver_pkg = swctx.install({ m[1].str() }).find(m[1].str());
            return;
        }
    }
    else
    {
        if (findDriver(InputType::DirectorySpecificationFile, swctx) ||
            findDriver(InputType::Directory, swctx))
            return;
    }

    throw SW_RUNTIME_ERROR("Cannot select driver for " + normalize_path(p));
}

void Input::init(const LocalPackage &p, const SwContext &swctx)
{
    /*gn = p.getData().group_number;

    data = p.getDirSrc2();
    if (findDriver(InputType::DirectorySpecificationFile, swctx) ||
        findDriver(InputType::Directory, swctx))
        return;
    throw SW_RUNTIME_ERROR("Cannot select driver for " + p.toString());*/

    data = p;
    type = InputType::InstalledPackage;
    driver = swctx.getDrivers().begin()->second.get();
}

bool Input::operator==(const Input &rhs) const
{
    if (gn == 0)
        return data == rhs.data;
    return std::tie(gn, data) == std::tie(rhs.gn, rhs.data);
}

bool Input::operator<(const Input &rhs) const
{
    if (gn == 0)
        return data < rhs.data;
    return std::tie(gn, data) < std::tie(rhs.gn, rhs.data);
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

String Input::getSpecification() const
{
    return driver->getSpecification(*this);
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
    switch (i.getType())
    {
    case InputType::InstalledPackage:
        s = i.getPackageId().toString();
        break;
    default:
        s = normalize_path(i.getPath());
        break;
    }
    for (auto &ss : settings)
        s += ss.getHash();
    return s;
}

std::vector<ITargetPtr> InputWithSettings::loadTargets(SwBuild &b) const
{
    if (!i.isLoaded())
        throw SW_RUNTIME_ERROR("Input is not loaded");

    std::vector<ITargetPtr> tgts;

    if (i.getType() == InputType::InstalledPackage)
    {
        for (auto &ep : i.getEntryPoints())
        {
            for (auto &s : settings)
            {
                // load only this pkg
                auto t = ep->loadPackages(b, s, { i.getPackageId() });
                tgts.insert(tgts.end(), t.begin(), t.end());
            }
        }
        return tgts;
    }

    // for non installed packages we do special handling
    // we register their entry points in swctx
    // because up to this point this is not done

    for (auto &ep : i.getEntryPoints())
    {
        // find difference to set entry points
        auto old = b.getTargets();

        for (auto &s : settings)
        {
            // load all packages here
            auto t = ep->loadPackages(b, s, {});
            tgts.insert(tgts.end(), t.begin(), t.end());
        }

        // don't forget to set EPs for loaded targets
        for (const auto &[pkg, tgts] : b.getTargets())
        {
            if (old.find(pkg) != old.end())
                continue;
            b.getContext().getTargetData(pkg).setEntryPoint(ep);
        }
    }
    return tgts;
}

}
