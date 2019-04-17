// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "sw/driver/language_storage.h"
#include "sw/driver/types.h"
#include "sw/driver/source_file.h"

#include <sw/builder/node.h>
#include <sw/builder/os.h>
#include <sw/manager/license.h>
#include <sw/manager/package.h>
#include <sw/manager/package_path.h>
#include <sw/manager/source.h>

#include <any>
#include <mutex>
#include <optional>

#include "base_macro.h"

namespace sw
{

struct Solution;

namespace driver
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
bool isExecutable(TargetType T);

SW_DRIVER_CPP_API
String toString(TargetType T);

struct NativeExecutedTarget;
struct Solution;
struct Target;
struct ProjectTarget;
struct DirectoryTarget;
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

/**
* \brief TargetBase
*/
struct SW_DRIVER_CPP_API TargetBase : Node, LanguageStorage, ProjectDirectories
{
    using TargetMap = PackageVersionMapBase<TargetBaseTypePtr, std::unordered_map, primitives::version::VersionMap>;

    // hide?
    // Target Source.
    // use struct Source Source;
    Source source;

    // New root directory after downloading and unpacking.
    path UnpackDirectory;

    // command storage, use driver::Commands?
    // not needed? actual commands may be hidden in programs that lie in common Storage below
    //std::vector<std::shared_ptr<builder::Command>> CommandStorage;

    // Data storage for objects that must be alive with the target.
    // For example, program clones etc.
    std::vector<std::any> Storage;

    /**
    * \brief Target scope.
    */
    TargetScope Scope = TargetScope::Build;

    // flags
    // local projects, not fetched
    bool Local = true;
    bool UseStorageBinaryDir = false;
    bool PostponeFileResolving = false;
    bool IsConfig = false;
    bool ParallelSourceDownload = true;
    bool DryRun = false;

    PackagePath NamePrefix;
    const Solution *solution = nullptr;

public:
    TargetBase();
    virtual ~TargetBase();

    void add(const TargetBaseTypePtr &t);

    /**
    * \brief Add child target.
    */
    template <typename T, typename ... Args>
    T& add(const PackagePath &Name, Args && ... args)
    {
        if constexpr (sizeof...(Args) > 0)
        {
            if constexpr (std::is_convertible_v<std::tuple_element_t<0, std::tuple<Args...>>, Version>)
                return addTarget1<T>(Name, std::forward<Args>(args)...);
            else
                return addTarget1<T>(Name, pkg ? getPackage().version : Version(), std::forward<Args>(args)...);
        }
        else
            return addTarget1<T>(Name, pkg ? getPackage().version : Version(), std::forward<Args>(args)...);
    }

    /**
    * \brief Add child target.
    */
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

    // remove?
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
            auto pkgname = pkg ? getPackage().ppath / Name : Name;
            i = getChildren().find(pkgname);
            if (i == getChildren().end(pkgname))
                throw SW_RUNTIME_ERROR("No such target: " + Name.toString() + " or " + (pkgname).toString());
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
    ProjectTarget &addProject(Args && ... args) { return addTarget<ProjectTarget>(std::forward<Args>(args)...); }

    DirectoryTarget &addDirectory(const PackagePath &Name) { return addTarget<DirectoryTarget>(Name); }

    virtual TargetType getType() const = 0;
    String getTypeName() const { return toString(getType()); }
    const LocalPackage &getPackage() const;

    String getConfig(bool use_short_config = false) const;
    path getBaseDir() const;
    path getServiceDir() const;
    path getTargetsDir() const;
    path getTargetDirShort(const path &root) const;
    path getTempDir() const;

    void setRootDirectory(const path &);
    void setSource(const Source &);
    Source &getSource() { return source; }
    const Source &getSource() const { return source; }

    // really local package
    bool isLocal() const;
    //bool isLocalOrOverridden() const { return Local && || ? getPackage().getOverriddenDir(); }

    TargetBase &operator+=(const Source &);

    // remove?
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

    LocalPackage &getPackageMutable();

    bool hasSameParent(const TargetBase *t) const;
    int getCommandStorageType() const;

    path getObjectDir() const;
    path getObjectDir(const LocalPackage &pkg) const;
    static path getObjectDir(const LocalPackage &pkg, const String &cfg);

private:
    std::unique_ptr<LocalPackage> pkg;

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

    Commands getCommands() const;
    UnresolvedDependenciesType gatherUnresolvedDependencies() const;
    DependencyPtr getDependency() const; // returns current target as dependency
    void registerCommand(builder::Command &cmd) const;

    // main apis
    virtual bool init(); // multipass init,
    virtual bool prepare() { return false; } // multipass prepare,
    virtual Files gatherAllFiles() const { return {}; }
    virtual DependenciesType gatherDependencies() const { return DependenciesType{}; }

    // other
    virtual void removeFile(const path &fn, bool binary_dir = false);

    //auto getPreparePass() const { return prepare_pass; }
    virtual bool mustResolveDeps() const { return deps_resolved ? false : (deps_resolved = true); }

    using TargetBase::operator+=;

protected:
    SW_MULTIPASS_VARIABLE(prepare_pass);
    SW_MULTIPASS_VARIABLE(init_pass);
    mutable bool deps_resolved = false;

    path getOutputFileName() const;

private:
    virtual Commands getCommands1() const { return Commands{}; }
};

struct SW_DRIVER_CPP_API ProjDirBase : Target
{
    using Target::Target;
    using Target::operator=;

    virtual ~ProjDirBase() = default;

    TargetType getType() const override { return TargetType::Directory; }
};

struct SW_DRIVER_CPP_API DirectoryTarget : ProjDirBase
{
    using ProjDirBase::ProjDirBase;
    virtual ~DirectoryTarget() = default;
};

struct SW_DRIVER_CPP_API ProjectTarget : ProjDirBase
{
    using ProjDirBase::ProjDirBase;
    virtual ~ProjectTarget() = default;

    TargetType getType() const override { return TargetType::Project; }
};

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
    ASSIGN_TYPES(Target)
    ASSIGN_TYPES(LinkLibrary)
    ASSIGN_TYPES(SystemLinkLibrary)

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
    SW_TARGET_USING_ASSIGN_OPS(TargetOptions);

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
    SW_TARGET_USING_ASSIGN_OPS(TargetOptionsGroup<TargetOptions>);

private:
    ASSIGN_WRAPPER(add, NativeTargetOptionsGroup);
    ASSIGN_WRAPPER(remove, NativeTargetOptionsGroup);

public:
    VariablesType Variables;
    ASSIGN_TYPES(Variable)

    void add(const Variable &v);
    void remove(const Variable &v);

    Files gatherAllFiles() const;
    DependenciesType gatherDependencies() const;

    // from other target
    path getFile(const DependencyPtr &dep, const path &fn);
    path getFile(const Target &dep, const path &fn);
};

// from target.cpp

#define SW_IS_LOCAL_BINARY_DIR isLocal() && !UseStorageBinaryDir

template <class SF>
std::unordered_set<SF*> gatherSourceFiles(const SourceFileStorage &s)
{
    // maybe cache result?
    std::unordered_set<SF*> files;
    for (auto &[p, f] : s)
    {
        if (!f->isActive())
            continue;
        auto f2 = f->template as<SF>();
        if (f2)
            files.insert(f2);
    }
    return files;
}

} // namespace sw
