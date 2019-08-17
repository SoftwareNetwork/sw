// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "build.h"

#include "entry_point.h"
#include "frontend/cppan/yaml.h"
#include "functions.h"
#include "inserts.h"
#include "module.h"
#include "run.h"
#include "suffix.h"
#include "sw_abi_version.h"
#include "target/native.h"

#include <sw/builder/file_storage.h>
#include <sw/builder/program.h>
#include <sw/core/input.h>
#include <sw/core/sw_context.h>
#include <sw/manager/database.h>
#include <sw/manager/settings.h>
#include <sw/manager/storage.h>

#include <boost/algorithm/string.hpp>
#include <boost/range/combine.hpp>
#include <boost/thread/lock_types.hpp>
#include <primitives/emitter.h>
#include <primitives/date_time.h>
#include <primitives/executor.h>
#include <primitives/pack.h>
#include <primitives/symbol.h>
#include <primitives/templates.h>
#include <primitives/sw/settings_program_name.h>

#include <boost/dll.hpp>
#include <nlohmann/json.hpp>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "build");

bool gWithTesting;
path gIdeFastPath;
path gIdeCopyToDir;
int gNumberOfJobs = -1;

std::map<sw::PackagePath, sw::Version> gUserSelectedPackages;

namespace sw
{

static void sw_check_abi_version(int v)
{
    if (v > SW_MODULE_ABI_VERSION)
        throw SW_RUNTIME_ERROR("Module ABI (" + std::to_string(v) + ") is greater than binary ABI (" + std::to_string(SW_MODULE_ABI_VERSION) + "). Update your sw binary.");
    if (v < SW_MODULE_ABI_VERSION)
        throw SW_RUNTIME_ERROR("Module ABI (" + std::to_string(v) + ") is less than binary ABI (" + std::to_string(SW_MODULE_ABI_VERSION) + "). Update sw driver headers (or ask driver maintainer).");
}

Build::Build(SwBuild &mb)
    : checker(*this)
{
    main_build_ = &mb;
}

SwContext &Build::getContext() const
{
    return getMainBuild().getContext();
}

TargetMap &Build::getChildren()
{
    return getMainBuild().getTargets();
}

const TargetMap &Build::getChildren() const
{
    return getMainBuild().getTargets();
}

const OS &Build::getHostOs() const
{
    return getContext().HostOS;
}

path Build::getChecksDir() const
{
    return getServiceDir() / "checks";
}

void Build::setModuleData(ModuleSwappableData &d)
{
    module_data = &d;
}

ModuleSwappableData &Build::getModuleData() const
{
    if (!module_data)
        throw SW_LOGIC_ERROR("no module data was set");
    return *module_data;
}

const TargetSettings &Build::getSettings() const
{
    return getModuleData().current_settings;
}

const BuildSettings &Build::getBuildSettings() const
{
    return getModuleData().bs;
}

const TargetSettings &Build::getHostSettings() const
{
    return getModuleData().host_settings;
}

// can be used in configs to load subdir configs
// s.build->loadModule("client/sw.cpp").call<void(Solution &)>("build", s);
Module Build::loadModule(const path &p) const
{
    SW_UNIMPLEMENTED;

    auto fn2 = p;
    if (!fn2.is_absolute())
        fn2 = SourceDir / fn2;
    // driver->build_cpp_spec(swctx, p);
    //return getContext().getModuleStorage().get(dll);
}

void Build::load_inline_spec(const path &fn)
{
    load_configless(fn);
}

void Build::load_dir(const path &)
{
    SW_UNIMPLEMENTED;
}

void Build::load_configless(const path &file_or_dir)
{
    SW_UNIMPLEMENTED;
    //load_dll({}, false);

    bool dir = fs::is_directory(file_or_dir);

    Strings comments;
    if (!dir)
    {
        // for generators
        //config = file_or_dir;

        auto f = read_file(file_or_dir);

        auto b = f.find("/*");
        if (b != f.npos)
        {
            auto e = f.find("*/", b);
            if (e != f.npos)
            {
                auto s = f.substr(b + 2, e - b - 2);
                if (!s.empty())
                    comments.push_back(s);
            }
        }
    }

    /*for (auto &s : settings)
    {
        //current_settings = &s;
        if (!dir)
        {
            //exe += file_or_dir;

            std::exception_ptr eptr;
            for (auto &c : comments)
            {
                try
                {
                    auto root = YAML::Load(c);
                    cppan_load(root, file_or_dir.stem().u8string());
                    break;
                }
                catch (...)
                {
                    eptr = std::current_exception();
                }
            }
            if (eptr)
                std::rethrow_exception(eptr);

            // count non sw targets
            //if (getChildren().size() == 1)
            //{
                //if (auto nt = getChildren().begin()->second.begin()->second->as<NativeCompiledTarget>())
                    //*nt += file_or_dir;
            //}

            TargetsToBuild = getChildren();
        }
        else
        {
            auto &exe = addExecutable(ide_solution_name);
            bool read_deps_from_comments = false;

            if (!read_deps_from_comments)
            {
                SW_UNIMPLEMENTED; // and never was

                //for (auto &[p, d] : getPackageStore().resolved_packages)
                //{
                    //if (d.installed)
                        //exe += std::make_shared<Dependency>(p.toString());
                //}
            }
        }
    }*/
}

bool Build::skipTarget(TargetScope Scope) const
{
    return false;
}

bool Build::isKnownTarget(const LocalPackage &p) const
{
    return getModuleData().known_targets.empty() ||
        p.getPath().is_loc() || // used by cfg targets and checks
        getModuleData().known_targets.find(p) != getModuleData().known_targets.end();
}

path Build::getSourceDir(const LocalPackage &p) const
{
    return p.getDirSrc2();
}

std::optional<path> Build::getSourceDir(const Source &s, const Version &v) const
{
    auto s2 = s.clone();
    s2->applyVersion(v);
    auto i = source_dirs_by_source.find(s2->getHash());
    if (i != source_dirs_by_source.end())
        return i->second;
    return {};
}

path Build::getTestDir() const
{
    SW_UNIMPLEMENTED;
    //return BinaryDir / "test" / getSettings().getConfig();
}

void Build::addTest(Test &cb, const String &name)
{
    auto dir = getTestDir() / name;
    fs::remove_all(dir); // also makea condition here

    auto &c = *cb.c;
    c.name = "test: [" + name + "]";
    c.always = true;
    c.working_directory = dir;
    SW_UNIMPLEMENTED;
    //c.addPathDirectory(BinaryDir / getSettings().getConfig());
    c.out.file = dir / "stdout.txt";
    c.err.file = dir / "stderr.txt";
    tests.insert(cb.c);
}

Test Build::addTest(const ExecutableTarget &t)
{
    return addTest("test." + std::to_string(tests.size() + 1), t);
}

Test Build::addTest(const String &name, const ExecutableTarget &tgt)
{
    SW_UNIMPLEMENTED;
    /*auto c = tgt.addCommand();
    c << cmd::prog(tgt);
    Test t(c);
    addTest(t, name);
    return t;*/
}

Test Build::addTest()
{
    return addTest("test." + std::to_string(tests.size() + 1));
}

Test Build::addTest(const String &name)
{
    Test cb(getContext());
    addTest(cb, name);
    return cb;
}

}
