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

bool Driver::canLoad(const Input &i) const
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

static String spec;

void Driver::load(SwBuild &b, const std::set<Input> &inputs)
{
    PackageIdSet pkgsids;
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
            spec = read_file(p);
            load_spec_file(b, p, i.getSettings());
            break;
        }
        default:
            SW_UNREACHABLE;
        }
    }

    if (!pkgsids.empty())
        load_packages(b.getContext(), pkgsids);
}

String Driver::getSpecification() const
{
    return spec;
}

template <class T>
std::shared_ptr<PrepareConfigEntryPoint> Driver::build_configs1(SwBuild &b, const T &objs)
{
    auto ts = b .getContext().getHostSettings();
    ts["native"]["library"] = "static";
    if (debug_configs)
        ts["native"]["configuration"] = "debug";

    auto ep = std::make_shared<PrepareConfigEntryPoint>(objs);
    ep->loadPackages(b, ts, {}); // load all

    return ep;
}

path Driver::build_cpp_spec(SwContext &swctx, const path &fn)
{
    // separate build
    auto mb2 = swctx.createBuild();
    auto ep = build_configs1(mb2, Files{ fn });
    // set our main target
    mb2.getTargetsToBuild()[*ep->tgt] = mb2.getTargets()[*ep->tgt];
    mb2.overrideBuildState(BuildState::PackagesResolved);
    mb2.loadPackages();
    mb2.prepare();
    mb2.execute();
    return ep->r.begin()->second;
}

void Driver::load_packages(SwContext &swctx, const PackageIdSet &pkgsids)
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
        return;

    path dll;
    {
        auto mb2 = swctx.createBuild();
        auto ep = build_configs1(mb2, pkgs);
        // set our main target
        mb2.getTargetsToBuild()[*ep->tgt] = mb2.getTargets()[*ep->tgt];
        mb2.overrideBuildState(BuildState::PackagesResolved);
        mb2.loadPackages();
        mb2.prepare();
        mb2.execute();
        dll = ep->out;
    }

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
        swctx.getTargetData(p).setEntryPoint(std::move(ep));
    }
}

void Driver::load_spec_file(SwBuild &b, const path &fn, const std::set<TargetSettings> &settings)
{
    auto fe = selectFrontendByFilename(fn);
    if (!fe)
        throw SW_RUNTIME_ERROR("frontend was not found for file: " + normalize_path(fn));

    LOG_TRACE(logger, "using " << toString(*fe) << " frontend");
    switch (fe.value())
    {
    case FrontendType::Sw:
    {
        auto dll = build_cpp_spec(b.getContext(), fn);
        load_dll(b, dll, settings);
    }
        break;
    case FrontendType::Cppan:
        SW_UNIMPLEMENTED;
        //cppan_load();
        break;
    }
}

void Driver::load_dll(SwBuild &b, const path &dll, const std::set<TargetSettings> &settings)
{
    auto ep = std::make_shared<NativeModuleTargetEntryPoint>(b.getContext().getModuleStorage().get(dll));

    // find difference to set entry points
    auto old = b.getTargets();

    // load all
    for (auto &s : settings)
        ep->loadPackages(b, s, {});

    // don't forget to set EPs for loaded targets
    for (const auto &[pkg, tgts] : b.getTargets())
    {
        if (old.find(pkg) != old.end())
            continue;
        b.getContext().getTargetData(pkg).setEntryPoint(ep);
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
