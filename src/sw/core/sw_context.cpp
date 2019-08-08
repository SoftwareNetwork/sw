// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "sw_context.h"

#include "build.h"
#include "input.h"
#include "driver.h"

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "context");

namespace sw
{

void detectCompilers(SwCoreContext &s);

IDriver::~IDriver() = default;

SwCoreContext::SwCoreContext(const path &local_storage_root_dir)
    : SwBuilderContext(local_storage_root_dir)
{
    // before detection
    createHostSettings();
    detectCompilers(*this);
    // after detection
    setHostPrograms();
}

SwCoreContext::~SwCoreContext()
{
}

void SwCoreContext::createHostSettings()
{
    host_settings = toTargetSettings(getHostOs());

    auto &ts = host_settings;
    ts["native"]["configuration"] = "release";
    ts["native"]["library"] = "shared";
    ts["native"]["mt"] = "false";
}

void SwCoreContext::setHostPrograms()
{
    auto &ts = host_settings;
    ts["native"]["configuration"] = "release";
    ts["native"]["library"] = "shared";

    if (getHostOs().is(OSType::Windows))
    {
        ts["native"]["stdlib"]["c"] = "com.Microsoft.Windows.SDK.ucrt";
        ts["native"]["stdlib"]["cpp"] = "com.Microsoft.VisualStudio.VC.libcpp";
        ts["native"]["stdlib"]["kernel"] = "com.Microsoft.Windows.SDK.um";

        if (!getPredefinedTargets()["com.Microsoft.VisualStudio.VC.cl"].empty())
        {
            ts["native"]["program"]["c"] = "com.Microsoft.VisualStudio.VC.cl";
            ts["native"]["program"]["cpp"] = "com.Microsoft.VisualStudio.VC.cl";
            ts["native"]["program"]["asm"] = "com.Microsoft.VisualStudio.VC.ml";
            ts["native"]["program"]["lib"] = "com.Microsoft.VisualStudio.VC.lib";
            ts["native"]["program"]["link"] = "com.Microsoft.VisualStudio.VC.link";
        }
        // add more defaults
        else
            SW_UNIMPLEMENTED;
    }
    else if (getHostOs().is(OSType::Linux))
    {
        /*ts["native"]["stdlib"]["c"] = "com.Microsoft.Windows.SDK.ucrt";
        ts["native"]["stdlib"]["cpp"] = "com.Microsoft.VisualStudio.VC.libcpp";
        ts["native"]["stdlib"]["kernel"] = "com.Microsoft.Windows.SDK.um";*/

        auto if_add = [this](auto &s, const auto &name)
        {
            if (getPredefinedTargets()[name].empty())
                return false;
            s = name;
            return true;
        };

        //if_add(ts["native"]["program"]["c"], "org.gnu.gcc");
        if_add(ts["native"]["program"]["c"], "org.LLVM.clang");
        //if_add(ts["native"]["program"]["cpp"], "org.gnu.gpp");
        if_add(ts["native"]["program"]["cpp"], "org.LLVM.clangpp");

        if_add(ts["native"]["program"]["asm"], "org.gnu.gcc.as");
        if_add(ts["native"]["program"]["lib"], "org.gnu.binutils.ar");
        if_add(ts["native"]["program"]["link"], "org.gnu.gcc.ld");
    }
    // add more defaults
    else
        SW_UNIMPLEMENTED;
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
    drivers.insert_or_assign(pkg, std::move(driver));
}

void SwContext::executeBuild(const path &in)
{
    clearFileStorages();
    auto b = createBuild1();
    b->runSavedExecutionPlan(in);
}

Input &SwContext::addInput(const String &i)
{
    path p(i);
    if (fs::exists(p))
        return addInput(p);
    else
        return addInput(resolve(i));
}

Input &SwContext::addInput(const path &i)
{
    return addInput1(i);
}

Input &SwContext::addInput(const LocalPackage &p)
{
    auto &i = addInput1(p);
    if (i.isLoaded())
        return i;
    if (getTargetData().find(p) == getTargetData().end())
        return i;
    auto ep = getTargetData(p).getEntryPoint();
    if (ep)
        i.addEntryPoints({ ep });
    return i;
}

template <class I>
Input &SwContext::addInput1(const I &i)
{
    auto input = std::make_unique<Input>(i, *this);
    auto it = std::find_if(inputs.begin(), inputs.end(), [&i = *input](const auto &p)
    {
        return *p == i;
    });
    if (it != inputs.end())
        return **it;
    inputs.push_back(std::move(input));
    return *inputs.back();
}

void SwContext::loadEntryPoints(const std::set<Input*> &inputs, bool set_eps)
{
    std::map<IDriver *, std::vector<Input*>> active_drivers;
    for (auto &i : inputs)
    {
        if (!i->isLoaded())
            active_drivers[&i->getDriver()].push_back(i);
    }
    for (auto &[d, g] : active_drivers)
    {
        std::vector<RawInput> inputs;
        for (auto &i : g)
            inputs.push_back(*i);
        auto eps = d->createEntryPoints(*this, inputs);
        if (eps.size() != inputs.size())
            throw SW_RUNTIME_ERROR("Incorrect number of returned entry points");
        for (size_t i = 0; i < eps.size(); i++)
        {
            // when loading installed package, eps[i] may be empty
            // (ep already exists in driver)
            // so we take ep from context
            // test: sw build org.sw.demo.madler.zlib
            if (eps[i].empty())
            {
                if (inputs[i].getType() != InputType::InstalledPackage)
                    throw SW_RUNTIME_ERROR("unexpected input type");
                g[i]->addEntryPoints({ getTargetData(inputs[i].getPackageId()).getEntryPoint() });
            }
            else
                g[i]->addEntryPoints(eps[i]);

            if (!set_eps)
                continue;

            if (inputs[i].getType() != InputType::InstalledPackage)
            {
                // for non installed packages we must create entry points in sw context
                auto b = createBuild();
                auto s = getHostSettings();
                s["driver"]["dry-run"] = "true";
                for (auto &ep : eps[i])
                {
                    auto tgts = ep->loadPackages(*b, s, {});
                    for (auto &tgt  : tgts)
                        getTargetData(tgt->getPackage()).setEntryPoint(ep);
                }
                continue;
            }

            for (auto &ep : eps[i])
            {
                // for packages we must also register all other group packages
                // which are located in this config AND which are deps of this input package id
                auto m = resolve(UnresolvedPackages{ inputs[i].getPackageId() });
                auto &p = m.find(inputs[i].getPackageId())->second;
                for (auto &d : p.getData().dependencies)
                {
                    auto &p2 = m.find(d)->second;
                    if (p2.getData().group_number != p.getData().group_number)
                        continue;
                    getTargetData(p2).setEntryPoint(ep);
                }
            }
        }
    }
}

}
