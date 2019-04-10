// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "solution.h"

namespace sw
{

/**
* \brief Main build class, controls solutions.
*/
struct SW_DRIVER_CPP_API Build : Solution
{
    struct FetchInfo
    {
        SourceDirMap sources;
    } fetch_info;

    std::optional<path> config; // current config or empty in configless mode
    //path current_dll; // current loaded dll
    // child solutions
    std::vector<Solution> solutions;
    Solution *current_solution = nullptr;
    bool configure = true;
    bool perform_checks = true;
    bool ide = false;

    Build();
    ~Build();

    TargetType getType() const override { return TargetType::Build; }

    path build(const path &fn);
    void load(const path &fn, bool configless = false);
    void build_package(const String &pkg);
    void build_packages(const StringSet &pkgs);
    void run_package(const String &pkg);
    void execute();

    bool isConfigSelected(const String &s) const;
    const Module &loadModule(const path &fn) const;

    void prepare() override;
    bool prepareStep() override;

    Generator *getGenerator() { if (generator) return generator.get(); return nullptr; }
    const Generator *getGenerator() const { if (generator) return generator.get(); return nullptr; }

    CommandExecutionPlan getExecutionPlan() const override;

    // helper
    Solution &addSolutionRaw();
    Solution &addSolution();
    Solution &addCustomSolution();

    // hide?
    path build_configs(const std::unordered_set<ExtendedPackageData> &pkgs);

private:
    bool remove_ide_explans = false;
    std::optional<const Solution *> host;
    mutable StringSet used_configs;
    std::shared_ptr<Generator> generator; // not unique - just allow us to copy builds
    bool solutions_created = false;

    std::optional<std::reference_wrapper<Solution>> addFirstSolution();
    void setupSolutionName(const path &file_or_dir);
    SharedLibraryTarget &createTarget(const Files &files);
    path getOutputModuleName(const path &p);
    const Solution *getHostSolution();
    const Solution *getHostSolution() const;

    void performChecks() override;
    FilesMap build_configs_separate(const Files &files);

    void generateBuildSystem();

    // basic frontends
    void load_dll(const path &dll, bool usedll = true);
    void load_configless(const path &file_or_dir);
    void createSolutions(const path &dll, bool usedll = true);

    // other frontends
    void cppan_load();
    void cppan_load(const path &fn);
    void cppan_load(const yaml &root, const String &root_name = {});
    bool cppan_check_config_root(const yaml &root);

public:
    static PackagePath getSelfTargetName(const Files &files);
};

}
