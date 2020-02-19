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

#include <sw/manager/database.h>
#include <db_inputs.h>
#include "inserts.h"
#include <sqlpp11/sqlite3/connection.h>
#include <sqlpp11/sqlite3/sqlite3.h>
#include <sqlpp11/sqlpp11.h>

namespace sw
{

struct InputDatabase : Database
{
    InputDatabase(const path &p)
        : Database(p, inputs_db_schema)
    {
    }

    void setupInput(Input &i) const
    {
        if (i.getType() == InputType::Directory)
        {
            // set hash by path
            i.setHash(std::hash<path>()(i.getPath()));
            return;
        }

        const ::db::inputs::File file{};

        auto set_input = [&]()
        {
            auto spec = i.getSpecification();
            auto h = spec->getHash();
            i.setHash(h);

            // spec may contain many files
            for (auto &[f,_] : spec->files)
            {
                auto lwt = fs::last_write_time(f);
                std::vector<uint8_t> lwtdata(sizeof(lwt));
                memcpy(lwtdata.data(), &lwt, lwtdata.size());

                (*db)(insert_into(file).set(
                    file.path = normalize_path(f),
                    file.hash = h,
                    file.lastWriteTime = lwtdata
                ));
            }
        };

        auto q = (*db)(
            select(file.fileId, file.hash, file.lastWriteTime)
            .from(file)
            .where(file.path == normalize_path(i.getPath())));
        if (q.empty())
        {
            set_input();
            return;
        }

        bool ok = true;
        auto q2 = (*db)(
            select(file.fileId, file.path, file.lastWriteTime)
            .from(file)
            .where(file.hash == q.front().hash.value()));
        for (const auto &row : q2)
        {
            if (!fs::exists(row.path.value()))
            {
                ok = false;
                break;
            }
            auto lwt = fs::last_write_time(row.path.value());
            ok &= memcmp(row.lastWriteTime.value().data(), &lwt, sizeof(lwt)) == 0;
            if (!ok)
                break;
        }
        if (ok)
            i.setHash(q.front().hash.value());
        else
        {
            // remove old first
            for (const auto &row : q2)
                (*db)(remove_from(file).where(file.fileId == row.fileId));
            set_input();
        }
    }
};

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

std::vector<Input *> SwContext::addInput(const LocalPackage &p)
{
    auto v = addInput(p.getDirSrc2());
    SW_CHECK(v.size() == 1);
    v[0]->setPackage(p);
    return v;
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

    //
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
                auto [p,inserted] = registerInput(std::move(i));
                inputs_local.push_back(p);
                if (inserted)
                    LOG_TRACE(logger, "Selecting driver " + dp.toString() + " for input " + normalize_path(inputs_local.back()->getPath()));
            }
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

    SW_ASSERT(!inputs_local.empty(), "Inputs empty for " + normalize_path(p));
    return inputs_local;
}

std::pair<Input *, bool> SwContext::registerInput(std::unique_ptr<Input> i)
{
    if (!idb)
        idb = std::make_unique<InputDatabase>(getLocalStorage().storage_dir_tmp / "db" / "inputs.db");
    idb->setupInput(*i);
    auto h = i->getHash();
    auto [it,inserted] = inputs.emplace(h, std::move(i));
    return { &*it->second, inserted };
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
            i->load(*this);
    }

    // perform batch loads
    for (auto &[d, g] : batch_inputs)
        d->loadInputsBatch(*this, g);

    // perform parallel loads
    auto &e = getExecutor();
    Futures<void> fs;
    for (auto &i : parallel_inputs)
    {
        fs.push_back(e.push([i, this]
        {
            i->load(*this);
        }));
    }
    waitAndGet(fs);
}

}
