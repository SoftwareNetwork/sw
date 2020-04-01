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

#include "driver.h"

#include "build.h"
#include "suffix.h"
#include "target/all.h"
#include "entry_point.h"
#include "module.h"

#include <sw/core/input.h>
#include <sw/core/sw_context.h>
#include <sw/manager/storage.h>
#include <sw/support/serialization.h>

#include <boost/algorithm/string.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <nlohmann/json.hpp>
#include <primitives/lock.h>
#include <primitives/yaml.h>
#include <toml.hpp>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "driver.cpp");

bool debug_configs = true; // true for now
bool ignore_outdated_configs;

std::unordered_map<sw::PackageId, std::shared_ptr<sw::NativeBuiltinTargetEntryPoint>>
    load_builtin_entry_points();

void process_configure_ac2(const path &p);

namespace sw
{

PackageIdSet load_builtin_packages(SwContext &swctx);

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

Driver::Driver()
{
}

Driver::~Driver()
{
}

void Driver::processConfigureAc(const path &p)
{
    process_configure_ac2(p);
}

struct DriverInput
{
    FrontendType fe_type = FrontendType::Unspecified;
};

struct SpecFileInput : Input, DriverInput
{
    Driver *driver = nullptr;

    using Input::Input;

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

    std::unique_ptr<Specification> getSpecification() const override
    {
        auto spec = std::make_unique<Specification>();
        // TODO: take relative path here
        spec->addFile(getPath(), read_file(getPath()));
        return spec;
    }

    EntryPointsVector load1(SwContext &swctx) override
    {
        auto fn = getPath();
        switch (fe_type)
        {
        case FrontendType::Sw:
        case FrontendType::SwC:
        {
            auto out = driver->build_configs1(swctx, { this }).begin()->second;
            auto ep = std::make_shared<NativeModuleTargetEntryPoint>(Module(swctx.getModuleStorage().get(out.dll, out.PATH)));
            ep->source_dir = fn.parent_path();
            return { ep };
        }
        case FrontendType::SwVala:
        {
            auto b = swctx.createBuild();

            auto ts = driver->getDllConfigSettings(swctx);
            //ts["native"]["library"] = "shared"; // why?
            NativeTargetEntryPoint ep1;
            auto b2 = ep1.createBuild(*b, ts, {}, {});

            PrepareConfig pc;
            pc.addInput(b2, *this);

            auto &tgts = b2.module_data.added_targets;
            for (auto &tgt : tgts)
                b->getTargets()[tgt->getPackage()].push_back(tgt);

            // execute
            for (auto &tgt : tgts)
                b->getTargetsToBuild()[tgt->getPackage()] = b->getTargets()[tgt->getPackage()]; // set our targets

            b->build();
            auto &out = pc.r[getPath()];
            auto ep = std::make_shared<NativeModuleTargetEntryPoint>(Module(swctx.getModuleStorage().get(out.dll, out.PATH)));
            ep->source_dir = fn.parent_path();
            return { ep };
        }
        case FrontendType::Cppan:
        {
            auto root = YAML::Load(read_file(fn));
            auto bf = [root](Build &b) mutable
            {
                b.cppan_load(root);
            };
            auto ep = std::make_shared<NativeBuiltinTargetEntryPoint>(bf);
            ep->source_dir = fn.parent_path();
            return { ep };
        }
        case FrontendType::Cargo:
        {
            auto root = toml::parse(normalize_path(fn));
            auto bf = [root](Build &b) mutable
            {
                std::string name = toml::find<std::string>(root["package"], "name");
                std::string version = toml::find<std::string>(root["package"], "version");
                auto &t = b.addTarget<RustExecutable>(name, version);
                t += "src/.*"_rr;
            };
            auto ep = std::make_shared<NativeBuiltinTargetEntryPoint>(bf);
            ep->source_dir = fn.parent_path();
            return { ep };
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
            auto ep = std::make_shared<NativeBuiltinTargetEntryPoint>(bf);
            ep->source_dir = fn.parent_path();
            return { ep };
        }
        case FrontendType::Composer:
        {
            nlohmann::json j;
            j = nlohmann::json::parse(read_file(fn));
            auto bf = [j](Build &b) mutable
            {
                SW_UNIMPLEMENTED;
            };
            auto ep = std::make_shared<NativeBuiltinTargetEntryPoint>(bf);
            ep->source_dir = fn.parent_path();
            return { ep };
        }
        default:
            SW_UNIMPLEMENTED;
        }
    }

    void setEntryPoints(const EntryPointsVector &in) override { Input::setEntryPoints(in); }
};

struct InlineSpecInput : Input, DriverInput
{
    yaml root;

    using Input::Input;

    bool isBatchLoadable() const override { return false; }
    bool isParallelLoadable() const override { return !isBatchLoadable(); }

    std::unique_ptr<Specification> getSpecification() const override
    {
        SW_ASSERT(fe_type == FrontendType::Cppan, "not implemented");

        auto spec = std::make_unique<Specification>();
        String s;
        if (!root.IsNull())
            s = YAML::Dump(root);
        // TODO: mark as inline path (spec)
        // add spec type?
        spec->addFile(getPath(), s);
        return spec;
    }

    EntryPointsVector load1(SwContext &swctx) override
    {
        SW_ASSERT(fe_type == FrontendType::Cppan, "not implemented");

        auto p = getPath();
        if (root.IsNull())
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

        auto bf = [this, p](Build &b) mutable
        {
            auto tgts = b.cppan_load(root, p.stem().u8string());
            if (tgts.size() == 1)
                *tgts[0] += p;
        };
        auto ep = std::make_shared<NativeBuiltinTargetEntryPoint>(bf);
        ep->source_dir = p.parent_path();
        return { ep };
    }
};

struct DirInput : Input
{
    using Input::Input;

    bool isBatchLoadable() const override { return false; }
    bool isParallelLoadable() const override { return !isBatchLoadable(); }

    std::unique_ptr<Specification> getSpecification() const override
    {
        auto spec = std::make_unique<Specification>();
        spec->addFile(getPath(), {}); // empty
        return spec;
    }

    EntryPointsVector load1(SwContext &swctx) override
    {
        auto bf = [this](Build &b)
        {
            auto &t = b.addExecutable(getPath().stem().string());
        };
        auto ep = std::make_shared<NativeBuiltinTargetEntryPoint>(bf);
        ep->source_dir = getPath();
        return { ep };
    }
};

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

        auto i = std::make_unique<SpecFileInput>(*this, p, type);
        i->driver = (Driver*)this;
        i->fe_type = *fe;
        LOG_TRACE(logger, "using " << toString(i->fe_type) << " frontend for input " << p);
        inputs.push_back(std::move(i));
        break;
    }
    case InputType::DirectorySpecificationFile:
    {
        for (auto &f : findConfig(p, getAvailableFrontendConfigFilenames()))
        {
            for (auto &i : detectInputs(f, InputType::SpecificationFile))
                inputs.push_back(std::move(i));
        }
        break;
    }
    case InputType::InlineSpecification:
    {
        auto comments = get_inline_comments(p);

        if (comments.empty())
        {
            const auto &exts = getCppSourceFileExtensions();
            if (exts.find(p.extension().string()) != exts.end())
            {
                // file ehas cpp extension
                auto i = std::make_unique<InlineSpecInput>(*this, p, type);
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
                auto root = YAML::Load(c);

                auto i = std::make_unique<InlineSpecInput>(*this, p, type);
                i->fe_type = FrontendType::Cppan;
                i->root = root;
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
        auto i = std::make_unique<DirInput>(*this, p, type);
        LOG_TRACE(logger, "dir input " << p);
        inputs.push_back(std::move(i));
        break;
    }
    default:
        SW_UNREACHABLE;
    }
    return inputs;
}

void Driver::loadInputsBatch(SwContext &swctx, const std::set<Input *> &inputs) const
{
    std::map<path, Input *> m;
    for (auto &i : inputs)
    {
        SW_ASSERT(dynamic_cast<SpecFileInput *>(i), "Bad input type");
        m[i->getPath()] = i;
    }

    for (auto &[p, out] : build_configs1(swctx, inputs))
    {
        auto i = dynamic_cast<SpecFileInput *>(m[p]);
        if (!i)
        {
            LOG_WARN(logger, "Bad input");
            continue;
        }
        auto ep = std::make_shared<NativeModuleTargetEntryPoint>(Module(swctx.getModuleStorage().get(out.dll, out.PATH)));
        ep->source_dir = p.parent_path();
        i->setEntryPoints({ ep });
    }
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

std::unique_ptr<SwBuild> Driver::create_build(SwContext &swctx) const
{
    auto &ctx = swctx;
    auto b = ctx.createBuild();

    for (auto &[p, ep] : load_builtin_entry_points())
        b->setServiceEntryPoint(p, ep);

    // register
    for (auto &p : getBuiltinPackages(ctx))
        b->getTargets()[p];

    return std::move(b);
}

TargetSettings Driver::getDllConfigSettings(SwContext &swctx) const
{
    auto ts = swctx.createHostSettings();
    ts["native"]["library"] = "static"; // why not shared?
    //ts["native"]["mt"] = "true";
    if (debug_configs)
    {
#ifndef NDEBUG
        ts["native"]["configuration"] = "debug";
#else
        ts["native"]["configuration"] = "releasewithdebuginformation";
#endif
    }
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
                std::unordered_map<path, PrepareConfigOutputData> m2;
                m2[i->getPath()] = m.find(i->getPath())->second;
                boost::archive::binary_oarchive oa(ofs);
                oa << m2;
            }
        }
        return m;
    };

    // fast path
    {
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
    }

    //

    auto &ctx = swctx;
    //if (!b)
        auto b = create_build(ctx);

    NativeTargetEntryPoint ep;
    //                                                        load all our known targets
    auto b2 = ep.createBuild(*b, getDllConfigSettings(swctx), getBuiltinPackages(ctx), {});
    PrepareConfig pc;
    for (auto &i : inputs)
        pc.addInput(b2, *i);

    // fast path
    if (ignore_outdated_configs || !pc.isOutdated())
        return save_and_return(pc.r);

    auto &tgts = b2.module_data.added_targets;
    for (auto &tgt : tgts)
        b->getTargets()[tgt->getPackage()].push_back(tgt);

    // execute
    for (auto &tgt : tgts)
        b->getTargetsToBuild()[tgt->getPackage()] = b->getTargets()[tgt->getPackage()]; // set our targets
    b->overrideBuildState(BuildState::PackagesResolved);
    /*if (!ep->udeps.empty())
        LOG_WARN(logger, "WARNING: '#pragma sw require' is not well tested yet. Expect instability.");
    b->resolvePackages(ep->udeps);*/
    {
        // prevent simultaneous cfg builds
        ScopedFileLock lk(swctx.getLocalStorage().storage_dir_tmp / "cfg" / "build");
        b->loadPackages();
        b->prepare();
        b->execute();
    }

    for (auto &tgt : tgts)
    {
        b->getTargetsToBuild().erase(tgt->getPackage());
        b->getTargets().erase(tgt->getPackage());
    }

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
