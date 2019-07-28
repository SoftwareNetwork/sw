// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "driver.h"

#include "build.h"
#include "entry_point.h"
#include "module.h"

#include <sw/core/input.h>
#include <sw/core/sw_context.h>

#include <primitives/sw/cl.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "driver.cpp");

static cl::opt<bool> debug_configs("debug-configs", cl::desc("Build configs in debug mode"));

namespace sw
{

namespace driver::cpp
{

std::optional<path> findConfig(const path &dir, const FilesOrdered &fe_s)
{
    for (auto &fn : fe_s)
    {
        if (fs::exists(dir / fn))
            return dir / fn;
    }
    return {};
}

String toString(FrontendType t)
{
    switch (t)
    {
    case FrontendType::Sw:
        return "sw";
    case FrontendType::Cppan:
        return "cppan";
    default:
        throw std::logic_error("not implemented");
    }
}

Driver::Driver()
{
}

Driver::~Driver()
{
}

PackageId Driver::getPackageId() const
{
    return "org.sw.sw.driver.cpp-0.3.0"s;
}

bool Driver::canLoad(const RawInput &i) const
{
    switch (i.getType())
    {
    case InputType::SpecificationFile:
    {
        auto &fes = getAvailableFrontendConfigFilenames();
        return std::find(fes.begin(), fes.end(), i.getPath().filename().u8string()) != fes.end();
    }
    case InputType::InlineSpecification:
        SW_UNIMPLEMENTED;
        break;
    case InputType::DirectorySpecificationFile:
        return !!findConfig(i.getPath(), getAvailableFrontendConfigFilenames());
    case InputType::Directory:
        SW_UNIMPLEMENTED;
        break;
    default:
        SW_UNREACHABLE;
    }
    return false;
}

Driver::EntryPontsVector Driver::load(SwContext &swctx, const std::vector<RawInput> &inputs) const
{
    PackageIdSet pkgsids;
    std::unordered_map<path, EntryPontsVector1> p_eps;
    for (auto &i : inputs)
    {
        switch (i.getType())
        {
        case InputType::InstalledPackage:
        {
            pkgsids.insert(i.getPackageId());
            break;
        }
        case InputType::DirectorySpecificationFile:
        {
            auto p = *findConfig(i.getPath(), getAvailableFrontendConfigFilenames());
            p_eps[i.getPath()] = load_spec_file(swctx, p);
            break;
        }
        default:
            SW_UNIMPLEMENTED;
        }
    }

    std::unordered_map<PackageId, EntryPontsVector1> pkg_eps;
    if (!pkgsids.empty())
        pkg_eps = load_packages(swctx, pkgsids);

    EntryPontsVector eps;
    for (auto &i : inputs)
    {
        if (i.getType() == InputType::InstalledPackage)
            eps.push_back(pkg_eps[i.getPackageId()]);
        else
            eps.push_back(p_eps[i.getPath()]);
    }
    return eps;
}

String Driver::getSpecification(const RawInput &i) const
{
    switch (i.getType())
    {
    case InputType::InstalledPackage:
    {
        SW_UNIMPLEMENTED;
        break;
    }
    case InputType::DirectorySpecificationFile:
    {
        auto p = *findConfig(i.getPath(), getAvailableFrontendConfigFilenames());
        return read_file(p);
    }
    default:
        SW_UNIMPLEMENTED;
    }
}

template <class T>
std::shared_ptr<PrepareConfigEntryPoint> Driver::build_configs1(SwContext &swctx, const T &objs) const
{
    // separate build
    auto b = swctx.createBuild();

    auto ts = swctx.getHostSettings();
    ts["native"]["library"] = "static";
    if (debug_configs)
        ts["native"]["configuration"] = "debug";

    auto ep = std::make_shared<PrepareConfigEntryPoint>(objs);
    ep->loadPackages(*b, ts, {}); // load all

    // execute
    b->getTargetsToBuild()[*ep->tgt] = b->getTargets()[*ep->tgt]; // set our main target
    b->overrideBuildState(BuildState::PackagesResolved);
    b->loadPackages();
    b->prepare();
    b->execute();

    return ep;
}

std::unordered_map<PackageId, Driver::EntryPontsVector1> Driver::load_packages(SwContext &swctx, const PackageIdSet &pkgsids) const
{
    std::unordered_set<LocalPackage> in_pkgs;
    for (auto &p : pkgsids)
        in_pkgs.emplace(swctx.getLocalStorage(), p);

    // make pkgs unique
    std::unordered_map<PackageVersionGroupNumber, LocalPackage> cfgs2;
    for (auto &p : in_pkgs)
    {
        auto &td = swctx.getTargetData();
        if (td.find(p) == td.end())
            cfgs2.emplace(p.getData().group_number, p);
    }

    std::unordered_set<LocalPackage> pkgs;
    for (auto &[gn, p] : cfgs2)
        pkgs.insert(p);

    if (pkgs.empty())
        return {};

    auto dll = build_configs1(swctx, pkgs)->out;
    std::unordered_map<PackageId, EntryPontsVector1> eps;
    for (auto &p : in_pkgs)
    {
        auto &td = swctx.getTargetData();
        if (td.find(p) != td.end())
            continue;

        auto ep = std::make_shared<NativeModuleTargetEntryPoint>(
            Module(swctx.getModuleStorage().get(dll), gn2suffix(p.getData().group_number)));
        ep->module_data.NamePrefix = p.ppath.slice(0, p.getData().prefix);
        ep->module_data.current_gn = p.getData().group_number;
        ep->module_data.known_targets = pkgsids;
        swctx.getTargetData(p).setEntryPoint(ep);
        eps[p].push_back(ep);
    }
    return eps;
}

Driver::EntryPontsVector1 Driver::load_spec_file(SwContext &swctx, const path &fn) const
{
    auto fe = selectFrontendByFilename(fn);
    if (!fe)
        throw SW_RUNTIME_ERROR("frontend was not found for file: " + normalize_path(fn));

    LOG_TRACE(logger, "using " << toString(*fe) << " frontend");
    switch (fe.value())
    {
    case FrontendType::Sw:
    {
        auto dll = build_configs1(swctx, Files{ fn })->r.begin()->second;
        return { std::make_shared<NativeModuleTargetEntryPoint>(Module(swctx.getModuleStorage().get(dll))) };
    }
        break;
    case FrontendType::Cppan:
        SW_UNIMPLEMENTED;
        //cppan_load();
        break;
    default:
        SW_UNIMPLEMENTED;
    }
}

const StringSet &Driver::getAvailableFrontendNames()
{
    static StringSet s = []
    {
        StringSet s;
        for (const auto &t : getAvailableFrontendTypes())
            s.insert(toString(t));
        return s;
    }();
    return s;
}

const std::set<FrontendType> &Driver::getAvailableFrontendTypes()
{
    static std::set<FrontendType> s = []
    {
        std::set<FrontendType> s;
        for (const auto &[k, v] : getAvailableFrontends().left)
            s.insert(k);
        return s;
    }();
    return s;
}

const Driver::AvailableFrontends &Driver::getAvailableFrontends()
{
    static AvailableFrontends m = []
    {
        AvailableFrontends m;
        auto exts = getCppSourceFileExtensions();

        // objc
        exts.erase(".m");
        exts.erase(".mm");

        // top priority
        m.insert({ FrontendType::Sw, "sw.cpp" });
        m.insert({ FrontendType::Sw, "sw.cxx" });
        m.insert({ FrontendType::Sw, "sw.cc" });

        exts.erase(".cpp");
        exts.erase(".cxx");
        exts.erase(".cc");

        // rest
        for (auto &e : exts)
            m.insert({ FrontendType::Sw, "sw" + e });

        // cppan fe
        m.insert({ FrontendType::Cppan, "cppan.yml" });

        return m;
    }();
    return m;
}

const FilesOrdered &Driver::getAvailableFrontendConfigFilenames()
{
    static FilesOrdered f = []
    {
        FilesOrdered f;
        for (auto &[k, v] : getAvailableFrontends().left)
            f.push_back(v);
        return f;
    }();
    return f;
}

bool Driver::isFrontendConfigFilename(const path &fn)
{
    return !!selectFrontendByFilename(fn);
}

std::optional<FrontendType> Driver::selectFrontendByFilename(const path &fn)
{
    auto i = getAvailableFrontends().right.find(fn.filename());
    if (i == getAvailableFrontends().right.end())
        return {};
    return i->get_left();
}

} // namespace driver::cpp

} // namespace sw
