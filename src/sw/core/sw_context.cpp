// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "sw_context.h"

#include "build.h"
#include "input.h"
#include "input_database.h"
#include "driver.h"

#include <sw/manager/storage.h>

#include <primitives/executor.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "context");

namespace sw
{

SwCoreContext::SwCoreContext(const path &local_storage_root_dir, bool allow_network)
    : SwManagerContext(local_storage_root_dir, allow_network)
{
    // we must increase the limits for:
    // 1) manager (unpacker)
    // 2) builder
    // does not work on gentoo (works on ubuntu, fedora)
#ifdef __APPLE__
    if (support::set_max_open_files_limit(10 * 1024) != 10 * 1024)
        LOG_ERROR(logger, "Cannot raise number of maximum opened files");
#endif

    //
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

PackageSettings SwCoreContext::createHostSettings() const
{
    return toPackageSettings(getHostOs());
}

void SwCoreContext::setHostSettings(const PackageSettings &s)
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
    for (auto &[_, d] : drivers)
        d->setupBuild(*b);
    return std::move(b);
}

SwBuild *SwContext::registerOperation(SwBuild &b)
{
    std::unique_lock lk(m);
    if (stopped)
    {
        b.stop();
        return {};
    }
    auto &v = active_operations[std::this_thread::get_id()];
    auto old = v;
    v = &b;
    return old;
}

void SwContext::stop()
{
    std::unique_lock lk(m);
    stopped = true;
    for (auto &[_, o] : active_operations)
        o->stop();
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
        throw SW_RUNTIME_ERROR("Bad file type: " + to_string(normalize_path(p)));
    }

    p = normalize_path(primitives::filesystem::canonical(p));

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

    SW_ASSERT(!inputs_local.empty(), "Inputs empty for " + to_string(normalize_path(in)));
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
