// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "sw_context.h"

#include "build.h"
#include "input.h"
#include "driver.h"

#include <sw/manager/storage.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "context");

namespace sw
{

IDriver::~IDriver() = default;

SwCoreContext::SwCoreContext(const path &local_storage_root_dir)
    : SwBuilderContext(local_storage_root_dir)
{
    host_settings = createHostSettings();

    LOG_TRACE(logger, "Host configuration: " + getHostSettings().toString());
}

SwCoreContext::~SwCoreContext()
{
}

TargetSettings SwCoreContext::createHostSettings() const
{
    return toTargetSettings(getHostOs());
}

void SwCoreContext::setHostSettings(const TargetSettings &s)
{
    host_settings = s;

    // always log!
    LOG_TRACE(logger, "New host configuration: " + getHostSettings().toString());
}

TargetData &SwCoreContext::getTargetData(const PackageId &pkg)
{
    return target_data[pkg];
}

const TargetData &SwCoreContext::getTargetData(const PackageId &pkg) const
{
    auto i = target_data.find(pkg);
    if (i == target_data.end())
        throw SW_RUNTIME_ERROR("No target data for package: " + pkg.toString());
    return i->second;
}

void SwCoreContext::setEntryPoint(const PackageId &pkgid, const TargetEntryPointPtr &ep)
{
    LocalPackage p(getLocalStorage(), pkgid);
    return setEntryPoint(p, ep);
}

void SwCoreContext::setEntryPoint(const LocalPackage &p, const TargetEntryPointPtr &ep)
{
    if (!ep)
        return;

    auto iep = entry_points.find(p);
    if (iep != entry_points.end())
    {
        if (iep->second != ep)
            throw SW_RUNTIME_ERROR("Setting entry point twice for package " + p.toString());
        //getTargetData(p); // "register" package
        return;
    }
    //getTargetData(p).setEntryPoint(ep);
    //getTargetData(p); // "register" package
    entry_points[p] = ep; // after target data

    // local package
    if (p.getPath().isRelative())
        return;

    //try
    //{
        setEntryPoint(p.getData().group_number, ep);
    //}
    //catch (...)
    //{
    //}
}

void SwCoreContext::setEntryPoint(PackageVersionGroupNumber gn, const TargetEntryPointPtr &ep)
{
    if (gn == 0)
        return;

    auto iepgn = entry_points_by_group_number.find(gn);
    if (iepgn != entry_points_by_group_number.end())
    {
        if (iepgn->second != ep)
            throw SW_RUNTIME_ERROR("Setting entry point twice for group_number " + std::to_string(gn));
        return;
    }
    entry_points_by_group_number[gn] = ep;
}

TargetEntryPointPtr SwCoreContext::getEntryPoint(const PackageId &pkgid) const
{
    LocalPackage p(getLocalStorage(), pkgid);
    return getEntryPoint(p);
}

TargetEntryPointPtr SwCoreContext::getEntryPoint(const LocalPackage &p) const
{
    auto gn = p.getData().group_number;
    if (gn == 0)
    {
        gn = get_specification_hash(read_file(p.getDirSrc2() / "sw.cpp"));
        p.setGroupNumber(gn);
        ((PackageData &)p.getData()).group_number = gn;
    }

    auto ep = getEntryPoint(gn);
    if (ep)
        return ep;
    auto i = entry_points.find(p);
    if (i != entry_points.end())
        return i->second;
    return {};
}

TargetEntryPointPtr SwCoreContext::getEntryPoint(PackageVersionGroupNumber p) const
{
    if (p == 0)
        //return {};
        throw SW_RUNTIME_ERROR("Empty entry point"); // assert?

    auto i = entry_points_by_group_number.find(p);
    if (i == entry_points_by_group_number.end())
        return {};
    return i->second;
}

SwContext::SwContext(const path &local_storage_root_dir)
    : SwCoreContext(local_storage_root_dir)
{
}

SwContext::~SwContext()
{
}

std::unique_ptr<SwBuild> SwContext::createBuild1()
{
    return std::make_unique<SwBuild>(*this, fs::current_path() / SW_BINARY_DIR);
}

std::unique_ptr<SwBuild> SwContext::createBuild()
{
    auto b = createBuild1();
    b->getTargets() = getPredefinedTargets();
    return std::move(b);
}

void SwContext::registerDriver(const PackageId &pkg, std::unique_ptr<IDriver> &&driver)
{
    auto [_, inserted] = drivers.insert_or_assign(pkg, std::move(driver));
    if (inserted)
        LOG_TRACE(logger, "Registering driver: " + pkg.toString());
}

void SwContext::executeBuild(const path &in)
{
    clearFileStorages();
    auto b = createBuild1();
    b->runSavedExecutionPlan(in);
}

std::vector<Input *> SwContext::addInput(const String &i)
{
    path p(i);
    if (fs::exists(p))
        return addInput(p);
    else
        return addInput(resolve(i));
}

std::vector<Input *> SwContext::addInput(const path &in)
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

    p = fs::u8path(normalize_path(primitives::filesystem::canonical(p)));

    std::vector<Input *> inputs_local;

    auto findDriver = [this, &p, &inputs_local](auto type) -> bool
    {
        for (auto &[dp, d] : drivers)
        {
            auto inpts = d->detectInputs(p, type);
            if (inpts.empty())
                continue;
            for (auto &i : inpts)
            {
                auto it = std::find_if(inputs.begin(), inputs.end(), [&i = *i](const auto &p)
                {
                    return *p == i;
                });
                if (it != inputs.end())
                    inputs_local.push_back(&**it);
                else
                {
                    inputs.push_back(std::move(i));
                    inputs_local.push_back(&*inputs.back());
                }

                LOG_DEBUG(logger, "Selecting driver " + dp.toString() + " for input " + normalize_path(i->getPath()));
            }
            return true;
        }
        return false;
    };

    // spec or regular file
    if (status.type() == fs::file_type::regular)
    {
        if (findDriver(InputType::SpecificationFile) ||
            findDriver(InputType::InlineSpecification))
            return inputs_local;

        SW_UNIMPLEMENTED;

        // find in file first: 'sw driver package-id', call that driver on whole file
        /*auto f = read_file(p);

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
        }*/
    }
    else
    {
        if (findDriver(InputType::DirectorySpecificationFile) ||
            findDriver(InputType::Directory))
            return inputs_local;
    }

    SW_UNIMPLEMENTED;

    /*auto input = std::make_unique<Input>(i, *this);
    auto it = std::find_if(inputs.begin(), inputs.end(), [&i = *input](const auto &p)
    {
        return *p == i;
    });
    if (it != inputs.end())
        return **it;
    inputs.push_back(std::move(input));
    return *inputs.back();*/
}

std::vector<Input *> SwContext::addInput(const LocalPackage &p)
{
    if (!p.getData().group_number)
        throw SW_RUNTIME_ERROR("Missing group number");

    auto v = addInput(p.getGroupLeader().getDirSrc2());
    SW_CHECK(v.size() == 1);
    return v;
    /*auto &i = addInput(p.getGroupLeader().getDirSrc2());
    if (i.isLoaded())
        return i;
    //if (getTargetData().find(p) == getTargetData().end())
        //return i;
    auto ep = getEntryPoint(p);
    if (ep)
        i.addEntryPoints({ ep });
    return i;*/
}

void SwContext::loadEntryPoints(const std::set<Input*> &inputs, bool set_eps)
{
    for (auto &i : inputs)
        i->load(*this);
    return;

    std::map<const IDriver *, std::vector<Input*>> active_drivers;
    for (auto &i : inputs)
    {
        //if (!i->isLoaded())
            //active_drivers[&i->getDriver()].push_back(i);
    }
    for (auto &[d, g] : active_drivers)
    {
        std::vector<RawInput> inputs;
        for (auto &i : g)
            inputs.push_back(*i);
        //auto eps = d->createEntryPoints(*this, inputs); // batch load
        //if (eps.size() != inputs.size())
            //throw SW_RUNTIME_ERROR("Incorrect number of returned entry points");
        //for (size_t i = 0; i < eps.size(); i++)
        {
            // when loading installed package, eps[i] may be empty
            // (ep already exists in driver)
            // so we take ep from context
            // test: sw build org.sw.demo.madler.zlib
            //if (eps[i].empty())
            {
                SW_UNREACHABLE;
                //if (inputs[i].getType() != InputType::InstalledPackage)
                    //throw SW_RUNTIME_ERROR("unexpected input type");
                //g[i]->addEntryPoints({ getEntryPoint(inputs[i].getPackageId()) });
            }
            //else
                //g[i]->addEntryPoints(eps[i]);

            //if (!set_eps)
                //continue;

            //if (inputs[i].getType() != InputType::InstalledPackage)
            /*if (eps[i].empty())
            {
                // for non installed packages we must create entry points in sw context
                auto b = createBuild();
                auto s = getHostSettings();
                s["driver"]["dry-run"] = "true"; // used only to get pkgs list
                for (auto &ep : eps[i])
                {
                    auto tgts = ep->loadPackages(*b, s, {}, {});
                    for (auto &tgt : tgts)
                    {
                        PackageData d;
                        d.prefix = 0;
                        // add only gn atm
                        d.group_number = g[i]->getGroupNumber();
                        getLocalStorage().installLocalPackage(tgt->getPackage(), d);

                        setEntryPoint(LocalPackage(getLocalStorage(), tgt->getPackage()), ep);
                    }
                }
                continue;
            }*/
        }
    }
}

}
