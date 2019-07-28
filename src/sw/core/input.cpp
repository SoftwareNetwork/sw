// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "input.h"

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
    switch (getType())
    {
    case InputType::InstalledPackage:
        s = getPackageId().toString();
        break;
    default:
        s = normalize_path(getPath());
        break;
    }
    for (auto &ss : settings)
        s += ss.getHash();
    return s;
}

Input::Input(const path &p, const SwContext &swctx)
{
    init(p, swctx);
}

Input::Input(const PackageId &p, const SwContext &swctx)
{
    init(p, swctx);
}

void Input::init(const path &in, const SwContext &swctx)
{
    auto find_driver = [this, &swctx](auto t)
    {
        type = t;
        for (auto &[_, d] : swctx.getDrivers())
        {
            if (d->canLoad(*this))
            {
                driver = d.get();
                return true;
            }
        }
        return false;
    };

    path p = in;
    if (p.empty())
        p = swctx.source_dir;
    if (!p.is_absolute())
        p = fs::absolute(p);

    auto status = fs::status(p);
    data = path(normalize_path(p));

    // spec or regular file
    if (status.type() == fs::file_type::regular)
    {
        if (find_driver(InputType::SpecificationFile) ||
            find_driver(InputType::InlineSpecification))
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
    else if (status.type() == fs::file_type::directory)
    {
        if (find_driver(InputType::DirectorySpecificationFile) ||
            find_driver(InputType::Directory))
            return;
    }
    else
        throw SW_RUNTIME_ERROR("Bad file type: " + normalize_path(p));

    throw SW_RUNTIME_ERROR("Cannot select driver for " + normalize_path(p));
}

void Input::init(const PackageId &p, const SwContext &swctx)
{
    data = p;
    type = InputType::InstalledPackage;
    driver = swctx.getDrivers().begin()->second.get();
}

bool Input::isChanged() const
{
    SW_UNIMPLEMENTED;
    return true;

    switch (getType())
    {
    //case InputType::SpecificationFile:
        //return true;
    default:
        SW_UNIMPLEMENTED;
    }
}

void Input::addEntryPoint(const TargetEntryPointPtr &e)
{
    eps.push_back(e);
}

bool Input::isLoaded() const
{
    return !eps.empty();
}

void Input::load(SwBuild &b)
{
    if (eps.empty())
        throw SW_RUNTIME_ERROR("No entry points set");
    for (auto &s : settings)
    {
        for (auto &ep : eps)
            ep->loadPackages(b, s, {});
    }
}

String Input::getSpecification() const
{
    return driver->getSpecification(*this);
}

}
