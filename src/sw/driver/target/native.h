// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "base.h"

namespace sw
{

namespace driver
{
struct CommandBuilder;
}

namespace detail
{

#define STD_MACRO(x, p) static struct __sw_ ## p ## x {} p ## x;
#include "std.inl"
#undef STD_MACRO

}

enum class ConfigureFlags
{
    Empty = 0x0,

    AtOnly = 0x1, // @
    CopyOnly = 0x2,
    EnableUndefReplacements = 0x4,
    AddToBuild = 0x8,
    ReplaceUndefinedVariablesWithZeros = 0x10,

    Default = Empty, //AddToBuild,
};

struct PredefinedTarget : Target, PredefinedProgram
{
};

/**
* \brief Native Target is a binary target that produces binary files (probably executables).
*/
struct SW_DRIVER_CPP_API NativeTarget : Target
    //,protected NativeOptions
{
    using Target::Target;

    virtual path getOutputFile() const = 0;

    //
    virtual void setupCommand(builder::Command &c) const {}
    // move to runnable target? since we might have data only targets
    virtual void setupCommandForRun(builder::Command &c) const { setupCommand(c); } // for Launch?
};

// target without linking?
//struct SW_DRIVER_CPP_API ObjectTarget : NativeTarget {};

/**
* \brief Native Executed Target is a binary target that must be built.
*/
struct SW_DRIVER_CPP_API NativeCompiledTarget : NativeTarget,
    NativeTargetOptionsGroup
{
private:
    ASSIGN_WRAPPER(add, NativeCompiledTarget);
    //ASSIGN_WRAPPER(remove, NativeCompiledTarget);

public:
    using TargetsSet = std::unordered_set<Target*>;

    using TargetEvents::add;
    SW_TARGET_USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    std::optional<bool> HeaderOnly;
    std::optional<bool> AutoDetectOptions;
    std::shared_ptr<NativeLinker> Linker;
    std::shared_ptr<NativeLinker> Librarian;
    path InstallDirectory;
    path OutputDir; // subdir

    String ApiName;
    StringSet ApiNames;
    bool Empty = false;
    bool ExportAllSymbols = false;
    bool ExportIfStatic = false;
    bool PackageDefinitions = false;
    bool SwDefinitions = false;
    bool StartupProject = false; // move to description? move to Generator.VS... struct? IDE struct?
    bool GenerateWindowsResource = true; // internal?

    bool ImportFromBazel = false;
    StringSet BazelNames;
    String BazelTargetFunction;
    String BazelTargetName;

    CLanguageStandard CVersion = CLanguageStandard::Unspecified;
    bool CExtensions = false;
    CPPLanguageStandard CPPVersion = CPPLanguageStandard::Unspecified;
    bool CPPExtensions = false;

    bool UseModules = false;

    CompilerType ct = CompilerType::UnspecifiedCompiler;
    CompilerType getCompilerType() const { return ct; }

    //
    virtual ~NativeCompiledTarget();

    TargetType getType() const override { return TargetType::NativeLibrary; }

    bool init() override;
    bool prepare() override;
    Files gatherAllFiles() const override { return NativeTargetOptionsGroup::gatherAllFiles(); }
    DependenciesType gatherDependencies() const override { return NativeTargetOptionsGroup::gatherDependencies(); }

    void addPackageDefinitions(bool defs = false);
    std::shared_ptr<builder::Command> getCommand() const;
    //Files getGeneratedDirs() const override;
    path getOutputFile() const override;
    virtual path getImportLibrary() const;
    struct CheckSet &getChecks(const String &name);
    void setChecks(const String &name, bool check_definitions = false);
    void findSources();
    void autoDetectOptions();
    void autoDetectSources();
    void autoDetectIncludeDirectories();
    bool hasSourceFiles() const;
    Files gatherIncludeDirectories() const;
    TargetsSet gatherAllRelatedDependencies() const;
    NativeLinker *getSelectedTool() const;// override;
    //void setOutputFilename(const path &fn);
    virtual void setOutputFile();
    path getOutputDir() const;
    void removeFile(const path &fn, bool binary_dir = false) override;
    std::unordered_set<NativeSourceFile*> gatherSourceFiles() const;
    bool mustResolveDeps() const override { return prepare_pass == 2; }
    void setOutputDir(const path &dir);
    String getConfigRaw() const override;
    bool createWindowsRpath() const;

    driver::CommandBuilder addCommand() const;
    // add executed command?

    void writeFileOnce(const path &fn, const String &content = {}) const;
    void writeFileSafe(const path &fn, const String &content) const;
    void replaceInFileOnce(const path &fn, const String &from, const String &to) const; // deprecate?
    void patch(const path &fn, const String &from, const String &to) const;
    void patch(const path &fn, const String &patch_str) const;
    //void patch(const path &fn, const path &patch_fn) const;
    void deleteInFileOnce(const path &fn, const String &text) const;
    void pushFrontToFileOnce(const path &fn, const String &text) const;
    void pushBackToFileOnce(const path &fn, const String &text) const;
    void configureFile(path from, path to, ConfigureFlags flags = ConfigureFlags::Default);

    void addPrecompiledHeader(const path &h, const path &cpp = path());
    void addPrecompiledHeader(PrecompiledHeader &pch);
    NativeCompiledTarget &operator=(PrecompiledHeader &pch);

    void setupCommand(builder::Command &c) const override;

    virtual bool isStaticOnly() const { return false; }
    virtual bool isSharedOnly() const { return false; }

    //
    virtual void cppan_load_project(const yaml &root);

    //
    bool hasCircularDependency() const;

    using TargetBase::operator=;
    using TargetBase::operator+=;

#define STD_MACRO(x, p)            \
    void add(detail::__sw_##p##x); \
    ASSIGN_TYPES_NO_REMOVE(detail::__sw_##p##x);
#include "std.inl"
#undef STD_MACRO

protected:
    mutable NativeLinker *SelectedTool = nullptr;
    bool circular_dependency = false;

    Files gatherObjectFiles() const;
    Files gatherObjectFilesWithoutLibraries() const;
    TargetsSet gatherDependenciesTargets() const;
    bool prepareLibrary(LibraryType Type);
    void initLibrary(LibraryType Type);
    void configureFile1(const path &from, const path &to, ConfigureFlags flags);
    void detectLicenseFile();

private:
    bool already_built = false;
    std::map<path, path> break_gch_deps;
    mutable std::optional<Commands> generated_commands;
    path outputfile;
    Commands cmds;

    Commands getCommands1() const override;

    using Target::getOutputFileName;
    path getOutputFileName(const path &root) const;
    path getOutputFileName2(const path &subdir) const;
    Commands getGeneratedCommands() const;
    void resolvePostponedSourceFiles();
    void gatherStaticLinkLibraries(LinkLibrariesType &ll, Files &added, std::unordered_set<NativeCompiledTarget*> &targets, bool system);
    FilesOrdered gatherLinkDirectories() const;
    FilesOrdered gatherLinkLibraries() const;
    void merge1();
    void findCompiler();
    void processCircular(Files &objs);

    path getPatchDir(bool binary_dir) const;
};

/**
* \brief Library target that can be built as static and shared.
*/
struct SW_DRIVER_CPP_API LibraryTarget : NativeCompiledTarget
{
    using NativeCompiledTarget::operator=;

    bool init() override;
    path getImportLibrary() const override;

    bool prepare() override;
};

/**
* \brief Executable target.
*/
struct SW_DRIVER_CPP_API ExecutableTarget : NativeCompiledTarget, PredefinedProgram
{
    using PredefinedProgram::getProgram;

    TargetType getType() const override { return TargetType::NativeExecutable; }

    bool init() override;
    void cppan_load_project(const yaml &root) override;

    bool prepare() override;
};

/**
* \brief Static only target.
*/
struct SW_DRIVER_CPP_API StaticLibraryTarget : NativeCompiledTarget
{
    bool isStaticOnly() const override { return true; }

    bool init() override;

    TargetType getType() const override { return TargetType::NativeStaticLibrary; }
    path getImportLibrary() const override { return getOutputFile(); }

    bool prepare() override
    {
        return prepareLibrary(LibraryType::Static);
    }
};

/**
* \brief Shared only target.
*/
struct SW_DRIVER_CPP_API SharedLibraryTarget : NativeCompiledTarget
{
    bool isSharedOnly() const override { return true; }

    bool init() override;

    TargetType getType() const override { return TargetType::NativeSharedLibrary; }

    bool prepare() override
    {
        return prepareLibrary(LibraryType::Shared);
    }
};

// remove?
// module target is dll target without import lib generated
/**
* \brief Module only target.
*/
//struct SW_DRIVER_CPP_API ModuleLibraryTarget : LibraryTarget {};

}
