// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "checks_storage.h"

#include <execution_plan.h>
#include <target.h>

#include <sw/builder/driver.h>

#include <boost/dll/shared_library.hpp>
#include <boost/thread/shared_mutex.hpp>

#include <type_traits>
#include <unordered_map>

namespace sw
{

using FilesMap = std::unordered_map<path, path>;

/**
* \brief Single configuration solution.
*/
struct SW_DRIVER_CPP_API Solution : TargetBase
{
    // settings
    OS HostOS;
    //OS BuildOS; // for distributed compilation

    // solution (config) specific data
    TargetMap TargetsToBuild;
    Checker Checks;
    ChecksStorage checksStorage;

    // other data
    bool silent = false;

    // target data
    TargetMap children;
    TargetMap dummy_children;

    //
    using SourceDirMapBySource = std::unordered_map<Source, path>;
    SourceDirMapBySource source_dirs_by_source;

public:
    Solution(const Solution &);
    //Solution &operator=(const Solution &);
    virtual ~Solution();

    TargetType getType() const override { return TargetType::Solution; }

    // impl
    void execute();
    void execute() const;
    virtual void prepare();
    virtual void performChecks();
    void copyChecksFrom(const Solution &s);
    void clean() const;

    Commands getCommands() const;
    StaticLibraryTarget &getImportLibrary();

    void printGraph(const path &p) const;

    // helper
    virtual Solution &addSolution() { throw std::logic_error("invalid call"); }

    // child targets
    TargetMap &getChildren() override;
    const TargetMap &getChildren() const override;
    bool exists(const PackageId &p) const override;

    //Package getKnownTarget(const PackagePath &ppath) const;
    bool isKnownTarget(const PackageId &p) const;

    // get solution dir for package
    path getSourceDir(const PackageId &p) const;
    optional<path> getSourceDir(const Source &s, const Version &v) const;

    //protected:
    PackagesIdSet knownTargets;

    virtual ExecutionPlan<builder::Command> getExecutionPlan() const;
    ExecutionPlan<builder::Command> getExecutionPlan(Commands &cmds) const;

    static path getConfigFilename() { return "sw.cpp"; }

protected:
    Solution & base_ptr;

    Solution();

private:
    void checkPrepared() const;
    Files getGeneratedDirs() const;
    void createGeneratedDirs() const;
    UnresolvedDependenciesType gatherUnresolvedDependencies() const;

    path getChecksFilename() const;
    void loadChecks();
    void saveChecks() const;

private:
    friend struct ToBuild;
};

struct SW_DRIVER_CPP_API Build : Solution, PackageScript
{
    // child solutions
    std::vector<Solution> solutions;
    bool configure = false;
    bool perform_checks = true;

    Build();
    ~Build();

    TargetType getType() const override { return TargetType::Build; }

    path build_configs(const std::unordered_set<ExtendedPackageData> &pkgs);
    FilesMap build_configs_separate(const Files &files);

    path build(const path &fn);
    void build_and_load(const path &fn);
    void build_and_run(const path &fn);
    void build_package(const String &pkg);
    void load(const path &dll);
    bool execute() override;

    void performChecks() override;
    void prepare() override;

    // helper
    Solution &addSolution() override;

protected:
    ExecutionPlan<builder::Command> getExecutionPlan() const override;
    PackageDescriptionMap getPackages() const;

private:
    path dll;

    void setSettings();
    void findCompiler();
    SharedLibraryTarget &createTarget(const Files &files);

public:
    static PackagePath getSelfTargetName(const Files &files);
};

struct SW_DRIVER_CPP_API Module
{
    template <class F, bool Required = false>
    struct LibraryCall
    {
        std::function<F> f;

        LibraryCall &operator=(std::function<F> f)
        {
            this->f = f;
            return *this;
        }

        template <class ... Args>
        void operator()(Args && ... args) const
        {
            if (f)
                f(std::forward<Args>(args)...);
            else if (Required)
                throw std::runtime_error("Required function is not present in the module");
        }
    };

    boost::dll::shared_library module;

    Module(const path &dll);
    Module(const Module &) = delete;

    // api
    LibraryCall<void(Checker &)> check;
    LibraryCall<void(Solution &)> configure;
    void build(Solution &s) const;

private:
    LibraryCall<void(Solution &), true> build_;
};

struct SW_DRIVER_CPP_API ModuleStorage
{
    std::unordered_map<path, Module> modules;
    boost::upgrade_mutex m;

    ModuleStorage() = default;
    ModuleStorage(const ModuleStorage &) = delete;

    const Module &get(const path &dll);
};

ModuleStorage &getModuleStorage(Solution &owner);

}
