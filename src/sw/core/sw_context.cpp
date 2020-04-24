/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "sw_context.h"

#include "build.h"
#include "input.h"
#include "input_database.h"
#include "driver.h"

#include <sw/manager/storage.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "context");

namespace sw
{

SwCoreContext::SwCoreContext(const path &local_storage_root_dir, bool allow_network)
    : SwManagerContext(local_storage_root_dir, allow_network)
{
    HostOS = getHostOS();
    host_settings = createHostSettings();

    LOG_TRACE(logger, "Host configuration: " + getHostSettings().toString());
}

SwCoreContext::~SwCoreContext()
{
}

InputDatabase &SwCoreContext::getInputDatabase()
{
    if (!idb)
        idb = std::make_unique<InputDatabase>(getLocalStorage().storage_dir_tmp / "db" / "inputs.db");
    return *idb;
}

InputDatabase &SwCoreContext::getInputDatabase() const
{
    SW_CHECK(idb);
    return *idb;
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

SwContext::SwContext(const path &local_storage_root_dir, bool allow_network)
    : SwCoreContext(local_storage_root_dir, allow_network)
{
}

SwContext::~SwContext()
{
}

std::unique_ptr<SwBuild> SwContext::createBuild1()
{
    auto b = std::make_unique<SwBuild>(*this, fs::current_path() / SW_BINARY_DIR);
    return b;
}

std::unique_ptr<SwBuild> SwContext::createBuild()
{
    auto b = createBuild1();
    b->getTargets() = getPredefinedTargets();
    return std::move(b);
}

SwBuild *SwContext::registerOperation(SwBuild *b)
{
    std::unique_lock lk(m);
    auto &v = active_operations[std::this_thread::get_id()];
    auto old = v;
    v = b;
    return old;
}

void SwContext::stop(std::thread::id id)
{
    std::unique_lock lk(m);
    if (active_operations[id])
        active_operations[id]->stop();
}

void SwContext::registerDriver(const PackageId &pkg, std::unique_ptr<IDriver> &&driver)
{
    auto [_, inserted] = drivers.insert_or_assign(pkg, std::move(driver));
    if (inserted)
        LOG_TRACE(logger, "Registering driver: " + pkg.toString());
}

void SwContext::executeBuild(const path &in)
{
    //clearFileStorages();
    auto b = createBuild1();
    b->runSavedExecutionPlan(in);
}

std::vector<std::unique_ptr<Input>> SwContext::detectInputs(const path &in) const
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

    //
    std::vector<std::unique_ptr<Input>> inputs;

    auto findDriver = [this, &p, &inputs](auto type) -> bool
    {
        for (auto &[dp, d] : drivers)
        {
            auto inpts = d->detectInputs(p, type);
            if (inpts.empty())
                continue;
            inputs = std::move(inpts);
            return true;
        }
        return false;
    };

    // spec or regular file
    if (status.type() == fs::file_type::regular)
    {
        if (!findDriver(InputType::SpecificationFile) &&
            !findDriver(InputType::InlineSpecification))
        {
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
    }
    else
    {
        if (!findDriver(InputType::DirectorySpecificationFile) &&
            !findDriver(InputType::Directory))
        {
            SW_UNIMPLEMENTED;
        }
    }

    return inputs;
}

std::vector<Input *> SwContext::addInputInternal(const path &in)
{
    auto inpts = detectInputs(in);

    std::vector<Input *> inputs_local;
    for (auto &i : inpts)
    {
        auto [p,inserted] = registerInput(std::move(i));
        inputs_local.push_back(p);
        //if (inserted)
            //LOG_TRACE(logger, "Selecting driver " << dp.toString() << " for input " << inputs_local.back()->getName());
    }

    SW_ASSERT(!inputs_local.empty(), "Inputs empty for " + normalize_path(in));
    return inputs_local;
}

std::pair<Input *, bool> SwContext::registerInput(std::unique_ptr<Input> i)
{
    auto h = i->getHash();
    auto [it,inserted] = inputs.emplace(h, std::move(i));
    return { &*it->second, inserted };
}

Input *SwContext::getInput(size_t hash) const
{
    auto it = inputs.find(hash);
    if (it == inputs.end())
        return nullptr;
    return it->second.get();
}

void SwContext::loadEntryPointsBatch(const std::set<Input *> &inputs)
{
    std::map<const IDriver *, std::set<Input*>> batch_inputs;
    std::set<Input*> parallel_inputs;

    // select inputs
    for (auto &i : inputs)
    {
        if (i->isLoaded())
            continue;
        if (i->isBatchLoadable())
            batch_inputs[&i->getDriver()].insert(i);
        else if (i->isParallelLoadable())
            parallel_inputs.insert(i);
        else
            // perform single loads
            i->load();
    }

    // perform batch loads
    for (auto &[d, g] : batch_inputs)
        d->loadInputsBatch(g);

    // perform parallel loads
    auto &e = getExecutor();
    Futures<void> fs;
    for (auto &i : parallel_inputs)
    {
        fs.push_back(e.push([i, this]
        {
            i->load();
        }));
    }
    waitAndGet(fs);
}

}
