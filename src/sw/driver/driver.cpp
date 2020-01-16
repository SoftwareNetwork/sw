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
#include <sw/manager/storage.h>

#include <boost/algorithm/string.hpp>
#include <primitives/sw/cl.h>
#include <primitives/yaml.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "driver.cpp");

static cl::opt<bool> debug_configs("debug-configs", cl::desc("Build configs in debug mode"));

std::unordered_map<sw::PackageId, std::shared_ptr<sw::NativeBuiltinTargetEntryPoint>>
    load_builtin_entry_points();

namespace sw
{

PackageIdSet load_builtin_packages(SwContext &swctx);

String gn2suffix(PackageVersionGroupNumber gn)
{
    return "_" + (gn > 0 ? std::to_string(gn) : ("_" + std::to_string(-gn)));
}

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

std::optional<path> Driver::canLoadInput(const RawInput &i) const
{
    switch (i.getType())
    {
    case InputType::SpecificationFile:
    {
        auto &fes = getAvailableFrontendConfigFilenames();
        auto it = std::find(fes.begin(), fes.end(), i.getPath().filename());
        if (it != fes.end())
        {
            return i.getPath();
        }
        // or check by extension
        /*it = std::find_if(fes.begin(), fes.end(), [e = i.getPath().extension()](const auto &fe)
        {
            return fe.extension() == e;
        });
        if (it != fes.end())
        {
            return i.getPath();
        }*/
        break;
    }
    case InputType::DirectorySpecificationFile:
    {
        if (auto p = findConfig(i.getPath(), getAvailableFrontendConfigFilenames()))
        {
            return *p;
        }
        break;
    }
    case InputType::InlineSpecification:
        if (can_load_configless_file(i.getPath()))
            return i.getPath();
        break;
    case InputType::Directory:
        return i.getPath();
    default:
        SW_UNREACHABLE;
    }
    return {};
}

Driver::EntryPointsVector Driver::createEntryPoints(SwContext &swctx, const std::vector<RawInput> &inputs) const
{
    PackageIdSet pkgsids;
    std::unordered_map<path, EntryPointsVector1> p_eps;
    for (auto &i : inputs)
    {
        switch (i.getType())
        {
        case InputType::InstalledPackage:
        {
            pkgsids.insert(i.getPackageId());
            break;
        }
        case InputType::SpecificationFile:
        {
            p_eps[i.getPath()] = load_spec_file(swctx, i.getPath());
            break;
        }
        case InputType::InlineSpecification:
        {
            p_eps[i.getPath()] = load_configless_file(swctx, i.getPath());
            break;
        }
        case InputType::Directory:
        {
            p_eps[i.getPath()] = load_configless_dir(swctx, i.getPath());
            break;
        }
        default:
            SW_UNIMPLEMENTED;
        }
    }

    std::unordered_map<PackageId, EntryPointsVector1> pkg_eps;
    if (!pkgsids.empty())
        pkg_eps = load_packages(swctx, pkgsids);

    EntryPointsVector eps;
    for (auto &i : inputs)
    {
        if (i.getType() == InputType::InstalledPackage)
            eps.push_back(pkg_eps[i.getPackageId()]);
        else
            eps.push_back(p_eps[i.getPath()]);
    }
    return eps;
}

std::unique_ptr<Specification> Driver::getSpecification(const RawInput &i) const
{
    auto spec = std::make_unique<Specification>();
    switch (i.getType())
    {
    case InputType::SpecificationFile:
    {
        // TODO: take relative path here
        spec->addFile(i.getPath(), read_file(i.getPath()));
        break;
    }
    case InputType::InlineSpecification:
    {
        auto s = load_configless_file_spec(i.getPath());
        if (!s)
            throw SW_RUNTIME_ERROR("Cannot load inline specification");
        // TODO: mark as inline path (spec)
        // add spec type?
        spec->addFile(i.getPath(), *s);
    }
    case InputType::Directory:
    {
        spec->addFile(i.getPath(), {}); // empty
        break;
    }
    default:
        SW_UNIMPLEMENTED;
    }
    return spec;
}

PackageIdSet Driver::getBuiltinPackages(SwContext &swctx) const
{
    if (!builtin_packages)
    {
        std::unique_lock lk(m_bp);
        builtin_packages = load_builtin_packages(swctx);
    }
    return *builtin_packages;
}

template <class T>
std::shared_ptr<PrepareConfigEntryPoint> Driver::build_configs1(SwContext &swctx, const T &objs) const
{
    auto &ctx = swctx;
    auto b = ctx.createBuild();

    auto ts = ctx.getHostSettings();
    ts["native"]["library"] = "static";
    //ts["native"]["mt"] = "true";
    if (debug_configs)
        ts["native"]["configuration"] = "debug";

    for (auto &[p, ep] : load_builtin_entry_points())
        b->setServiceEntryPoint(p, ep);

    // before load packages!
    for (auto &p : getBuiltinPackages(ctx))
        b->addKnownPackage(p);

    auto ep = std::make_shared<PrepareConfigEntryPoint>(objs);
    auto tgts = ep->loadPackages(*b, ts, b->getKnownPackages(), {}); // load all our known targets
    if (tgts.size() != 1)
        throw SW_LOGIC_ERROR("something went wrong, only one lib target must be exported");

    // fast path
    if (!ep->isOutdated())
        return ep;

    for (auto &tgt : tgts)
        b->getTargets()[tgt->getPackage()].push_back(tgt);

    // execute
    b->getTargetsToBuild()[*ep->tgt] = b->getTargets()[*ep->tgt]; // set our main target
    b->overrideBuildState(BuildState::PackagesResolved);
    b->loadPackages();
    b->prepare();
    b->execute();

    return ep;
}

std::unordered_map<PackageId, Driver::EntryPointsVector1> Driver::load_packages(SwContext &swctx, const PackageIdSet &pkgsids) const
{
    // init here
    getBuiltinPackages(swctx);

    std::unordered_map<PackageId, EntryPointsVector1> eps;

    std::unordered_set<LocalPackage> in_pkgs;
    for (auto &p : pkgsids)
        in_pkgs.emplace(swctx.getLocalStorage(), p);

    // make pkgs unique
    std::unordered_map<PackageVersionGroupNumber, LocalPackage> cfgs2;
    for (auto &p : in_pkgs)
    {
        auto ep = swctx.getEntryPoint(p);
        if (!ep)
            cfgs2.emplace(p.getData().group_number, p);
        else
            eps[p].push_back(ep);

        /*auto &td = swctx.getTargetData();
        if (td.find(p) == td.end())
            cfgs2.emplace(p.getData().group_number, p);*/
    }

    std::unordered_set<LocalPackage> pkgs;
    for (auto &[gn, p] : cfgs2)
        pkgs.insert(p);

    if (pkgs.empty())
        return eps;

    auto dll = build_configs1(swctx, pkgs)->out;
    for (auto &p : in_pkgs)
    {
        if (auto ep = swctx.getEntryPoint(p))
        {
            eps[p].push_back(ep);
            continue;
        }
        auto &td = swctx.getTargetData();
        if (td.find(p) != td.end())
            continue;

        auto ep = std::make_shared<NativeModuleTargetEntryPoint>(
            Module(swctx.getModuleStorage().get(dll), gn2suffix(p.getData().group_number)));
        //ep->module_data.NamePrefix = p.getPath().slice(0, p.getData().prefix);
        swctx.setEntryPoint(p, ep);
        eps[p].push_back(ep);
    }
    return eps;
}

Driver::EntryPointsVector1 Driver::load_spec_file(SwContext &swctx, const path &fn) const
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
        auto ep = std::make_shared<NativeModuleTargetEntryPoint>(Module(swctx.getModuleStorage().get(dll)));
        ep->source_dir = fn.parent_path();
        return { ep };
    }
        break;
    case FrontendType::Cppan:
    {
        auto root = YAML::Load(read_file(fn));
        auto bf = [root](Build &b) mutable
        {
            b.cppan_load(root);
        };
        auto ep = std::make_shared<NativeBuiltinTargetEntryPoint>(bf);
        return { ep };
    }
        break;
    default:
        SW_UNIMPLEMENTED;
    }
}

static Strings get_inline_comments(const path &p)
{
    auto f = read_file(p);

    Strings comments;
    auto b = f.find("/*");
    if (b != f.npos)
    {
        auto e = f.find("*/", b);
        if (e != f.npos)
        {
            auto s = f.substr(b + 2, e - b - 2);
            boost::trim(s);
            if (!s.empty())
                comments.push_back(s);
        }
    }
    return comments;
}

Driver::EntryPointsVector1 Driver::load_configless_dir(SwContext &, const path &p) const
{
    auto bf = [p](Build &b)
    {
        auto &t = b.addExecutable(p.stem().string());
    };
    auto ep = std::make_shared<NativeBuiltinTargetEntryPoint>(bf);
    ep->source_dir = p;
    return { ep };
}

Driver::EntryPointsVector1 Driver::load_configless_file(SwContext &, const path &p) const
{
    auto comments = get_inline_comments(p);

    if (comments.empty())
    {
        auto bf = [p](Build &b)
        {
            auto &t = b.addExecutable(p.stem().string());
            t += p;
        };
        auto ep = std::make_shared<NativeBuiltinTargetEntryPoint>(bf);
        ep->source_dir = p.parent_path();
        return { ep };
    }

    for (auto &c : comments)
    {
        try
        {
            auto root = YAML::Load(c);
            auto bf = [root, name = p.stem().u8string()](Build &b) mutable
            {
                b.cppan_load(root, name);
            };
            auto ep = std::make_shared<NativeBuiltinTargetEntryPoint>(bf);
            ep->source_dir = p.parent_path();
            return { ep };
        }
        catch (...)
        {
        }
    }
    throw SW_RUNTIME_ERROR("cannot load yaml comments");
}

bool Driver::can_load_configless_file(const path &p) const
{
    auto comments = get_inline_comments(p);

    if (comments.empty())
    {
        const auto &exts = getCppSourceFileExtensions();
        return exts.find(p.extension().string()) != exts.end();
    }

    for (auto &c : comments)
    {
        try
        {
            YAML::Load(c);
            return true;
        }
        catch (...)
        {
        }
    }
    return false;
}

std::optional<String> Driver::load_configless_file_spec(const path &p) const
{
    auto comments = get_inline_comments(p);

    if (comments.empty())
    {
        return String{};
    }

    for (auto &c : comments)
    {
        try
        {
            YAML::Load(c);
            return c;
        }
        catch (...)
        {
        }
    }
    return {};
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
    if (i != getAvailableFrontends().right.end())
        return i->get_left();
    // or check by extension
    /*i = std::find_if(getAvailableFrontends().right.begin(), getAvailableFrontends().right.end(), [e = fn.extension()](const auto &fe)
    {
        return fe.first.extension() == e;
    });
    if (i != getAvailableFrontends().right.end())
        return i->get_left();*/
    return {};
}

} // namespace driver::cpp

} // namespace sw
