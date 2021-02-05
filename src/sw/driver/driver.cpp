// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <sw/builder/command_storage.h>
#include <sw/builder/file_storage.h>

#include "driver.h"

#include "build.h"
#include "builtin_input.h"
#include "extensions.h"
#include "suffix.h"
#include "input.h"
#include "target/all.h"
#include "entry_point.h"
#include "module.h"
#include "frontend/cmake/cmake_fe.h"
#include "frontend/cppan/cppan.h"
#include "compiler/detect.h"
#include "compiler/set_settings.h"
#include "package.h"

#include <sw/core/build.h>
#include <sw/core/package.h>
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
    path getSourceDirectory() const override { return getData().sdir; }
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
                b.module_data.current_settings = &rr.settings;
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
    path rdir;
    path sdir;

    using Package::Package;
    std::unique_ptr<Package> clone() const override { return std::make_unique<ConfigPackage>(*this); }
    bool isInstallable() const override { return false; }
    path getSourceDirectory() const override { return sdir; }
    path getRootDirectory() const override { return rdir; }
};

struct ConfigStorage : IStorage
{
    SwContext &swctx;
    std::unordered_map<UnresolvedPackageName, std::pair<PackageName,EntryPointFunctions>> targets;

    ConfigStorage(SwContext &swctx)
        : swctx(swctx)
    {
    }

    bool resolve(ResolveRequest &rr) const override
    {
        // we try to resolve ourselves using default storage

        auto i = targets.find(rr.u);
        if (i == targets.end())
            return false;

        ResolveRequest rr2{ i->second.first, rr.settings };
        if (!swctx.resolve(rr2, true))
            return false;

        auto installed = swctx.getLocalStorage().install(rr2.getPackage());
        auto &p = installed ? *installed : rr2.getPackage();

        auto pkg = makePackage(p.getId());
        auto &p2 = (ConfigPackage &)*pkg;
        p2.epfs = i->second.second;
        p2.sdir = p.getSourceDirectory();
        p2.rdir = p.getRootDirectory();
        auto d = std::make_unique<PackageData>(p.getData());
        pkg->setData(std::move(d));
        rr.setPackageForce(std::move(pkg));
        return true;
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

    std::vector<ITargetPtr> loadPackages(SwBuild &b, Resolver &r, const PackageSettings &s) const override
    {
        if (!isLoaded())
            throw SW_RUNTIME_ERROR("Input is not loaded: " + std::to_string(getHash()));

        LOG_TRACE(logger, "Loading input " << getName() << ", settings = " << s.toString());

        // are we sure that load package can return dry-run?
        // if it cannot return dry run packages, we cannot remove this wrapper
        std::vector<ITargetPtr> tgts;
        auto t = ep->loadPackages(b, r, s);
        for (auto &tgt : t)
        {
            //if (tgt->getSettings()["dry-run"])
                //SW_UNIMPLEMENTED;
            tgts.push_back(std::move(tgt));
        }
        // it is possible to get all targets dry run for some reason
        // why?
        return tgts;
    }

    ITargetPtr loadPackage(SwBuild &b, Resolver &r, const PackageSettings &s, const Package &p) const override
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

        return ep->loadPackage(b, r, s, p);
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
    void load() override {}
};

Driver::Driver(SwContext &swctx)
    : swctx(swctx)
{
    bs = std::make_unique<BuiltinStorage>(swctx);
    cs = std::make_unique<ConfigStorage>(swctx);

    // register inputs
    for (auto &&e : load_builtin_entry_points())
    {
        for (auto &&[u,n] : e.resolver_cache)
            cs->targets.emplace(u, decltype(cs->targets)::mapped_type{ n, e.bfs });
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

    void load() override
    {
        auto fn = getSpecification().files.getData().begin()->second.absolute_path;
        switch (fe_type)
        {
        case FrontendType::Sw:
        case FrontendType::SwC:
        {
            auto out = static_cast<const Driver&>(getDriver()).build_configs1(swctx, { this }).begin()->second;
            SW_UNIMPLEMENTED;
            //module = loadSharedLibrary(out.dll, out.PATH, swctx.getSettings()["do_not_remove_bad_module"].get<bool>());
            //ep = std::make_unique<NativeModuleTargetEntryPoint>(*module);
            //ep->source_dir = fn.parent_path();
            break;
        }
        case FrontendType::SwVala:
        {
            SW_UNIMPLEMENTED;
            /*auto b = swctx.createBuild();

            auto ts = static_cast<const Driver&>(getDriver()).getDllConfigSettings(*b);
            //ts["native"]["library"] = "shared"; // why?
            NativeTargetEntryPoint ep1;
            auto b2 = ep1.createBuild(*b, ts);

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
            ep = std::make_unique<NativeModuleTargetEntryPoint>(*module);
            ep->source_dir = fn.parent_path();*/
            break;
        }
        case FrontendType::Cppan:
        {
            auto root = YAML::Load(read_file(fn));
            auto bf = [root](Build &b) mutable
            {
                frontend::cppan::cppan_load(b, root);
            };
            ep = std::make_unique<NativeBuiltinTargetEntryPoint>(bf);
            ep->source_dir = fn.parent_path();
            break;
        }
        case FrontendType::Cmake:
        {
            ep = std::make_unique<CmakeTargetEntryPoint>(fn);
            break;
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
            ep = std::make_unique<NativeBuiltinTargetEntryPoint>(bf);
            ep->source_dir = fn.parent_path();
            break;
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
            ep = std::make_unique<NativeBuiltinTargetEntryPoint>(bf);
            ep->source_dir = fn.parent_path();
            break;
        }
        case FrontendType::Composer:
        {
            nlohmann::json j;
            j = nlohmann::json::parse(read_file(fn));
            auto bf = [j](Build &b) mutable
            {
                SW_UNIMPLEMENTED;
            };
            ep = std::make_unique<NativeBuiltinTargetEntryPoint>(bf);
            ep->source_dir = fn.parent_path();
            break;
        }
        default:
            SW_UNIMPLEMENTED;
        }
    }
};

struct InlineSpecInput : DriverInput
{
    yaml root;

    using DriverInput::DriverInput;

    bool isParallelLoadable() const override { return true; }

    void load() override
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
            ep = std::make_unique<NativeBuiltinTargetEntryPoint>(bf);
            ep->source_dir = p.parent_path();
            return;
        }

        auto bf = [this, p](Build &b) mutable
        {
            auto tgts = frontend::cppan::cppan_load(b, root, to_string(p.stem().u8string()));
            if (tgts.size() == 1)
                *tgts[0] += p;
        };
        ep = std::make_unique<NativeBuiltinTargetEntryPoint>(bf);
        ep->source_dir = p.parent_path();
    }
};

struct DirInput : DriverInput
{
    using DriverInput::DriverInput;

    bool isParallelLoadable() const override { return true; }

    void load() override
    {
        auto dir = getSpecification().dir;
        auto bf = [this, dir](Build &b)
        {
            auto &t = b.addExecutable(dir.stem().string());
        };
        ep = std::make_unique<NativeBuiltinTargetEntryPoint>(bf);
        ep->source_dir = dir;
    }
};

void Driver::setupBuild(SwBuild &b) const
{
    // add builtin resolver
    //b.getResolver().addStorage(*bs);
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
    else if (!p.getId().getSettings().empty())
    {
        struct PreparedInput : Input
        {
            std::unique_ptr<Package> p;

            PreparedInput(SwContext &swctx, const IDriver &d, const Package &p)
                : Input(swctx, d, std::make_unique<Specification>(SpecificationFiles{})), p(p.clone())
            {
            }

            ITargetPtr loadPackage(SwBuild &b, Resolver &r, const PackageSettings &s, const Package &p) const override
            {
                struct BinaryTarget : ITarget
                {
                    PackagePtr p;
                    PackageSettings input_settings;
                    PackageSettings interface_settings;
                    Commands cmds;
                    std::unique_ptr<CommandStorage> cs;
                    FileStorage fs;

                    BinaryTarget(SwContext &swctx, const PackageSettings &s, const Package &p)
                        : p(p.clone()), input_settings(s)
                    {
                        auto d = p.getRootDirectory();
                        interface_settings.mergeFromString(read_file(d / "settings.json"));

                        // we do not want to load old commands for now
                        // if you remove one of the generated files, re-run the build for this package
                        // OR
                        // we must find a way to check all files, if any is missing, then run commands.
                        // but this is performed exactly by commands

                        /*cmds = loadCommands(d / "commands.bin");
                        if (!cmds.empty())
                            cs = std::make_unique<CommandStorage>((*cmds.begin())->command_storage_root);
                        for (auto &c : cmds)
                        {
                            c->setFileStorage(fs);
                            c->command_storage = cs.get();
                        }*/
                    }

                    const PackageName &getPackage() const { return p->getId().getName(); }
                    const Source &getSource() const { SW_UNIMPLEMENTED; }
                    const PackageSettings &getSettings() const { return input_settings; }
                    const PackageSettings &getInterfaceSettings() const { return interface_settings; }
                    Commands getCommands() const override { return cmds; }
                };

                return std::make_unique<BinaryTarget>(swctx, s, p);
            }

            std::vector<ITargetPtr> loadPackages(SwBuild &b, Resolver &r, const PackageSettings &s) const override
            {
                SW_UNIMPLEMENTED;
            }

            bool isLoaded() const override { return true; }
            void load() override {}
        };

        auto d = p.getRootDirectory();
        // also check output files in settings
        // also check commands?
        // it is too late to check anything here
        if (fs::exists(d / "settings.json"))
            return std::make_unique<PreparedInput>(swctx, *this, p);

        // not our deal
        throw SW_RUNTIME_ERROR("Cannot load package: " + p.getId().toString());

        // install source pkg if missing
        PackageSettings s;
        ResolveRequest rr{ p.getId().getName(), s };
        swctx.resolve(rr, true);
        swctx.getLocalStorage().install(rr.getPackage());
    }

    auto inputs = detectInputs(p.getSourceDirectory());
    SW_CHECK(inputs.size() == 1);
    return std::move(inputs[0]);
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
        SW_UNIMPLEMENTED;
        /*i->module = loadSharedLibrary(out.dll, out.PATH, swctx.getSettings()["do_not_remove_bad_module"].get<bool>());
        auto ep = std::make_unique<NativeModuleTargetEntryPoint>(*i->module);
        ep->source_dir = p.parent_path();
        i->setEntryPoint(std::move(ep));*/
    }
}

std::unique_ptr<SwBuild> Driver::create_build(SwContext &swctx) const
{
    auto b = swctx.createBuild();
    return b;
}

PackageSettings Driver::getDllConfigSettings(/*SwBuild &b*/) const
{
    auto ts = swctx.createHostSettings();
    addSettingsAndSetConfigPrograms(/*b, */ts);
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

    //CachedStorage cs;
    //CachingResolver r(cs);
    Resolver r;
    r.addStorage(*this->cs); // pkg storage
    r.addStorage(*bs); // builtin tools storage
    //b->getResolver().addStorage(*bs);

    PrepareConfig pc;
    auto inputs_ep = [&inputs, &pc](Build &b)
    {
        for (auto &i : inputs)
            pc.addInput(b, *i);
    };

    const std::pair<String, NativeBuiltinTargetEntryPoint::BuildFunction> builtin_local_pkgs[] =
    {
        {"implib"s, addImportLibrary},
        {"delay_loader"s, addDelayLoadLibrary},
        {"config_pch"s, addConfigPchLibrary},
        {"provided_inputs"s, inputs_ep},
    };
    auto ts = getDllConfigSettings();
    std::vector<std::unique_ptr<package_loader>> loaders;
    for (auto &[name, f] : builtin_local_pkgs)
    {
        auto h = std::hash<String>()(name);
        auto i = std::make_unique<BuiltinInput>(swctx, *this, h);
        auto ep = std::make_unique<sw::NativeBuiltinTargetEntryPoint>(f);
        i->setEntryPoint(std::move(ep));

        PackageId id{ name + "-0.0.1"s,{} };
        Package p{ id };

        auto b = create_build(ctx);
        //b->setResolver(r);

        i->load();

        PackageSettings s;
        s["dry-run"] = true;
        auto tgts = i->loadPackages(*b, r, s);
        SW_CHECK(tgts.size() == 1);
        for (auto &&t : tgts)
        {
            struct some_pkg : Package
            {
                using Package::Package;

                virtual std::unique_ptr<Package> clone() const { return std::make_unique<some_pkg>(*this); }
            };

            // we do not need any settings here
            PackageId id{ t->getPackage(), {} };
            auto p = std::make_unique<some_pkg>(id);
            auto d = std::make_unique<PackageData>();
            d->prefix = 0;
            p->setData(std::move(d));

            auto pp = std::make_unique<my_package_loader>(*p);
            pp->i = std::move(i);
            pp->b = std::move(b);
            pp->r = std::make_unique<Resolver>(r);
            loaders.emplace_back(std::move(pp));
        }

        /*auto [ii, _] = swctx.registerInput(std::move(i));
        //LogicalInput bi(*ii, {});
        //bi.addPackage(name + "-0.0.1"s);
        //sw::InputWithSettings is(bi);
        sw::UserInput is(*ii);
        is.addSettings(ts);
        b->addInput(is);*/
    }

    // prevent simultaneous cfg builds
    ScopedFileLock lk(swctx.getLocalStorage().storage_dir_tmp / "cfg" / "build");
    //b->build();

    std::vector<std::shared_ptr<sw::package_transform>> transforms;
    for (auto &p : loaders)
        transforms.push_back(p->load(ts));

    std::vector<const sw::package_transform*> pkg_ptr;
    for (auto &p : transforms)
        pkg_ptr.push_back(p.get());
    sw::transform_executor e;
    e.execute(pkg_ptr);

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

std::vector<std::unique_ptr<Input>> Driver::detectInputs(const path &in) const
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
        auto inpts = detectInputs(p, type);
        if (inpts.empty())
            return false;
        inputs = std::move(inpts);
        return true;
    };

    // spec or regular file
    if (status.type() == fs::file_type::regular)
    {
        if (!findDriver(InputType::SpecificationFile) &&
            !findDriver(InputType::InlineSpecification))
        {
            // nothing found, ok
            return {};

            SW_UNIMPLEMENTED;

            // find in file first: 'sw driver package-id', call that driver on whole file
            //auto f = read_file(p);

            //static const std::regex r("sw\\s+driver\\s+(\\S+)");
            //std::smatch m;
            //if (std::regex_search(f, m, r))
            //{
            //SW_UNIMPLEMENTED;

            ////- install driver
            ////- load & register it
            ////- re-run this ctor

            //auto driver_pkg = swctx.install({ m[1].str() }).find(m[1].str());
            //return;
            //}
        }
    }
    else
    {
        if (!findDriver(InputType::DirectorySpecificationFile) &&
            !findDriver(InputType::Directory))
        {
            // nothing found, ok
            return {};

            SW_UNIMPLEMENTED;
        }
    }

    return inputs;
}

std::vector<Driver::package_loader_ptr> Driver::load_packages(std::vector<std::unique_ptr<Input>> &&inputs)
{
    Resolver r;
    r.addStorage(*swctx.overridden_storage);
    r.addStorage(swctx.getLocalStorage()); // after overridden
    for (auto s : swctx.getRemoteStorages())
        r.addStorage(*s);
    r.addStorage(*bs); // builtin tools storage

    auto b = swctx.createBuild();
    //b->getResolver().addStorage(*bs);
    std::vector<package_loader_ptr> loaders;
    for (auto &&i : inputs)
    {
        i->load();

        std::shared_ptr<Input> is = std::move(i);
        PackageSettings s;
        s["dry-run"] = true;
        for (auto &&t : is->loadPackages(*b, r, s))
        {
            struct some_pkg : Package
            {
                using Package::Package;

                virtual std::unique_ptr<Package> clone() const { return std::make_unique<some_pkg>(*this); }
            };

            // we do not need any settings here
            PackageId id{ t->getPackage(), {} };
            auto p = std::make_unique<some_pkg>(id);
            auto d = std::make_unique<PackageData>();
            d->prefix = 0;
            p->setData(std::move(d));

            auto pp = std::make_unique<my_package_loader>(*p);
            pp->i = is;
            pp->b = std::move(b);
            pp->r = std::make_unique<Resolver>(r);
            loaders.emplace_back(std::move(pp));
        }
    }
    return loaders;
}

std::vector<Driver::package_loader_ptr> Driver::load_packages(const path &in)
{
    auto inputs = detectInputs(in);
    return load_packages(std::move(inputs));
}

static std::unordered_map<PackageId, Driver::package_loader_ptr> loaders1;
static std::unordered_map<PackageId, Driver::package_loader_ptr> loaders2;

Driver::package_loader_ptr Driver::load_package(const Package &p)
{
    auto lp = dynamic_cast<const ConfigPackage *>(&p);

    if (lp)
    {
        auto i = loaders1.find(p.getId());
        if (i != loaders1.end())
            return i->second;
    }
    else
    {
        auto i = loaders2.find(p.getId());
        if (i != loaders2.end())
            return i->second;
    }

    Resolver r;
    if (lp)
    {
        r.addStorage(*this->cs); // pkg storage
        r.addStorage(*bs); // builtin tools storage
    }
    else
    {
        r.addStorage(*swctx.overridden_storage);
        r.addStorage(swctx.getLocalStorage()); // after overridden
        for (auto s : swctx.getRemoteStorages())
            r.addStorage(*s);
        r.addStorage(*bs); // builtin tools storage
    }

    auto i = getInput(p);
    i->load();

    auto b = swctx.createBuild();
    //b->getResolver().addStorage(*bs);

    auto pp = std::make_shared<my_package_loader>(p);
    pp->i = std::move(i);
    pp->b = std::move(b);
    pp->r = std::make_unique<Resolver>(r);

    if (lp)
        return loaders1.emplace(p.getId(), pp).first->second;
    else
        return loaders2.emplace(p.getId(), pp).first->second;
}

} // namespace driver::cpp

} // namespace sw
