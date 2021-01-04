// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <sw/builder/command_storage.h>

#include "driver.h"

#include "build.h"
#include "builtin_input.h"
#include "extensions.h"
#include "suffix.h"
#include "target/all.h"
#include "entry_point.h"
#include "module.h"
#include "frontend/cmake/cmake_fe.h"
#include "frontend/cppan/cppan.h"
#include "compiler/detect.h"
#include "compiler/set_settings.h"

#include <sw/core/build.h>
#include <sw/core/input.h>
#include <sw/core/specification.h>
#include <sw/core/sw_context.h>
#include <sw/manager/storage.h>
#include <sw/support/serialization.h>

#include <boost/algorithm/string.hpp>
#include <nlohmann/json.hpp>
#include <primitives/lock.h>
#include <primitives/yaml.h>
#include <toml.hpp>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "driver.cpp");

void process_configure_ac2(const path &p);

namespace sw
{

void addImportLibrary(Build &b);
void addDelayLoadLibrary(Build &b);
void addConfigPchLibrary(Build &b);

namespace driver::cpp
{

enum class FrontendType
{
    Unspecified,

    // priority!

    // Sw prefix means sw.EXT files, almost always EXT is a language name
    Sw = 1,
    SwC,
    SwVala,

    Cppan,
    Cmake,

    Cargo, // rust
    Dub, // d
    Composer, // php
};

static FilesOrdered findConfig(const path &dir, const FilesOrdered &fe_s)
{
    FilesOrdered files;
    FilesSorted f2;
    for (auto &fn : fe_s)
    {
        if (!fs::exists(dir / fn))
            continue;
        // on windows some exts are the same .cpp and .CPP,
        // so we check it
        if (f2.insert(fs::canonical(dir / fn)).second)
            files.push_back(dir / fn);
    }
    return files;
}

static String toString(FrontendType t)
{
    switch (t)
    {
    case FrontendType::Sw:
        return "sw";
    case FrontendType::SwC:
        return "sw.c";
    case FrontendType::SwVala:
        return "sw.vala";
    case FrontendType::Cppan:
        return "cppan";
    case FrontendType::Cmake:
        return "cmake";
    case FrontendType::Cargo:
        return "cargo";
    case FrontendType::Dub:
        return "dub";
    case FrontendType::Composer:
        return "composer";
    default:
        throw std::logic_error("not implemented");
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

struct BuiltinPackage : Package
{
    NativeBuiltinTargetEntryPoint::BuildFunction f;

    using Package::Package;
    bool isInstallable() const override { return false; }
    std::unique_ptr<Package> clone() const override { return std::make_unique<BuiltinPackage>(*this); }
    path getDirSrc2() const override { return getData().sdir; }
};

// actually this is system storage
// or storage for programs found in the system
struct BuiltinStorage : IStorage
{
    struct BuiltinLoader
    {
        bool loaded = false;
        std::optional<PackageVersionRange> all;
        std::unordered_multimap<PackageVersionRange, ProgramDetector::DetectablePackageEntryPoint> eps;
        std::unordered_map<PackageVersion, ProgramDetector::DetectablePackageEntryPoint> version_eps;

        void addPair(const PackageVersionRange &r, const ProgramDetector::DetectablePackageEntryPoint &ep)
        {
            if (!all)
                all = r;
            else
                *all |= r;
            eps.emplace(r, ep);
        }

        void load(const BuiltinStorage &bs, ResolveRequest &rr)
        {
            if (loaded)
                return;

            std::vector<std::pair<ITargetPtr, NativeBuiltinTargetEntryPoint::BuildFunction>> targets;
            for (auto &[r, ep] : eps)
            {
                Build b(*bs.sb);
                b.module_data.current_settings = rr.settings;
                ep(b);
                SW_CHECK(b.module_data.getTargets().size() <= 1); // only 1 target per build call
                if (b.module_data.getTargets().empty())
                    continue;
                version_eps.emplace(b.module_data.getTargets()[0]->getPackage().getVersion(), ep);
            }

            loaded = true;
        }

        bool resolve(const BuiltinStorage &bs, ResolveRequest &rr)
        {
            if (!all || !all->contains(rr.u.getRange()))
                return false;

            load(bs, rr);

            for (auto &&[v, ep] : version_eps)
            {
                if (!rr.u.getRange().contains(v))
                    continue;
                auto pp = bs.makePackage({ PackageName{ rr.u.getPath(), v }, rr.getSettings() });
                auto &p = (BuiltinPackage &)*pp;
                p.f = ep;
                auto d = std::make_unique<PackageData>();
                d->prefix = 0;
                //d->settings = rr.getSettings();
                p.setData(std::move(d));

                rr.setPackage(std::move(pp));
            }
            SW_CHECK(rr.isResolved());
            return true;
        }
    };

    SwContext &swctx;
    std::unique_ptr<SwBuild> sb;
    mutable std::unordered_map<PackagePath, BuiltinLoader> available_loaders;

    BuiltinStorage(SwContext &swctx)
        : swctx(swctx)
    {
        sb = swctx.createBuild(); // fake build
        auto eps = getProgramDetector().getDetectablePackages();
        for (auto &[k, v] : eps)
            available_loaders[k.getPath()].addPair(k.getRange(), v);
    }

    std::unique_ptr<Package> makePackage(const PackageId &id) const override
    {
        return std::make_unique<BuiltinPackage>(id);
    }

    bool resolve(ResolveRequest &rr) const override
    {
        auto alit = available_loaders.find(rr.u.getPath());
        if (alit == available_loaders.end())
            return false;
        return alit->second.resolve(*this, rr);
    }
};

struct ConfigPackage : Package
{
    EntryPointFunctions epfs;

    using Package::Package;
    std::unique_ptr<Package> clone() const override { return std::make_unique<ConfigPackage>(*this); }
    bool isInstallable() const override { return false; }
    path getDirSrc2() const override { return getData().sdir; }
};

struct ConfigStorage : IStorage
{
    SwContext &swctx;
    std::unordered_map<size_t, EntryPointFunctions> eps;
    std::unordered_map<UnresolvedPackage, std::pair<PackageName,size_t>> targets;

    ConfigStorage(SwContext &swctx)
        : swctx(swctx)
    {
    }

    bool resolve(ResolveRequest &rr) const override
    {
        // we try to resolve ourselves using default storage

        if (auto i = targets.find(rr.u); i != targets.end())
        {
            ResolveRequest rr2{ i->second.first, rr.settings };
            if (swctx.resolve(rr2, true))
            {
                auto &p = rr2.getPackage();
                auto pkg = makePackage(p.getId());
                auto &p2 = (ConfigPackage &)*pkg;
                p2.epfs = eps.find(i->second.second)->second;
                auto d = std::make_unique<PackageData>(p.getData());
                pkg->setData(std::move(d));
                rr.setPackageForce(std::move(pkg));
                return true;
            }
        }
        return false;
    }

    std::unique_ptr<Package> makePackage(const PackageId &id) const override
    {
        auto p = std::make_unique<ConfigPackage>(id);
        return p;
    }
};

struct DriverInput : Input
{
    std::unique_ptr<NativeTargetEntryPoint> ep;
    FrontendType fe_type = FrontendType::Unspecified;

    using Input::Input;

    bool isLoaded() const override { return !!ep; }

    void setEntryPoint(std::unique_ptr<NativeTargetEntryPoint> in)
    {
        if (isLoaded())
            throw SW_RUNTIME_ERROR("Input already loaded");
        SW_ASSERT(in, "No entry points provided");
        ep = std::move(in);
    }

    std::vector<ITargetPtr> loadPackages(SwBuild &b, const PackageSettings &s, const PackageName *known_package, const PackagePath &prefix) const override
    {
        // maybe save all targets on load?

        // 1. If we load all installed packages, we might spend a lot of time here,
        //    in case if all of the packages are installed and config is huge (aws, qt).
        //    Also this might take a lot of memory.
        //
        // 2. If we load package by package we might spend a lot of time in subsequent loads.
        //

        if (!isLoaded())
            throw SW_RUNTIME_ERROR("Input is not loaded: " + std::to_string(getHash()));

        LOG_TRACE(logger, "Loading input " << getName() << ", settings = " << s.toString());

        // are we sure that load package can return dry-run?
        // if it cannot return dry run packages, we cannot remove this wrapper
        std::vector<ITargetPtr> tgts;
        auto t = ep->loadPackages(b, s, known_package, prefix);
        for (auto &tgt : t)
        {
            if (tgt->getSettings()["dry-run"])
                SW_UNIMPLEMENTED;
            tgts.push_back(std::move(tgt));
        }
        // it is possible to get all targets dry run for some reason
        // why?
        return tgts;
    }
};

struct BuiltinInput : DriverInput
{
    size_t h;

    BuiltinInput(SwContext &swctx, const IDriver &d, size_t hash)
        : DriverInput(swctx, d, std::make_unique<Specification>(SpecificationFiles{})), h(hash)
    {}

    bool isParallelLoadable() const { return true; }
    size_t getHash() const override { return h; }
    //EntryPointPtr load1(SwContext &) override { SW_UNREACHABLE; }
};

Driver::Driver(SwContext &swctx)
    : swctx(swctx)
{
    bs = std::make_unique<BuiltinStorage>(swctx);
    cs = std::make_unique<ConfigStorage>(swctx);

    // register inputs
    for (auto &&e : load_builtin_entry_points())
    {
        cs->eps[e.hash] = e.bfs;
        for (auto &&[u,n] : e.resolver_cache)
            cs->targets.emplace(u, decltype(cs->targets)::mapped_type{ n, e.hash });
    }
}

Driver::~Driver()
{
}

void Driver::processConfigureAc(const path &p)
{
    process_configure_ac2(p);
}

PackageName Driver::getPackageId()
{
    return "org.sw."s + PACKAGE "-" PACKAGE_VERSION;
}

struct SpecFileInput : DriverInput
{
    std::unique_ptr<Module> module;

    using DriverInput::DriverInput;

    bool isBatchLoadable() const override
    {
        return 0
            || fe_type == FrontendType::Sw
            || fe_type == FrontendType::SwC
            // vala requires glib which is not in default packages, so we load it separately
            ;
    }

    // everything else is parallel loadable
    bool isParallelLoadable() const override { return !isBatchLoadable(); }

    /*EntryPointPtr load1(SwContext &swctx) override
    {
        auto fn = getSpecification().files.getData().begin()->second.absolute_path;
        switch (fe_type)
        {
        case FrontendType::Sw:
        case FrontendType::SwC:
        {
            auto out = static_cast<const Driver&>(getDriver()).build_configs1(swctx, { this }).begin()->second;
            module = loadSharedLibrary(out.dll, out.PATH, swctx.getSettings()["do_not_remove_bad_module"].get<bool>());
            auto ep = std::make_unique<NativeModuleTargetEntryPoint>(*module);
            ep->source_dir = fn.parent_path();
            return ep;
        }
        case FrontendType::SwVala:
        {
            auto b = swctx.createBuild();

            auto ts = static_cast<const Driver&>(getDriver()).getDllConfigSettings(*b);
            //ts["native"]["library"] = "shared"; // why?
            NativeTargetEntryPoint ep1;
            auto b2 = ep1.createBuild(*b, ts, {}, {});

            PrepareConfig pc;
            pc.addInput(b2, *this);

            SW_UNIMPLEMENTED;
            //auto &tgts = b2.module_data.getTargets();
            //for (auto &tgt : tgts)
                //b->getTargets()[tgt->getPackage()].push_back(tgt);

            // execute
            SW_UNIMPLEMENTED;
            //for (auto &tgt : tgts)
                //b->getTargetsToBuild()[tgt->getPackage()] = b->getTargets()[tgt->getPackage()]; // set our targets

            b->build();
            auto &out = pc.r[fn];
            module = loadSharedLibrary(out.dll, out.PATH, swctx.getSettings()["do_not_remove_bad_module"].get<bool>());
            auto ep = std::make_unique<NativeModuleTargetEntryPoint>(*module);
            ep->source_dir = fn.parent_path();
            return ep;
        }
        case FrontendType::Cppan:
        {
            auto root = YAML::Load(read_file(fn));
            auto bf = [root](Build &b) mutable
            {
                frontend::cppan::cppan_load(b, root);
            };
            auto ep = std::make_unique<NativeBuiltinTargetEntryPoint>(bf);
            ep->source_dir = fn.parent_path();
            return ep;
        }
        case FrontendType::Cmake:
        {
            auto ep = std::make_unique<CmakeTargetEntryPoint>(fn);
            return ep;
        }
        case FrontendType::Cargo:
        {
            auto root = toml::parse(to_string(normalize_path(fn)));
            auto bf = [root](Build &b) mutable
            {
                std::string name = toml::find<std::string>(root["package"], "name");
                std::string version = toml::find<std::string>(root["package"], "version");
                auto &t = b.addTarget<RustExecutable>(name, version);
                t += "src/.*"_rr;
            };
            auto ep = std::make_unique<NativeBuiltinTargetEntryPoint>(bf);
            ep->source_dir = fn.parent_path();
            return ep;
        }
        case FrontendType::Dub:
        {
            // https://dub.pm/package-format-json
            if (fn.extension() == ".sdl")
                SW_UNIMPLEMENTED;
            nlohmann::json j;
            j = nlohmann::json::parse(read_file(fn));
            auto bf = [j](Build &b) mutable
            {
                auto &t = b.addTarget<DExecutable>(j["name"].get<String>(),
                    j.contains("version") ? j["version"].get<String>() : "0.0.1"s);
                if (j.contains("sourcePaths"))
                    t += FileRegex(t.SourceDir / j["sourcePaths"].get<String>(), ".*", true);
                else if (fs::exists(t.SourceDir / "source"))
                    t += "source/.*"_rr;
                else if (fs::exists(t.SourceDir / "src"))
                    t += "src/.*"_rr;
                else
                    throw SW_RUNTIME_ERROR("No source paths found");
            };
            auto ep = std::make_unique<NativeBuiltinTargetEntryPoint>(bf);
            ep->source_dir = fn.parent_path();
            return ep;
        }
        case FrontendType::Composer:
        {
            nlohmann::json j;
            j = nlohmann::json::parse(read_file(fn));
            auto bf = [j](Build &b) mutable
            {
                SW_UNIMPLEMENTED;
            };
            auto ep = std::make_unique<NativeBuiltinTargetEntryPoint>(bf);
            ep->source_dir = fn.parent_path();
            return ep;
        }
        default:
            SW_UNIMPLEMENTED;
        }
    }*/
};

struct InlineSpecInput : DriverInput
{
    yaml root;

    using DriverInput::DriverInput;

    bool isParallelLoadable() const override { return true; }

    /*EntryPointPtr load1(SwContext &swctx) override
    {
        SW_ASSERT(fe_type == FrontendType::Cppan, "not implemented");

        auto fn = getSpecification().files.getData().begin()->second.absolute_path;
        auto p = fn;
        if (root.IsNull())
        {
            auto bf = [p](Build &b)
            {
                auto &t = b.addExecutable(p.stem().string());
                t += p;
            };
            auto ep = std::make_unique<NativeBuiltinTargetEntryPoint>(bf);
            ep->source_dir = p.parent_path();
            return ep;
        }

        auto bf = [this, p](Build &b) mutable
        {
            auto tgts = frontend::cppan::cppan_load(b, root, to_string(p.stem().u8string()));
            if (tgts.size() == 1)
                *tgts[0] += p;
        };
        auto ep = std::make_unique<NativeBuiltinTargetEntryPoint>(bf);
        ep->source_dir = p.parent_path();
        return ep;
    }*/
};

struct DirInput : DriverInput
{
    using DriverInput::DriverInput;

    bool isParallelLoadable() const override { return true; }

    /*EntryPointPtr load1(SwContext &swctx) override
    {
        auto dir = getSpecification().dir;
        auto bf = [this, dir](Build &b)
        {
            auto &t = b.addExecutable(dir.stem().string());
        };
        auto ep = std::make_unique<NativeBuiltinTargetEntryPoint>(bf);
        ep->source_dir = dir;
        return ep;
    }*/
};

void Driver::setupBuild(SwBuild &b) const
{
    // add builtin resolver
    b.getResolver().addStorage(*bs);
}

std::unique_ptr<Input> Driver::getInput(const Package &p) const
{
    // we are trying to load predefined package
    if (p.getId().getName().getPath().isRelative())
        SW_UNREACHABLE;
    else if (auto lp = dynamic_cast<const BuiltinPackage *>(&p))
    {
        auto h = std::hash<Package>()(p);
        auto i = std::make_unique<BuiltinInput>(swctx, *this, h);
        auto ep = std::make_unique<sw::NativeBuiltinTargetEntryPoint>(lp->f);
        i->setEntryPoint(std::move(ep));
        return i;
    }
    else if (auto lp = dynamic_cast<const ConfigPackage *>(&p))
    {
        auto h = std::hash<Package>()(p);
        auto i = std::make_unique<BuiltinInput>(swctx, *this, h);
        auto ep = std::make_unique<sw::NativeBuiltinTargetEntryPoint>(lp->epfs.bf);
        ep->cf = lp->epfs.cf; // copy check ep
        i->setEntryPoint(std::move(ep));
        return i;
    }
    else
    {
        std::vector<const IDriver *> d2;
        d2.push_back(this);
        auto inputs = swctx.detectInputs(d2, p.getDirSrc2());
        SW_CHECK(inputs.size() == 1);
        return std::move(inputs[0]);
    }
}

std::vector<std::unique_ptr<Input>> Driver::detectInputs(const path &p, InputType type) const
{
    std::vector<std::unique_ptr<Input>> inputs;
    switch (type)
    {
    case InputType::SpecificationFile:
    {
        auto fe = selectFrontendByFilename(p);
        if (!fe)
            break;

        SpecificationFiles files;
        files.addFile(p.filename(), p);
        //
        // TODO: make it lazy only when uploading or managing spec (spec files)
        switch (*fe)
        {
        case FrontendType::Sw:
        {
            auto f = read_file(p);
            size_t pos = 0;
            const char s[] = "#pragma sw include ";
            while (1)
            {
                pos = f.find(s, pos);
                if (pos == f.npos)
                    break;
                auto start = pos + sizeof(s) - 1;
                pos++;
                auto pos2 = f.find("\n", start);
                if (pos2 == f.npos)
                    throw SW_RUNTIME_ERROR("'#pragma sw include' ended unexpectedly");
                auto fn = f.substr(start, pos2 - start);
                if (fn.empty())
                    throw SW_RUNTIME_ERROR("Empty fn");
                if (fn[0] == '\"')
                    fn = fn.substr(1);
                if (fn.empty())
                    throw SW_RUNTIME_ERROR("Empty fn");
                if (fn.back() == '\"')
                    fn = fn.substr(0, fn.size() - 1);
                if (fn.empty())
                    throw SW_RUNTIME_ERROR("Empty fn");
                auto absfn = p.parent_path() / fn;
                if (!is_under_root(absfn, p.parent_path()))
                    throw SW_RUNTIME_ERROR("Pointing to file outside current dir");
                files.addFile(fn, absfn);
            }
        }
            break;
        }
        //
        auto spec = std::make_unique<Specification>(files);

        auto i = std::make_unique<SpecFileInput>(swctx, *this, std::move(spec));
        i->fe_type = *fe;
        LOG_TRACE(logger, "using " << toString(i->fe_type) << " frontend for input " << p);
        inputs.push_back(std::move(i));
        break;
    }
    case InputType::DirectorySpecificationFile:
    {
        auto configs = findConfig(p, getAvailableFrontendConfigFilenames());
        //if (configs.size() > 1)
            //LOG_DEBUG(logger, "Multiple configs detected. Taking only the first one (using priority).");
        for (const auto &[idx,f] : enumerate(configs))
        {
            //if (configs.size() > 1)
                //LOG_DEBUG(logger, "Input #" << idx << ": " << f);
            for (auto &i : detectInputs(f, InputType::SpecificationFile))
                inputs.push_back(std::move(i));
            break;
        }
        break;
    }
    case InputType::InlineSpecification:
    {
        auto comments = get_inline_comments(p);

        if (comments.empty())
        {
            const auto &exts = getCppSourceFileExtensions();
            if (exts.find(p.extension().string()) != exts.end() || p.extension() == ".c")
            {
                SpecificationFiles f;
                f.addFile("cppan.yml", p, String{});
                auto spec = std::make_unique<Specification>(f);

                // file has cpp extension
                auto i = std::make_unique<InlineSpecInput>(swctx, *this, std::move(spec));
                i->fe_type = FrontendType::Cppan;
                LOG_TRACE(logger, "using inline " << toString(i->fe_type) << " frontend for input " << p);
                inputs.push_back(std::move(i));

                return inputs;
            }
        }

        for (auto &c : comments)
        {
            try
            {
                SpecificationFiles f;
                f.addFile("cppan.yml", p, c);
                auto spec = std::make_unique<Specification>(f);

                auto i = std::make_unique<InlineSpecInput>(swctx, *this, std::move(spec));
                i->fe_type = FrontendType::Cppan;
                i->root = YAML::Load(c);
                LOG_TRACE(logger, "using inline " << toString(i->fe_type) << " frontend for input " << p);
                inputs.push_back(std::move(i));

                return inputs;
            }
            catch (...)
            {
            }
        }
        break;
    }
    case InputType::Directory:
    {
        auto spec = std::make_unique<Specification>(p);
        auto i = std::make_unique<DirInput>(swctx, *this, std::move(spec));
        LOG_TRACE(logger, "dir input " << p);
        inputs.push_back(std::move(i));
        break;
    }
    default:
        SW_UNREACHABLE;
    }
    return inputs;
}

void Driver::loadInputsBatch(const std::set<Input *> &inputs) const
{
    std::map<path, Input *> m;
    for (auto &i : inputs)
    {
        auto i2 = dynamic_cast<SpecFileInput *>(i);
        SW_ASSERT(i2, "Bad input type");
        m[i2->getSpecification().files.getData().begin()->second.absolute_path] = i;
    }

    for (auto &[p, out] : build_configs1(swctx, inputs))
    {
        auto i = dynamic_cast<SpecFileInput *>(m[p]);
        if (!i)
        {
            LOG_WARN(logger, "Bad input");
            continue;
        }
        i->module = loadSharedLibrary(out.dll, out.PATH, swctx.getSettings()["do_not_remove_bad_module"].get<bool>());
        auto ep = std::make_unique<NativeModuleTargetEntryPoint>(*i->module);
        ep->source_dir = p.parent_path();
        i->setEntryPoint(std::move(ep));
    }
}

std::unique_ptr<SwBuild> Driver::create_build(SwContext &swctx) const
{
    auto b = swctx.createBuild();
    return b;
}

PackageSettings Driver::getDllConfigSettings(SwBuild &b) const
{
    auto ts = b.getContext().createHostSettings();
    addSettingsAndSetConfigPrograms(b, ts);
    return ts;
}

// not thread-safe
std::unordered_map<path, PrepareConfigOutputData> Driver::build_configs1(SwContext &swctx, const std::set<Input *> &inputs) const
{
    auto cfg_storage_dir = swctx.getLocalStorage().storage_dir_tmp / "cfg" / "stamps";
    fs::create_directories(cfg_storage_dir);

    auto save_and_return = [&cfg_storage_dir, &inputs](const std::unordered_map<path, PrepareConfigOutputData> &m)
    {
        for (auto &i : inputs)
        {
            auto fn = cfg_storage_dir / std::to_string(i->getHash()) += ".bin";
            std::ofstream ofs(fn, std::ios_base::out | std::ios_base::binary);
            if (ofs)
            {
                auto files = i->getSpecification().files.getData();
                SW_CHECK(!files.empty());
                auto fn = *files.begin();

                std::unordered_map<path, PrepareConfigOutputData> m2;
                m2[fn.second.absolute_path] = m.find(fn.second.absolute_path)->second;
                boost::archive::binary_oarchive oa(ofs);
                oa << m2;
            }
        }
        return m;
    };

    // fast path
    /*{
        std::unordered_map<path, PrepareConfigOutputData> m;
        bool ok = true;
        for (auto &i : inputs)
        {
            auto fn = cfg_storage_dir / std::to_string(i->getHash()) += ".bin";
            std::ifstream ifs(fn, std::ios_base::in | std::ios_base::binary);
            if (!ifs)
            {
                ok = false;
                break; // we failed
            }
            decltype(m) m2;
            boost::archive::binary_iarchive ia(ifs);
            ia >> m2;
            m.merge(m2);
        }
        if (ignore_outdated_configs && ok)
            return m;
    }*/

    //

    auto &ctx = swctx;
    //if (!b)
    auto b = create_build(ctx);

    CachedStorage cs;
    CachingResolver r(cs);
    r.addStorage(*this->cs); // pkg storage
    r.addStorage(*bs); // builtin tools storage
    b->setResolver(r);
    //b->getResolver().addStorage(*bs);

    PrepareConfig pc;
    auto inputs_ep = [&inputs, &pc](Build &b)
    {
        for (auto &i : inputs)
            pc.addInput(b, *i);
    };

    std::pair<String, NativeBuiltinTargetEntryPoint::BuildFunction> builtin_local_pkgs[] =
    {
        {"implib"s, addImportLibrary},
        {"delay_loader"s, addDelayLoadLibrary},
        {"config_pch"s, addConfigPchLibrary},
        {"provided_inputs"s, inputs_ep},
    };
    auto ts = getDllConfigSettings(*b);
    for (auto &[name, f] : builtin_local_pkgs)
    {
        auto h = std::hash<String>()(name);
        auto i = std::make_unique<BuiltinInput>(swctx, *this, h);
        auto ep = std::make_unique<sw::NativeBuiltinTargetEntryPoint>(f);
        i->setEntryPoint(std::move(ep));
        auto [ii, _] = swctx.registerInput(std::move(i));
        //LogicalInput bi(*ii, {});
        //bi.addPackage(name + "-0.0.1"s);
        //sw::InputWithSettings is(bi);
        sw::UserInput is(*ii);
        is.addSettings(ts);
        b->addInput(is);
    }

    // prevent simultaneous cfg builds
    ScopedFileLock lk(swctx.getLocalStorage().storage_dir_tmp / "cfg" / "build");
    b->build();

    return save_and_return(pc.r);
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

        // top priority
        m.insert({ FrontendType::Sw, "sw.cpp" });
        m.insert({ FrontendType::Sw, "sw.cxx" });
        m.insert({ FrontendType::Sw, "sw.cc" });

        m.insert({ FrontendType::SwC, "sw.c" });
        m.insert({ FrontendType::SwVala, "sw.vala" });

        // cppan fe
        m.insert({ FrontendType::Cppan, "cppan.yml" });

        //
        m.insert({ FrontendType::Cmake, "CMakeLists.txt" }); // swCMakeLists.txt? CMakeLists.sw?

        // rust fe
        m.insert({ FrontendType::Cargo, "Cargo.toml" });

        // d fe
        m.insert({ FrontendType::Dub, "dub.json" });
        m.insert({ FrontendType::Dub, "dub.sdl" });

        // php
        m.insert({ FrontendType::Composer, "composer.json" });

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
