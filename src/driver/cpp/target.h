// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <language.h>
#include <license.h>
#include <node.h>
#include <os.h>
#include <package.h>
#include <package_path.h>
#include <source.h>
#include <source_file.h>
#include <types.h>

#include <any>
#include <mutex>
#include <optional>

#define IMPORT_LIBRARY "cppan.dll"

#define ASSIGN_WRAPPER(f, t)          \
    struct f##_files : Assigner       \
    {                                 \
        t &r;                         \
                                      \
        f##_files(t &r) : r(r)        \
        {                             \
            r.startAssignOperation(); \
        }                             \
                                      \
        using Assigner::operator();   \
                                      \
        template <class T>            \
        void operator()(const T &v)   \
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

enum class ConfigureFlags
{
    Empty = 0x0,

    AtOnly = 0x1, // @
    CopyOnly = 0x2,
    //AddToBuild          = 0x4,

    Default = Empty,//AddToBuild,
};

// append only!
enum class TargetType : uint32_t
{
    Build = 0,
    Solution = 1,
    Project = 2, // explicitly created
    Directory = 3, // implicitly created?
    NativeLibrary = 4,
    NativeExecutable = 5,
    NativeStaticLibrary = 6,
    NativeSharedLibrary = 7,
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
    using TargetMap = std::unordered_map<PackageId, TargetBaseTypePtr>;

    struct SettingsX
    {
        OS HostOS;
        //OS BuildOS; // for distributed compilation
        OS TargetOS;
        NativeToolchain Native;

        // other langs?
        // make polymorphic?

        String getConfig(const TargetBase *t, bool use_short_config = false) const;
    };
    SettingsX Settings; // current configuration

    PackageId pkg;

    /**
    * \brief Target Source.
    */
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
    std::enable_if_t<(sizeof...(Args) > 0), T&> add(const PackagePath &Name, Args && ... args)
    {
        return add1<T>(Name, std::forward<Args>(args)...);
    }

    template <typename T, typename ... Args>
    std::enable_if_t<(sizeof...(Args) == 0), T&> add(const PackagePath &Name, Args && ... args)
    {
        return addTarget1<T>(Name, pkg.version, std::forward<Args>(args)...);
    }

    // msvc bug workaround
private:
    template <typename T, typename ... Args>
    std::enable_if_t<std::is_convertible_v<std::tuple_element_t<0, std::tuple<Args...>>, Version>, T&>
        add1(const PackagePath &Name, Args && ... args)
    {
        return addTarget1<T>(Name, std::forward<Args>(args)...);
    }
    template <typename T, typename ... Args>
    std::enable_if_t<!std::is_convertible_v<std::tuple_element_t<0, std::tuple<Args...>>, Version>, T&>
        add1(const PackagePath &Name, Args && ... args)
    {
        return addTarget1<T>(Name, pkg.version, std::forward<Args>(args)...);
    }
public:

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
        auto i = std::find_if(getChildren().begin(), getChildren().end(),
            [&Name](const auto &e) { return e.first.ppath == Name; });
        if (i == getChildren().end())
        {
            auto n2 = pkg.ppath / Name;
            i = std::find_if(getChildren().begin(), getChildren().end(),
                [&Name, &n2](const auto &e) { return e.first.ppath == n2; });
        }
        return getTarget<T>(i, Name.toString());
    }

    template <typename T = Target>
    T &getTarget(const PackageId &p)
    {
        auto i = getChildren().find(p);
        return getTarget<T>(i, p.target_name);
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
        return getTargetPtr<T>(i, p.target_name);
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
    path getTargetDirShort() const;
    path getChecksDir() const;
    path getTempDir() const;

    void setRootDirectory(const path &);
    void setSource(const Source &);

    TargetBase &operator+=(const Source &);

    /// experimental
    void operator=(const Source &);

    void fetch();

    Solution *getSolution();
    const Solution *getSolution() const;

    // ???? why can't access protected in build
    path RootDirectory;
protected:
    // impl
    bool prepared = false;

    TargetBase(const TargetBase &);
    //TargetBase &operator=(const TargetBase &);

    bool hasSameParent(const TargetBase *t) const;

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
    T &getTarget(const TargetMap::const_iterator &i, const String &n)
    {
        return (T &)*getTargetPtr(i, n);
    }

    template <typename T = Target>
    std::shared_ptr<T> getTargetPtr(const TargetMap::const_iterator &i, const String &n)
    {
        if (i == getChildren().end())
            throw std::runtime_error("No such target: " + n);
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

/**
* \brief Single project target.
*/
struct SW_DRIVER_CPP_API Target : TargetBase, std::enable_shared_from_this<Target>
    //,protected SourceFileStorage
    //, Executable // impl, must not be visible to users
{
    LicenseType License = LicenseType::UnspecifiedOpenSource;
    path LicenseFilename;

    // info
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

    using TargetBase::TargetBase;
    Target() = default;
    virtual ~Target() = default;

    virtual void init() = 0;
    virtual void init2() = 0;
    virtual Commands getCommands() const = 0;
    virtual Files getGeneratedDirs() const = 0;
    virtual bool prepare() = 0;
    //virtual void clear() = 0;
    virtual void findSources() = 0;
    virtual UnresolvedDependenciesType gatherUnresolvedDependencies() const = 0;

    virtual void removeFile(const path &fn, bool binary_dir = false);

protected:
    int prepare_pass = 1;
};

struct SW_DRIVER_CPP_API ProjDirBase : Target
{
    using Target::Target;
    using Target::operator=;

    virtual ~ProjDirBase() = default;

    TargetType getType() const override { return TargetType::Directory; }
    void init() override {}
    void init2() override {}
    Commands getCommands() const override { return Commands{}; }
    Files getGeneratedDirs() const override { return Files{}; }
    bool prepare() override { return false; }
    //virtual void clear() override {}
    void findSources() override {}
    UnresolvedDependenciesType gatherUnresolvedDependencies() const override { return UnresolvedDependenciesType{}; }
    //void configureFile(path from, path to, ConfigureFlags flags = ConfigureFlags::Default) override {}
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
* \brief Native Target is a binary target that can or cannot be built.
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

    void merge(const SourceFileStorage &g, const GroupSettings &s = GroupSettings())
    {
        SourceFileStorage::merge(g, s);
    }

    void merge(const NativeOptions &g, const GroupSettings &s = GroupSettings())
    {
        NativeOptions::merge(g, s);
    }

    void merge(const TargetOptions &g, const GroupSettings &s = GroupSettings())
    {
        SourceFileStorage::merge(g, s);
        NativeOptions::merge(g, s);
    }

    void startAssignOperation()
    {
        SourceFileStorage::startAssignOperation();
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

    //
    ASSIGN_TYPES(sw::tag_static_t)
    ASSIGN_TYPES(sw::tag_shared_t)
};

struct Events_
{
    using TargetEvent = std::function<void(void)>;

    std::vector<TargetEvent> PreBuild;
    // addCustomCommand()?
    // preBuild?
    // postBuild?
    // postLink?

    Commands getCommands() const;
    void clear();
};

struct SW_DRIVER_CPP_API TargetOptionsGroup :
    InheritanceGroup<TargetOptions>
{
private:
    ASSIGN_WRAPPER(add, TargetOptionsGroup);
    ASSIGN_WRAPPER(remove, TargetOptionsGroup);

public:
    using TargetOptions::operator+=;
    using TargetOptions::operator-=;
    using TargetOptions::operator=;
    using TargetOptions::operator<<;
    using TargetOptions::operator>>;
    using TargetOptions::add;
    using TargetOptions::remove;

    Events_ Events;
    ASSIGN_TYPES_NO_REMOVE(std::function<void(void)>)

    void add(const std::function<void(void)> &f);

    VariablesType Variables;
    ASSIGN_TYPES(Variable)

    void add(const Variable &v);
    void remove(const Variable &v);

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

/**
* \brief Native Executed Target is a binary target that must be built.
*/
struct SW_DRIVER_CPP_API NativeExecutedTarget : NativeTarget,
    TargetOptionsGroup
{
    using SourceFilesSet = std::unordered_set<NativeSourceFile*>;

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

    // probably solution can be passed in setupChild() in TargetBase
    NativeExecutedTarget();
    NativeExecutedTarget(LanguageType L);
    virtual ~NativeExecutedTarget() = default;

    TargetType getType() const override { return TargetType::NativeLibrary; }

    void init() override;
    void init2() override;
    void addPackageDefinitions(bool defs = false);
    std::shared_ptr<builder::Command> getCommand() const override;
    Commands getCommands() const override;
    Files getGeneratedDirs() const override;
    bool prepare() override;
    path getOutputFile() const override;
    path getImportLibrary() const override;
    void setChecks(const String &name);
    void findSources() override;
    SourceFilesSet gatherSourceFiles() const;
    bool hasSourceFiles() const;
    Files gatherAllFiles() const;
    Files gatherIncludeDirectories() const;
    NativeLinker *getSelectedTool() const;
    void setOutputDir(const path &dir);
    virtual path getOutputDir() const;
    void removeFile(const path &fn, bool binary_dir = false) override;

    driver::cpp::CommandBuilder addCommand();

    void writeFileOnce(const path &fn, bool binary_dir = true) const;
    void writeFileOnce(const path &fn, const char *content, bool binary_dir = true) const;
    void writeFileOnce(const path &fn, const String &content, bool binary_dir = true) const;
    void writeFileSafe(const path &fn, const String &content, bool binary_dir = true) const;
    void replaceInFileOnce(const path &fn, const String &from, const String &to, bool binary_dir = false) const;
    void deleteInFileOnce(const path &fn, const String &from, bool binary_dir = false) const;
    void pushFrontToFileOnce(const path &fn, const String &text, bool binary_dir = false) const;
    void pushBackToFileOnce(const path &fn, const String &text, bool binary_dir = false) const;
    void configureFile(path from, path to, ConfigureFlags flags = ConfigureFlags::Default) const;

    /*#ifdef _MSC_VER
    #define SW_DEPRECATED __declspec(deprecated)
    #else
    #define SW_DEPRECATED
    #endif*/

    //SW_DEPRECATED
    void fileWriteOnce(const path &fn, bool binary_dir = true) const;
    //SW_DEPRECATED
    void fileWriteOnce(const path &fn, const char *content, bool binary_dir = true) const;
    //SW_DEPRECATED
    void fileWriteOnce(const path &fn, const String &content, bool binary_dir = true) const;
    //SW_DEPRECATED
    void fileWriteSafe(const path &fn, const String &content, bool binary_dir = true) const;

    void addPrecompiledHeader(const path &h, const path &cpp = path());
    void addPrecompiledHeader(const PrecompiledHeader &pch);
    NativeExecutedTarget &operator=(const PrecompiledHeader &pch);

    virtual bool isStaticOnly() const { return false; }
    virtual bool isSharedOnly() const { return false; }

    using TargetBase::operator=;
    using TargetBase::operator+=;
    using TargetOptionsGroup::operator+=;
    using TargetOptionsGroup::operator-=;
    using TargetOptionsGroup::operator=;
    using TargetOptionsGroup::operator<<;
    using TargetOptionsGroup::operator>>;
    using TargetOptionsGroup::add;
    using TargetOptionsGroup::remove;

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
    UnresolvedDependenciesType gatherUnresolvedDependencies() const override;
    FilesOrdered gatherLinkDirectories() const;
    FilesOrdered gatherLinkLibraries() const;
    bool prepareLibrary(LibraryType Type);
    void setOutputFile();
    void initLibrary(LibraryType Type);
    void configureFile1(const path &from, const path &to, ConfigureFlags flags) const;
    void detectLicenseFile();

private:
    path OutputDir;
    bool already_built = false;

    void autoDetectOptions();
    path getOutputFileName(const path &root) const;
    Commands getGeneratedCommands() const;
    void resolvePostponedSourceFiles();

    path getPatchDir(bool binary_dir) const;
};

/**
* \brief Library target that can be built as static and shared.
*/
struct SW_DRIVER_CPP_API LibraryTarget : NativeExecutedTarget
{
    LibraryTarget() = default;
    LibraryTarget(LanguageType L);

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
    using NativeExecutedTarget::NativeExecutedTarget;

    TargetType getType() const override { return TargetType::NativeExecutable; }

    /*ExecutableTarget &operator=(const DependenciesType &t)
    {
        NativeLinkerOptions::operator=(t);
        return *this;
    }*/

protected:
    bool prepare() override;
    path getOutputDir() const override;
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
    //using LibraryTargetBase::LibraryTargetBase;
    StaticLibraryTarget() = default;
    StaticLibraryTarget(LanguageType L);

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
    //using LibraryTargetBase::LibraryTargetBase;
    SharedLibraryTarget() = default;
    SharedLibraryTarget(LanguageType L);

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

#undef ASSIGN_TYPES
#undef ASSIGN_OP_ACTION
#undef ASSIGN_OP

}
