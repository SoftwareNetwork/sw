// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "sw_context.h"

#include <nlohmann/json.hpp>
#include <primitives/pack.h>

#include <regex>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "context");

namespace sw
{

IDriver::~IDriver() = default;

SwContext::SwContext(const path &local_storage_root_dir)
    : SwBuilderContext(local_storage_root_dir)
{
    source_dir = fs::canonical(fs::current_path());
}

SwContext::~SwContext()
{
}

void SwContext::load(const Inputs &strings)
{
    auto inputs = makeInputs(strings);
    load(inputs);
}

void SwContext::build(const Inputs &strings)
{
    auto inputs = makeInputs(strings);
    load(inputs);
    //build->execute();
    SW_UNIMPLEMENTED;
}

bool SwContext::prepareStep()
{
    bool r = false;
    for (auto &[d, g] : active_drivers)
        r |= d->prepareStep();
    return r;
}

PackageDescriptionMap SwContext::getPackages() const
{
    PackageDescriptionMap m;
    for (auto &[pkg, tgts] : getTargets())
    {
        // deps
        if (pkg.ppath.isAbsolute())
            continue;

        auto &t = *tgts.begin();
        if (!t->isReal())
            continue;

        nlohmann::json j;

        // source, version, path
        t->getSource().save(j["source"]);
        j["version"] = pkg.getVersion().toString();
        j["path"] = pkg.ppath.toString();

        auto rd = source_dir;
        /*if (!fetch_info.sources.empty())
        {
            auto src = t->getSource().clone(); // copy
            src->applyVersion(pkg.version);
            auto si = fetch_info.sources.find(src->getHash());
            if (si == fetch_info.sources.end())
                throw SW_RUNTIME_ERROR("no such source");
            rd = si->second;
        }*/
        j["root_dir"] = normalize_path(rd);

        // double check files (normalize them)
        Files files;
        for (auto &f : t->getSourceFiles())
            files.insert(f.lexically_normal());

        // we put files under SW_SDIR_NAME to keep space near it
        // e.g. for patch dir or other dirs (server provided files)
        // we might unpack to other dir, but server could push service files in neighbor dirs like gpg keys etc
        nlohmann::json jm;
        auto files_map1 = primitives::pack::prepare_files(files, rd.lexically_normal());
        for (const auto &[f1, f2] : files_map1)
        {
            nlohmann::json jf;
            jf["from"] = normalize_path(f1);
            jf["to"] = normalize_path(f2);
            j["files"].push_back(jf);
        }

        // deps
        for (auto &d : t->getDependencies())
        {
            nlohmann::json jd;
            jd["path"] = d.ppath.toString();
            jd["range"] = d.range.toString();
            j["dependencies"].push_back(jd);
        }

        auto s = j.dump();
        m[pkg] = std::make_unique<JsonPackageDescription>(s);
    }
    return m;
}

SwContext::ProcessedInputs SwContext::makeInputs(const Inputs &strings)
{
    ProcessedInputs inputs;
    for (auto &s : strings)
    {
        switch (s.index())
        {
        case 0:
            inputs.emplace(std::get<String>(s), *this);
            break;
        case 1:
            inputs.emplace(std::get<path>(s), *this);
            break;
        case 2:
            inputs.emplace(std::get<PackageId>(s), *this);
            break;
        default:
            SW_UNREACHABLE;
        }
    }
    return inputs;
}

void SwContext::load(const ProcessedInputs &inputs)
{
    for (auto &i : inputs)
        active_drivers[&i.getDriver()].insert(i);
    for (auto &[d, g] : active_drivers)
        d->load(g);
}

void SwContext::registerDriver(std::unique_ptr<IDriver> driver)
{
    drivers.insert_or_assign(driver->getPackageId(), std::move(driver));
}

Input::Input(const String &s, const SwContext &swctx)
{
    init(s, swctx);
}

Input::Input(const path &p, const SwContext &swctx)
{
    init(p, swctx);
}

Input::Input(const PackageId &p, const SwContext &swctx)
{
    init(p);
}

void Input::init(const String &s, const SwContext &swctx)
{
    path p(s);
    if (fs::exists(p))
        init(p, swctx);
    else
        init(PackageId(s));
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
    subject = path(normalize_path(p));

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

void Input::init(const PackageId &p)
{
    // create package, install, get source dir, find there spec file
    // or install driver of package ...
    SW_UNIMPLEMENTED;

    //subject = p;
    //type = InputType::PackageId;
}

path Input::getPath() const
{
    return subject;
}

}

