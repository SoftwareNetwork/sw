// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "native1.h"

namespace sw
{

struct NativeCompiledTarget;

namespace detail
{

struct PrecompiledHeader
{
    bool use_only = false;

    path header;
    path source;

    //
    path name; // base filename
    String fancy_name;
    //
    path dir;
    path obj; // obj file (msvc)
    path pdb; // pdb file (msvc)
    path pch; // file itself

    path get_base_pch_path() const
    {
        return dir / name;
    }

    void setup(const NativeCompiledTarget &, const PathOptionsType &pch_headers);
};

}

// target without linking?
//struct SW_DRIVER_CPP_API ObjectTarget : NativeTarget {};

/**
* \brief Native Executed Target is a binary target that must be built.
*/
// actually this is asm/c/cpp target
struct SW_DRIVER_CPP_API NativeCompiledTarget
    : NativeTarget
    , NativeTargetOptionsGroup
{
private:
    ASSIGN_WRAPPER(add, NativeCompiledTarget);
    ASSIGN_WRAPPER(remove, NativeCompiledTarget);

public:
    using TargetsSet = std::unordered_set<const ITarget*>;

public:
    NativeCompiledTarget(TargetBase &parent, const PackageId &);

    using TargetEvents::add;

    ASSIGN_TYPES(ApiNameType)
    void add(const ApiNameType &i);
    void remove(const ApiNameType &i);
    SW_TARGET_USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    std::optional<bool> HeaderOnly;
    std::optional<bool> AutoDetectOptions;

    String ApiName;
    StringSet ApiNames;
    bool Empty = false;
    bool ExportAllSymbols = false;
    bool ExportIfStatic = false;
    bool PackageDefinitions = false;
    bool SwDefinitions = false;
    bool StartupProject = false; // move to description? move to Generator.VS... struct? IDE struct?
    bool GenerateWindowsResource = true; // internal?
    bool NoUndefined = true;
    bool WholeArchive = false;

    // unity
    // https://cmake.org/cmake/help/latest/prop_tgt/UNITY_BUILD.html
    // maybe implement source code before and after?
    bool UnityBuild = false;
    int UnityBuildBatchSize = 8;

    //
    bool PreprocessStep = false;

    bool ImportFromBazel = false;
    StringSet BazelNames;
    String BazelTargetFunction;
    String BazelTargetName;

    // autodetected option (if not provided)
    // if any c++ files are present
    // if true, stdlib will be added
    //std::optional<bool> Cpp;
    //std::optional<bool> AddCPPLibrary;
    // enum CppLibrary {libstdc++/libc++}

    CLanguageStandard CVersion = CLanguageStandard::Unspecified;
    bool CExtensions = false;
    CPPLanguageStandard CPPVersion = CPPLanguageStandard::Unspecified;
    bool CPPExtensions = false;

    bool UseModules = false;
    // bool Framework = false; // TODO: macos framework
    //bool ObjectLibrary = false;

    //
    virtual ~NativeCompiledTarget();

    TargetType getType() const override { return target_type; }

    void init() override;
    void prepare() override;
    void prepare2() override;
    Files gatherAllFiles() const override { return NativeTargetOptionsGroup::gatherAllFiles(); }
    DependenciesType gatherDependencies() const override;
    TargetFiles getFiles() const override;

    void addPackageDefinitions(bool defs = false);
    //virtual std::shared_ptr<builder::Command> getCommand() const;
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
    //NativeLinker *getSelectedTool() const override;
    void setOutputFile() override;
    //void setOutputFilename(const path &fn);
    path getOutputDir1() const;
    void removeFile(const path &fn, bool binary_dir = false) override;
    //std::unordered_set<NativeSourceFile*> gatherSourceFiles() const;
    bool mustResolveDeps() const override { return prepare_pass == 2; }
    void setOutputDir(const path &dir);
    bool createWindowsRpath() const;

    // reconsider?
    CompilerType getCompilerType() const;
    LinkerType getLinkerType() const;

    void writeFileOnce(const path &fn, const String &content = {});
    void writeFileSafe(const path &fn, const String &content);
    void replaceInFileOnce(const path &fn, const String &from, const String &to); // deprecate?
    void patch(const path &fn, const String &from, const String &to);
    void patch(const path &fn, const String &patch_str);
    //void patch(const path &fn, const path &patch_fn) const;
    void deleteInFileOnce(const path &fn, const String &text);
    void pushFrontToFileOnce(const path &fn, const String &text);
    void pushBackToFileOnce(const path &fn, const String &text);
    void configureFile(path from, path to, ConfigureFlags flags = ConfigureFlags::Default);

    void setupCommand(builder::Command &c) const override;

    //
    bool hasCircularDependency() const;

    using TargetBase::operator=;
    using Target::operator+=;

#define STD_MACRO(x, p)            \
    void add(detail::__sw_##p##x); \
    ASSIGN_TYPES_NO_REMOVE(detail::__sw_##p##x);
#include "std.inl"
#undef STD_MACRO

    // internal data
    detail::PrecompiledHeader pch;

protected:
    //mutable NativeLinker *SelectedTool = nullptr;
    bool circular_dependency = false;
    bool IsSwConfig = false;
    bool IsSwConfigLocal = false;
    TargetType target_type = TargetType::NativeLibrary;

    bool prepareLibrary(LibraryType Type);
    void configureFile1(const path &from, const path &to, ConfigureFlags flags);
    void detectLicenseFile();

public:
    bool isHeaderOnly() const;
    bool isStaticLibrary() const override;
    bool isStaticOrHeaderOnlyLibrary() const;
    virtual bool isExecutable() const { return false; }
    TargetType getRealType() const;

protected:
    path getBinaryParentDir() const override;

private:
    CompilerType ct = CompilerType::Unspecified;
    LinkerType lt = LinkerType::Unspecified;
    bool already_built = false;
    mutable std::optional<Commands> generated_commands;
    path outputfile;
    path implibfile;
    //Commands cmds;
    Files configure_files; // needed by IDEs, move to base target later

    using ActiveDeps = std::vector<TargetDependency>;
    std::optional<ActiveDeps> active_deps;
    // only this pkg deps!
    ActiveDeps &getActiveDependencies();
    const ActiveDeps &getActiveDependencies() const;
    // deps from all subdeps too
    DependenciesType all_deps_normal;
    DependenciesType all_deps_idir_only;
    DependenciesType all_deps_llibs_only;

protected:
    Commands getCommands1() const override;

private:
    Commands getGeneratedCommands() const;
    FilesOrdered gatherRpathLinkDirectories() const;
    void processCircular(Files &objs);
    path getPatchDir(bool binary_dir) const;
    void addFileSilently(const path &);

    mutable bool interface_settings_set = false;
    const PackageSettings &getInterfaceSettings() const override;

    void createPrecompiledHeader();
public:
    bool hasOwnPrecompiledHeader() const;
private:

    //std::unique_ptr<NativeCompiler> prog_cl_cpp;
    //std::unique_ptr<NativeCompiler> prog_cl_c;
    //std::unique_ptr<NativeCompiler> prog_cl_asm;
    //std::unique_ptr<NativeLinker> prog_link;
    //std::unique_ptr<NativeLinker> prog_lib;
    bool libstdcppset = false;
    void findCompiler();
    std::unique_ptr<NativeCompiler> activateCompiler(const PackageSetting &s, const UnresolvedPackage &id, const StringSet &exts, bool extended_desc);
    std::unique_ptr<NativeLinker> activateLibrarian(LinkerType);
    std::unique_ptr<NativeLinker> activateLinker(LinkerType);
    std::unique_ptr<NativeLinker> activateLinker(const PackageSetting &s);
    std::unique_ptr<NativeLinker> activateLinker(const PackageSetting &s, const UnresolvedPackage &id, bool extended_desc);

    void prepare_pass1();
    void prepare_pass2();
    void prepare_pass3();
    void prepare_pass3_1();
    void prepare_pass3_2();
    void prepare_pass3_3();
    void prepare_pass4();
    void prepare_pass5();
    void prepare_pass6();
    void prepare_pass7();
    void prepare_pass8();
    void prepare_pass9();

    path getOutputFileName(const path &root) const override;
public:
    path getOutputFileName2(const path &subdir) const override;
private:
    path generate_rc();
};

/**
* \brief Library target that can be built as static and shared.
*/
struct SW_DRIVER_CPP_API LibraryTarget : NativeCompiledTarget
{
    using NativeCompiledTarget::NativeCompiledTarget;
    using NativeCompiledTarget::operator=;

    void init() override;
    void prepare() override;
};

/**
* \brief Executable target.
*/
struct SW_DRIVER_CPP_API ExecutableTarget : NativeCompiledTarget, PredefinedProgram
{
    using NativeCompiledTarget::NativeCompiledTarget;
    using PredefinedProgram::getProgram;

    TargetType getType() const override { return TargetType::NativeExecutable; }
    void prepare() override;

private:
    bool isExecutable() const override { return true; }
};

/**
* \brief Static only target.
*/
struct SW_DRIVER_CPP_API StaticLibraryTarget : NativeCompiledTarget
{
    using NativeCompiledTarget::NativeCompiledTarget;

    TargetType getType() const override { return TargetType::NativeStaticLibrary; }
    void prepare() override
    {
        prepareLibrary(LibraryType::Static);
        return;
    }
};

/**
* \brief Shared only target.
*/
struct SW_DRIVER_CPP_API SharedLibraryTarget : NativeCompiledTarget
{
    using NativeCompiledTarget::NativeCompiledTarget;

    TargetType getType() const override { return TargetType::NativeSharedLibrary; }
    void prepare() override
    {
        prepareLibrary(LibraryType::Shared);
        return;
    }
};

// remove?
// module target is dll target without import lib generated
/**
* \brief Module only target.
*/
//struct SW_DRIVER_CPP_API ModuleLibraryTarget : LibraryTarget {};

}
