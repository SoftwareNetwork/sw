// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <language_storage.h>
#include <license.h>
#include <node.h>
#include <os.h>
#include <package.h>
#include <package_path.h>
#include <source.h>
#include <source_file.h>
#include <types.h>

//#include <nlohmann/json_fwd.hpp>
#include <nlohmann/json.hpp>

#include <any>
#include <mutex>
#include <optional>

#define IMPORT_LIBRARY "sw.dll"

#define ASSIGN_WRAPPER(f, t)          \
    struct f##_files : Assigner       \
    {                                 \
        t &r;                         \
                                      \
        f##_files(t &r) : r(r)        \
        {                             \
        }                             \
                                      \
        using Assigner::operator();   \
                                      \
        template <class U>            \
        void operator()(const U &v)   \
        {                             \
            if (!canProceed(r))       \
                return;               \
            r.f(v);                   \
        }                             \
    }

#define ASSIGN_OP(op, f, t)                                   \
    stream_list_inserter<f##_files> operator op(const t &v)   \
    {                                                         \
        auto x = make_stream_list_inserter(f##_files(*this)); \
        x(v);                                                 \
        return x;                                             \
    }

#define ASSIGN_OP_ACTION(op, f, t, a)                         \
    stream_list_inserter<f##_files> operator op(const t &v)   \
    {                                                         \
        a;                                                    \
        auto x = make_stream_list_inserter(f##_files(*this)); \
        x(v);                                                 \
        return x;                                             \
    }

#define ASSIGN_TYPES_NO_REMOVE(t)        \
    ASSIGN_OP(+=, add, t)                \
    ASSIGN_OP_ACTION(=, add, t, clear()) \
    ASSIGN_OP(<<, add, t)

#define ASSIGN_TYPES(t)       \
    ASSIGN_TYPES_NO_REMOVE(t) \
    ASSIGN_OP(-=, remove, t)  \
    ASSIGN_OP(>>, remove, t)

#define ASSIGN_TYPES_AND_EXCLUDE(t) \
    ASSIGN_TYPES(t)                 \
    ASSIGN_OP(^=, remove_exclude, t)

#define USING_ASSIGN_OPS(t) \
    using t::operator+=;    \
    using t::operator-=;    \
    using t::operator=;     \
    using t::operator<<;    \
    using t::operator>>;    \
    using t::add;           \
    using t::remove

namespace sw
{

struct Solution;

namespace driver::cpp
{

struct CommandBuilder;

}

// ? (re)move?
enum class TargetScope
{
    Analyze,
    Benchmark,
    Build,
    Coverage,
    Documentation,
    Example,
    Format,
    Helper, // same as tool?
    Profile,
    Sanitize,
    Tool,
    Test,
    UnitTest,
    Valgrind,
};

enum class CallbackType
{
    CreateTarget,
    CreateTargetInitialized,
    BeginPrepare,
    EndPrepare,

//    std::vector<TargetEvent> PreBuild;
// addCustomCommand()?
// preBuild?
// postBuild?
// postLink?
};

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

// passed (serialized) via strings
enum class TargetType : int32_t
{
    Unspecified,

    Build,
    Solution,

    Project, // explicitly created
    Directory, // implicitly created?

    NativeLibrary,
    NativeStaticLibrary,
    NativeSharedLibrary,
    NativeExecutable,

    CSharpLibrary,
    CSharpExecutable,

    RustLibrary,
    RustExecutable,

    GoLibrary,
    GoExecutable,

    FortranLibrary,
    FortranExecutable,

    // add/replace with java jar?
    JavaLibrary,
    JavaExecutable,

    // add/replace with java jar?
    KotlinLibrary,
    KotlinExecutable,

    DLibrary,
    DStaticLibrary,
    DSharedLibrary,
    DExecutable,
};

// enforcement rules apply to target to say how many checks it should perform
enum class EnforcementType
{
    ChechFiles,
    CheckRegexes,
};

SW_DRIVER_CPP_API
String toString(TargetType T);

struct NativeExecutedTarget;
struct Solution;
struct Target;
struct Project;
struct Directory;
using TargetBaseType = Target;
using TargetBaseTypePtr = std::shared_ptr<TargetBaseType>;

struct ExecutableTarget;
struct LibraryTarget;
struct StaticLibraryTarget;
struct SharedLibraryTarget;

using Executable = ExecutableTarget;
using Library = LibraryTarget;
using StaticLibrary = StaticLibraryTarget;
using SharedLibrary = SharedLibraryTarget;

struct SW_DRIVER_CPP_API TargetBase : Node, LanguageStorage, ProjectDirectories
{
    using TargetMap = PackageVersionMapBase<TargetBaseTypePtr, std::unordered_map, std::map>;

    // rename? or keep available via api
    PackageId pkg;

    /**
    * \brief Target Source.
    */
    // hide?
    Source source;

    /**
    * \brief New root directory after downloading and unpacking.
    */
    path UnpackDirectory;

    /**
    * \brief Data storage for objects that must be alive with the target.
    */
    std::vector<std::any> Storage;

    /**
    * \brief Target scope.
    */
    TargetScope Scope = TargetScope::Build;

    // flags
    /// local projects, not fetched
    bool Local = true;
    bool UseStorageBinaryDir = false;
    bool PostponeFileResolving = false;
    bool IsConfig = false;
    bool ParallelSourceDownload = true;
    bool DryRun = false;

    PackagePath NamePrefix;
    const Solution *solution = nullptr;

public:
    TargetBase() = default;
    virtual ~TargetBase();

    /**
    * \brief Add child target.
    */
    void add(const TargetBaseTypePtr &t);

    template <typename T, typename ... Args>
    T& add(const PackagePath &Name, Args && ... args)
    {
        if constexpr (sizeof...(Args) > 0)
        {
            if constexpr (std::is_convertible_v<std::tuple_element_t<0, std::tuple<Args...>>, Version>)
                return addTarget1<T>(Name, std::forward<Args>(args)...);
            else
                return addTarget1<T>(Name, pkg.version, std::forward<Args>(args)...);
        }
        else
            return addTarget1<T>(Name, pkg.version, std::forward<Args>(args)...);
    }

    template <typename T, typename ... Args>
    T &addTarget(Args && ... args)
    {
        return add<T>(std::forward<Args>(args)...);
    }

#define ADD_TARGET(t)                               \
    template <typename... Args>                     \
    t &add##t(Args &&... args)                      \
    {                                               \
        return add<t>(std::forward<Args>(args)...); \
    }

    ADD_TARGET(Executable)
    ADD_TARGET(Library)
    ADD_TARGET(StaticLibrary)
    ADD_TARGET(SharedLibrary)

#undef ADD_TARGET

    template <typename T = Target>
    T &getTarget(const PackagePath &Name)
    {
        auto i = getChildren().find(Name);
        if (i == getChildren().end(Name))
        {
            i = getChildren().find(pkg.ppath / Name);
            if (i == getChildren().end(Name))
                throw SW_RUNTIME_ERROR("No such target: " + Name.toString() + " or " + (pkg.ppath / Name).toString());
        }
        if (i->second.size() > 1)
            throw SW_RUNTIME_ERROR("Target: " + i->first.toString() + " has more than one version");
        return (T&)*i->second.begin()->second;
    }

    template <typename T = Target>
    T &getTarget(const PackageId &p)
    {
        auto i = getChildren().find(p);
        return getTarget<T>(i, p.toString());
    }

    template <typename T = Target>
    std::shared_ptr<T> getTargetPtr(const PackagePath &Name)
    {
        auto i = std::find_if(getChildren().begin(), getChildren().end(),
            [&Name](const auto &e) { return e.first.ppath == Name; });
        return getTargetPtr<T>(i, Name.toString());
    }

    template <typename T = Target>
    std::shared_ptr<T> getTargetPtr(const PackageId &p)
    {
        auto i = getChildren().find(p);
        return getTargetPtr<T>(i, p.toString());
    }

    template <typename ... Args>
    Project &addProject(Args && ... args) { return addTarget<Project>(std::forward<Args>(args)...); }

    Directory &addDirectory(const PackagePath &Name) { return addTarget<Directory>(Name); }

    virtual TargetType getType() const = 0;
    virtual String getTypeName() const { return toString(getType()); }
    const PackageId &getPackage() const { return pkg; }

    String getConfig(bool use_short_config = false) const;
    path getBaseDir() const;
    path getServiceDir() const;
    path getTargetsDir() const;
    path getTargetDirShort(const path &root) const;
    path getTempDir() const;

    void setRootDirectory(const path &);
    void setSource(const Source &);

    /// really local package
    bool isLocal() const { return Local && !pkg.getOverriddenDir(); }
    bool isLocalOrOverridden() const { return Local && pkg.getOverriddenDir(); }

    TargetBase &operator+=(const Source &);

    /// experimental
    void operator=(const Source &);

    void fetch();

    Solution *getSolution();
    const Solution *getSolution() const;

protected:
    // impl
    path RootDirectory;
    bool prepared = false;

    TargetBase(const TargetBase &);
    //TargetBase &operator=(const TargetBase &);

    bool hasSameParent(const TargetBase *t) const;

    path getObjectDir() const;
    path getObjectDir(const PackageId &pkg) const;
    static path getObjectDir(const PackageId &pkg, const String &cfg);

private:
    template <typename T, typename ... Args>
    T &addTarget1(const PackagePath &Name, const Version &V, Args && ... args)
    {
        static_assert(std::is_base_of_v<Target, T>, "Provide a valid Target type.");

        auto t = std::make_shared<T>(std::forward<Args>(args)...);
        return (T&)addTarget2(t, Name, V);
    }

    TargetBase &addTarget2(const TargetBaseTypePtr &t, const PackagePath &Name, const Version &V);

    template <typename T = Target>
    T &getTarget(const TargetMap::iterator &i, const String &n)
    {
        return (T &)*getTargetPtr(i, n);
    }

    template <typename T = Target>
    T &getTarget(const TargetMap::const_iterator &i, const String &n) const
    {
        return (T &)*getTargetPtr(i, n);
    }

    template <typename T = Target>
    std::shared_ptr<T> getTargetPtr(const TargetMap::iterator &i, const String &n)
    {
        if (i == getChildren().end())
            throw SW_RUNTIME_ERROR("No such target: " + n);
        return std::static_pointer_cast<T>(i->second);
    }

    template <typename T = Target>
    std::shared_ptr<T> getTargetPtr(const TargetMap::const_iterator &i, const String &n) const
    {
        if (i == getChildren().end())
            throw SW_RUNTIME_ERROR("No such target: " + n);
        return std::static_pointer_cast<T>(i->second);
    }

    PackagePath constructTargetName(const PackagePath &Name) const;

    void addChild(const TargetBaseTypePtr &t);
    virtual void setupTarget(TargetBaseType *t) const;
    void applyRootDirectory();

    // impl, unreachable
    virtual bool exists(const PackageId &p) const;
    virtual TargetMap &getChildren();
    virtual const TargetMap &getChildren() const;

    friend struct Assigner;
};

struct SW_DRIVER_CPP_API TargetDescription
{
    LicenseType License = LicenseType::UnspecifiedOpenSource;
    path LicenseFilename;

    std::string fullname;
    std::string description;
    std::string url;
    std::string bugreport_url;
    std::string email;
    // build, test emails?
    /**
    * \brief Where to find this target.
    * - on site
    * - in store?
    */
    PackagePath category; // lowercase only!
    StringSet tags; // lowercase only!
                    // changes-file
                    // description-file (or readme file)
};

/**
* \brief Single project target.
*/
struct SW_DRIVER_CPP_API Target : TargetBase, std::enable_shared_from_this<Target>
    //,protected SourceFileStorage
    //, Executable // impl, must not be visible to users
{
    // rename to information?
    TargetDescription Description; // or inherit?

    using TargetBase::TargetBase;
    Target() = default;
    virtual ~Target() = default;

    virtual void init(); // add multipass init if needed
    virtual Commands getCommands() const = 0;
    virtual bool prepare() = 0;
    //virtual void clear() = 0;
    virtual void findSources() = 0;
    UnresolvedDependenciesType gatherUnresolvedDependencies() const;
    virtual DependenciesType gatherDependencies() const = 0;

    virtual void removeFile(const path &fn, bool binary_dir = false);

    //virtual Program *getSelectedTool() const = 0;

    virtual void setOutputFile() = 0;
    void setOutputDir(const path &dir);

    //auto getPreparePass() const { return prepare_pass; }
    virtual bool mustResolveDeps() const { return deps_resolved ? false : (deps_resolved = true); }

protected:
    int prepare_pass = 1;
    mutable bool deps_resolved = false;
    path OutputDir;

    path getOutputFileName() const;
};

struct SW_DRIVER_CPP_API ProjDirBase : Target
{
    using Target::Target;
    using Target::operator=;

    virtual ~ProjDirBase() = default;

    TargetType getType() const override { return TargetType::Directory; }
    void init() override {}
    Commands getCommands() const override { return Commands{}; }
    //Files getGeneratedDirs() const override { return Files{}; }
    bool prepare() override { return false; }
    //virtual void clear() override {}
    void findSources() override {}
    DependenciesType gatherDependencies() const override { return DependenciesType{}; }
    void setOutputFile() override {}
};

struct SW_DRIVER_CPP_API Directory : ProjDirBase
{
    using ProjDirBase::ProjDirBase;
    virtual ~Directory() = default;
};

struct SW_DRIVER_CPP_API Project : ProjDirBase
{
    using ProjDirBase::ProjDirBase;
    virtual ~Project() = default;

    TargetType getType() const override { return TargetType::Project; }
};

struct SW_DRIVER_CPP_API CustomTarget : Target {};

/**
* \brief Native Target is a binary target that produces binary files (probably executables).
*/
struct SW_DRIVER_CPP_API NativeTarget : Target
    //,protected NativeOptions
{
    using Target::Target;

    NativeTarget() = default;
    virtual ~NativeTarget() = default;

    DependencyPtr getDependency() const;

    virtual std::shared_ptr<builder::Command> getCommand() const = 0;
    virtual path getOutputFile() const = 0;
    virtual path getImportLibrary() const = 0;

    //
    virtual void setupCommand(builder::Command &c) const {}
    virtual void setupCommandForRun(builder::Command &c) const { setupCommand(c); } // for Launch?
};

// <SHARED | STATIC | MODULE | UNKNOWN>
struct SW_DRIVER_CPP_API ImportedTarget : NativeTarget
{
    /*using NativeTarget::NativeTarget;

    virtual ~ImportedTarget() = default;

    std::shared_ptr<Command> getCommand() const override {}
    path getOutputFile() const override;
    path getImportLibrary() const override;*/
};

// why?
// to have header only targets
struct SW_DRIVER_CPP_API InterfaceTarget : NativeTarget {};

// same source files must be autodetected and compiled once!
// but why? we already compare commands
//struct SW_DRIVER_CPP_API ObjectTarget : NativeTarget {};

struct WithSourceFileStorage {};
struct WithoutSourceFileStorage {};
struct WithNativeOptions {};
struct WithoutNativeOptions {};

//template <class ... Args>
//struct SW_DRIVER_CPP_API TargetOptions : Args...
struct SW_DRIVER_CPP_API TargetOptions : SourceFileStorage, NativeOptions
{
    using SourceFileStorage::add;
    using SourceFileStorage::remove;
    using SourceFileStorage::operator=;

    using NativeOptions::add;
    using NativeOptions::remove;
    using NativeOptions::operator=;

    void add(const IncludeDirectory &i);
    void remove(const IncludeDirectory &i);

    template <class F>
    void iterate(F &&f, const GroupSettings &s = GroupSettings())
    {
        SourceFileStorage::iterate(std::forward<F>(f), s);
        NativeOptions::iterate(std::forward<F>(f), s);
    }

    template <class F, class SFS, class NO, class ... Args>
    void iterate(F &&f, const GroupSettings &s = GroupSettings())
    {
        if constexpr (std::is_same_v<SFS, WithSourceFileStorage>)
            SourceFileStorage::iterate(std::forward<F>(f), s);
        if constexpr (std::is_same_v<NO, WithNativeOptions>)
            NativeOptions::iterate(std::forward<F>(f), s);
    }

    void merge(const TargetOptions &g, const GroupSettings &s = GroupSettings())
    {
        SourceFileStorage::merge(g, s);
        NativeOptions::merge(g, s);
    }

private:
    ASSIGN_WRAPPER(add, TargetOptions);
    ASSIGN_WRAPPER(remove, TargetOptions);
    ASSIGN_WRAPPER(remove_exclude, TargetOptions);

public:
    // source files
    //ASSIGN_TYPES(String)
    ASSIGN_TYPES_AND_EXCLUDE(path)
    ASSIGN_TYPES_AND_EXCLUDE(Files)
    ASSIGN_TYPES_AND_EXCLUDE(FileRegex)

    // compiler options
    ASSIGN_TYPES(Definition)
    ASSIGN_TYPES(DefinitionsType)
    ASSIGN_TYPES(IncludeDirectory)

    // linker options
    ASSIGN_TYPES(NativeTarget)
    ASSIGN_TYPES(LinkLibrary)

    //
    ASSIGN_TYPES(PackageId)
    ASSIGN_TYPES(DependencyPtr)
    ASSIGN_TYPES(UnresolvedPackage)
    ASSIGN_TYPES(UnresolvedPackages)

    //
    ASSIGN_TYPES(sw::tag_static_t)
    ASSIGN_TYPES(sw::tag_shared_t)
};

template <class T>
struct SW_DRIVER_CPP_API TargetOptionsGroup :
    InheritanceGroup<T>
{
private:
    ASSIGN_WRAPPER(add, TargetOptionsGroup);
    ASSIGN_WRAPPER(remove, TargetOptionsGroup);

public:
    USING_ASSIGN_OPS(TargetOptions);

    void inheritance(const TargetOptionsGroup &g, const GroupSettings &s = GroupSettings())
    {
        InheritanceGroup<TargetOptions>::inheritance(g, s);
    }

    template <class SFS, class NO, class F>
    void iterate(F &&f, const GroupSettings &s = GroupSettings())
    {
        InheritanceGroup<TargetOptions>::iterate<F, SFS, NO>(std::forward<F>(f), s);
    }

    // self merge
    void merge(const GroupSettings &s = GroupSettings())
    {
        InheritanceGroup<TargetOptions>::merge(s); // goes last
    }

    // merge to others
    void merge(const TargetOptionsGroup &g, const GroupSettings &s = GroupSettings())
    {
        auto s2 = s;
        s2.merge_to_self = false;

        InheritanceGroup<TargetOptions>::merge(g, s2);
    }
};

struct SW_DRIVER_CPP_API NativeTargetOptionsGroup : TargetOptionsGroup<TargetOptions>
{
    USING_ASSIGN_OPS(TargetOptionsGroup<TargetOptions>);

private:
    ASSIGN_WRAPPER(add, NativeTargetOptionsGroup);
    ASSIGN_WRAPPER(remove, NativeTargetOptionsGroup);

public:
    VariablesType Variables;
    ASSIGN_TYPES(Variable)

    void add(const Variable &v);
    void remove(const Variable &v);
};

/**
* \brief Native Executed Target is a binary target that must be built.
*/
struct SW_DRIVER_CPP_API NativeExecutedTarget : NativeTarget,
    NativeTargetOptionsGroup
{
    USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    String ApiName;
    StringSet ApiNames;
    std::optional<bool> HeaderOnly;
    std::optional<bool> AutoDetectOptions;
    bool Empty = false;
    std::shared_ptr<NativeLinker> Linker;
    std::shared_ptr<NativeLinker> Librarian;
    bool ExportAllSymbols = false;
    bool ExportIfStatic = false;
    path InstallDirectory;
    bool PackageDefinitions = false;

    bool ImportFromBazel = false;
    StringSet BazelNames;
    String BazelTargetFunction;
    String BazelTargetName;

    CLanguageStandard CVersion = CLanguageStandard::Unspecified;
    bool CExtensions = false;
    CPPLanguageStandard CPPVersion = CPPLanguageStandard::Unspecified;
    bool CPPExtensions = false;

    bool UseModules = false;

    // add properies - values

    // unstable
    //bool add_d_on_debug = false;

    virtual ~NativeExecutedTarget();

    TargetType getType() const override { return TargetType::NativeLibrary; }

    void init() override;
    void addPackageDefinitions(bool defs = false);
    std::shared_ptr<builder::Command> getCommand() const override;
    Commands getCommands() const override;
    //Files getGeneratedDirs() const override;
    bool prepare() override;
    path getOutputFile() const override;
    path makeOutputFile() const;
    path getImportLibrary() const override;
    void setChecks(const String &name);
    void findSources() override;
    void autoDetectOptions();
    void autoDetectSources();
    void autoDetectIncludeDirectories();
    bool hasSourceFiles() const;
    Files gatherAllFiles() const;
    Files gatherIncludeDirectories() const;
    FilesOrdered gatherLinkLibraries() const;
    NativeLinker *getSelectedTool() const;// override;
    //void setOutputFilename(const path &fn);
    void setOutputFile() override;
    virtual path getOutputBaseDir() const; // used in commands
    path getOutputDir() const;
    void removeFile(const path &fn, bool binary_dir = false) override;
    std::unordered_set<NativeSourceFile*> gatherSourceFiles() const;
    bool mustResolveDeps() const override { return prepare_pass == 2; }

    driver::cpp::CommandBuilder addCommand() const;
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
    void addPrecompiledHeader(PrecompiledHeader pch);
    NativeExecutedTarget &operator=(PrecompiledHeader pch);

    virtual bool isStaticOnly() const { return false; }
    virtual bool isSharedOnly() const { return false; }

    //
    virtual void cppan_load_project(const yaml &root);

    using TargetBase::operator=;
    using TargetBase::operator+=;

protected:
    using TargetsSet = std::unordered_set<NativeTarget*>;
    using once_mutex_t = std::recursive_mutex;

    once_mutex_t once;
    mutable NativeLinker *SelectedTool = nullptr;
    UniqueVector<Dependency*> CircularDependencies;
    std::shared_ptr<NativeLinker> CircularLinker;

    Files gatherObjectFiles() const;
    Files gatherObjectFilesWithoutLibraries() const;
    TargetsSet gatherDependenciesTargets() const;
    TargetsSet gatherAllRelatedDependencies() const;
    DependenciesType gatherDependencies() const override;
    FilesOrdered gatherLinkDirectories() const;
    bool prepareLibrary(LibraryType Type);
    void initLibrary(LibraryType Type);
    void configureFile1(const path &from, const path &to, ConfigureFlags flags);
    void detectLicenseFile();

private:
    std::optional<nlohmann::json> precomputed_data;
    //path OutputFilename;
    bool already_built = false;
    std::map<path, path> break_gch_deps;
    mutable std::optional<Commands> generated_commands;

    using Target::getOutputFileName;
    path getOutputFileName(const path &root) const;
    Commands getGeneratedCommands() const;
    void resolvePostponedSourceFiles();
    void gatherStaticLinkLibraries(LinkLibrariesType &ll, Files &added, std::unordered_set<NativeExecutedTarget*> &targets);

    void tryLoadPrecomputedData();
    void applyPrecomputedData();
    void savePrecomputedData();

    path getPatchDir(bool binary_dir) const;
};

/**
* \brief Library target that can be built as static and shared.
*/
struct SW_DRIVER_CPP_API LibraryTarget : NativeExecutedTarget
{
    using NativeExecutedTarget::operator=;

    void init() override;

protected:
    bool prepare() override;
};

using Library = LibraryTarget;

/**
* \brief Executable target.
*/
struct SW_DRIVER_CPP_API ExecutableTarget : NativeExecutedTarget//, Program
{
    TargetType getType() const override { return TargetType::NativeExecutable; }

    void cppan_load_project(const yaml &root) override;

    path getOutputBaseDir() const override;

protected:
    bool prepare() override;
};

using Executable = ExecutableTarget;

struct SW_DRIVER_CPP_API LibraryTargetBase : NativeExecutedTarget
{
    using NativeExecutedTarget::NativeExecutedTarget;
};

/**
* \brief Static only target.
*/
struct SW_DRIVER_CPP_API StaticLibraryTarget : LibraryTargetBase
{
    bool isStaticOnly() const override { return true; }

    void init() override;

    TargetType getType() const override { return TargetType::NativeStaticLibrary; }

protected:
    bool prepare() override
    {
        return prepareLibrary(LibraryType::Static);
    }
};

using StaticLibrary = StaticLibraryTarget;

/**
* \brief Shared only target.
*/
struct SW_DRIVER_CPP_API SharedLibraryTarget : LibraryTargetBase
{
    bool isSharedOnly() const override { return true; }

    void init() override;

    TargetType getType() const override { return TargetType::NativeSharedLibrary; }

protected:
    bool prepare() override
    {
        return prepareLibrary(LibraryType::Shared);
    }
};

using SharedLibrary = SharedLibraryTarget;

/**
* \brief Module only target.
*/
struct SW_DRIVER_CPP_API ModuleLibraryTarget : LibraryTarget {};

// C#

struct SW_DRIVER_CPP_API CSharpTarget : Target
    , NativeTargetOptionsGroup
{
    USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    std::shared_ptr<CSharpCompiler> compiler;

    TargetType getType() const override { return TargetType::CSharpLibrary; }

    void init() override;

    void setOutputFile() override;
    Commands getCommands(void) const override;
    bool prepare() override;
    void findSources() override;
    DependenciesType gatherDependencies() const override;

private:
    using Target::getOutputFileName;
    path getOutputFileName(const path &root) const;
};

struct SW_DRIVER_CPP_API CSharpExecutable : CSharpTarget
{
    TargetType getType() const override { return TargetType::CSharpExecutable; }
};

// Rust

struct SW_DRIVER_CPP_API RustTarget : Target
    , NativeTargetOptionsGroup
{
    USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    std::shared_ptr<RustCompiler> compiler;

    TargetType getType() const override { return TargetType::RustLibrary; }

    void init() override;

    void setOutputFile() override;
    Commands getCommands(void) const override;
    bool prepare() override;
    void findSources() override;
    DependenciesType gatherDependencies() const override;

private:
    using Target::getOutputFileName;
    path getOutputFileName(const path &root) const;
};

struct SW_DRIVER_CPP_API RustExecutable : RustTarget
{
    TargetType getType() const override { return TargetType::RustExecutable; }
};

// Go

struct SW_DRIVER_CPP_API GoTarget : Target
    , NativeTargetOptionsGroup
{
    USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    std::shared_ptr<GoCompiler> compiler;

    TargetType getType() const override { return TargetType::GoLibrary; }

    void init() override;

    void setOutputFile() override;
    Commands getCommands(void) const override;
    bool prepare() override;
    void findSources() override;
    DependenciesType gatherDependencies() const override;

private:
    using Target::getOutputFileName;
    path getOutputFileName(const path &root) const;
};

struct SW_DRIVER_CPP_API GoExecutable : GoTarget
{
    TargetType getType() const override { return TargetType::GoExecutable; }
};

// Fortran

struct SW_DRIVER_CPP_API FortranTarget : Target
    , NativeTargetOptionsGroup
{
    USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    std::shared_ptr<FortranCompiler> compiler;

    TargetType getType() const override { return TargetType::FortranLibrary; }

    void init() override;

    void setOutputFile() override;
    Commands getCommands(void) const override;
    bool prepare() override;
    void findSources() override;
    DependenciesType gatherDependencies() const override;

private:
    using Target::getOutputFileName;
    path getOutputFileName(const path &root) const;
};

struct SW_DRIVER_CPP_API FortranExecutable : FortranTarget
{
    TargetType getType() const override { return TargetType::FortranExecutable; }
};

// Java

struct SW_DRIVER_CPP_API JavaTarget : Target
    , NativeTargetOptionsGroup
{
    USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    std::shared_ptr<JavaCompiler> compiler;

    TargetType getType() const override { return TargetType::JavaLibrary; }

    void init() override;

    void setOutputFile() override;
    Commands getCommands(void) const override;
    bool prepare() override;
    void findSources() override;
    DependenciesType gatherDependencies() const override;

private:
    using Target::getOutputFileName;
    path getOutputFileName(const path &root) const;
};

struct SW_DRIVER_CPP_API JavaExecutable : JavaTarget
{
    TargetType getType() const override { return TargetType::JavaExecutable; }
};

// Kotlin

struct SW_DRIVER_CPP_API KotlinTarget : Target
    , NativeTargetOptionsGroup
{
    USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    std::shared_ptr<KotlinCompiler> compiler;

    TargetType getType() const override { return TargetType::KotlinLibrary; }

    void init() override;

    void setOutputFile() override;
    Commands getCommands(void) const override;
    bool prepare() override;
    void findSources() override;
    DependenciesType gatherDependencies() const override;

private:
    using Target::getOutputFileName;
    path getOutputFileName(const path &root) const;
};

struct SW_DRIVER_CPP_API KotlinExecutable : KotlinTarget
{
    TargetType getType() const override { return TargetType::KotlinExecutable; }
};

// D

struct SW_DRIVER_CPP_API DTarget : Target
    , NativeTargetOptionsGroup
{
    USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    std::shared_ptr<DCompiler> compiler;

    TargetType getType() const override { return TargetType::DLibrary; }

    void init() override;

    void setOutputFile() override;
    Commands getCommands(void) const override;
    bool prepare() override;
    void findSources() override;
    DependenciesType gatherDependencies() const override;

private:
    using Target::getOutputFileName;
    path getOutputFileName(const path &root) const;
};

struct SW_DRIVER_CPP_API DLibrary : DTarget
{
};

struct SW_DRIVER_CPP_API DStaticLibrary : DLibrary
{
    TargetType getType() const override { return TargetType::DStaticLibrary; }
};

struct SW_DRIVER_CPP_API DSharedLibrary : DLibrary
{
    TargetType getType() const override { return TargetType::DSharedLibrary; }
};

struct SW_DRIVER_CPP_API DExecutable : DTarget
{
    TargetType getType() const override { return TargetType::DExecutable; }
};

#undef ASSIGN_TYPES
#undef ASSIGN_OP_ACTION
#undef ASSIGN_OP
#undef USING_ASSIGN_OPS

}
