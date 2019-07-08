// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "base.h"

#include "sw/driver/command.h"
#include "sw/driver/build.h"

#include <sw/builder/jumppad.h>
#include <sw/core/sw_context.h>
#include <sw/manager/database.h>
#include <sw/manager/package_data.h>
#include <sw/manager/storage.h>
#include <sw/support/hash.h>

#include <nlohmann/json.hpp>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "target");

#define SW_BDIR_NAME "bd" // build (binary) dir
#define SW_BDIR_PRIVATE_NAME "bdp" // build (binary) private dir

/*

sys.compiler.c
sys.compiler.cpp
sys.compiler.runtime
sys.libc
sys.libcpp

sys.ar // aka lib
sys.ld // aka link

sys.kernel

*/

namespace sw
{

bool isExecutable(TargetType t)
{
    return
        0
        || t == TargetType::NativeExecutable
        || t == TargetType::CSharpExecutable
        || t == TargetType::RustExecutable
        || t == TargetType::GoExecutable
        || t == TargetType::FortranExecutable
        || t == TargetType::JavaExecutable
        || t == TargetType::KotlinExecutable
        || t == TargetType::DExecutable
        ;
}

String toString(TargetType T)
{
    switch (T)
    {
#define CASE(x) \
    case TargetType::x: \
        return #x

        CASE(Project);
        CASE(Directory);
        CASE(NativeLibrary);
        CASE(NativeExecutable);

#undef CASE
    }
    throw SW_RUNTIME_ERROR("unreachable code");
}

/*bool TargetSettings::operator<(const TargetSettings &rhs) const
{
    return std::tie(ss, dependencies, features) < std::tie(rhs.ss, rhs.dependencies, rhs.features);
}

String TargetSettings::getConfig() const
{
    return ss.getConfig();
}*/

SettingsComparator::~SettingsComparator()
{
}

bool SettingsComparator::equal(const TargetSettings &s1, const TargetSettings &s2) const
{
    return s1 == s2;
}

bool SettingsComparator::less(const TargetSettings &s1, const TargetSettings &s2) const
{
    return s1 < s2;
}

TargetBase::TargetBase()
{
}

TargetBase::TargetBase(const TargetBase &rhs)
    : TargetBaseData(rhs)
{
}

TargetBase::~TargetBase()
{
}

bool Target::hasSameProject(const ITarget &t) const
{
    if (this == &t)
        return true;
    auto t2 = t.as<const Target*>();
    if (!t2)
        return false;
    return current_project && current_project == t2->current_project;
    //return getPackage().ppath.hasSameParent(t.getPackage().ppath);
}

PackagePath TargetBase::constructTargetName(const PackagePath &Name) const
{
    return NamePrefix / (pkg ? getPackage().ppath / Name : Name);
}

TargetBase &TargetBase::addTarget2(bool add, const TargetBaseTypePtr &t, const PackagePath &Name, const Version &V)
{
    auto N = constructTargetName(Name);

    t->pkg = std::make_unique<LocalPackage>(getSolution().swctx.getLocalStorage(), N, V);

    //TargetSettings tid{ getSolution().getSettings().getTargetSettings() };
    //t->ts = &tid;
    t->ts = getSolution().getSettings();
    t->bs = t->ts;

    // set some general settings, then init, then register
    setupTarget(t.get());

    getSolution().call_event(*t, CallbackType::CreateTarget);

    // try to guess whether it's local package or not
    // TODO: one more case is when config is not local
    /*t->Local = 0
        // config -> local
        || IsConfig

        // if not under storage -> local
        //|| !is_under_root(t->SourceDir, getSolution().swctx.getLocalStorage().storage_dir_pkg)

        // if under storage but without prefix -> local
        //|| t->NamePrefix.empty()

        // absolute -> local
        || !t->pkg->ppath.isAbsolute()

        // local -> local
        || t->pkg->ppath.is_loc()

        ;*/

    t->Local = 0
        || getSolution().getCurrentGroupNumber() == 0
        || t->pkg->getOverriddenDir()
        ;

    // sdir
    if (!t->isLocal())
        t->setSourceDirectory(getSolution().getSourceDir(t->getPackage()));
    if (auto d = t->getPackage().getOverriddenDir())
        t->setSourceDirectory(*d);

    // set source dir
    if (t->SourceDir.empty())
    {
        // try to get solution provided source dir
        if (t->source)
        {
            if (auto sd = getSolution().getSourceDir(t->getSource(), t->getPackage().version); sd)
                t->setSourceDirectory(sd.value());
        }
        if (t->SourceDir.empty())
        {
            //t->SourceDir = SourceDir.empty() ? getSolution().SourceDir : SourceDir;
            //t->SourceDir = getSolution().SourceDir;
            t->setSourceDirectory(/*getSolution().*/SourceDirBase); // take from this
        }
    }

    // second try
    // try to guess whether it's local package or not
    // TODO: one more case is when config is not local
    /*t->Local = 0
        // config -> local
        || IsConfig

        // if not under storage -> local
        || !is_under_root(t->SourceDir, getSolution().swctx.getLocalStorage().storage_dir_pkg)

        // if under storage but without prefix -> local
        //|| t->NamePrefix.empty()

        // absolute -> local
        || !t->pkg->ppath.isAbsolute()

        // local -> local
        || t->pkg->ppath.is_loc()

        ;*/

    // before init
    if (!add)
        return *t;

    while (t->init())
        ;

    getSolution().call_event(*t, CallbackType::CreateTargetInitialized);

    auto &ref = addChild(t);
    //t->ts = getSolution().getSettings();
    //t->bs = t->ts;
    getSolution().call_event(*t, CallbackType::CreateTargetInitialized);
    return ref;
}

TargetBase &TargetBase::addChild(const TargetBaseTypePtr &t)
{
    if (t->getType() == TargetType::Directory || t->getType() == TargetType::Project)
    {
        getSolution().dummy_children.push_back(t);
        return *t;
    }

    // we do not activate targets that are not selected for current builds
    if (/*!isLocal() && */!getSolution().isKnownTarget(t->getPackage()))
    {
        t->DryRun = true;
        t->skip = true;
    }

    return addChild(t, t->getTargetSettings());
}

TargetBase &TargetBase::addChild(const TargetBaseTypePtr &t, const TargetSettings &tid)
{
    auto i = getSolution().getChildren().find(t->getPackage());
    if (i != getSolution().getChildren().end())
    {
        auto j = i->second.find(tid);
        if (j != i->second.end())
        {
            //throw SW_RUNTIME_ERROR("Target already exists: " + t->getPackage().toString());
            //return (*j)->as<TargetBase>();
        }
    }
    getSolution().addChild(t);
    return *t;
}

void TargetBase::setupTarget(TargetBaseType *t) const
{
    // find automatic way of copying data?

    // lang storage
    //t->languages = languages;

    // if parent target has new exts set up (e.g., .asm),
    // maybe children also want those exts automatically?
    //t->extensions = extensions;

    // inherit from this
    t->build = &getSolution();

    if (auto t0 = dynamic_cast<const Target*>(this))
        t->source = t0->source ? t0->source->clone() : nullptr;

    t->IsConfig = IsConfig; // TODO: inherit from reconsider
    t->DryRun = DryRun; // TODO: inherit from reconsider

    // inherit from solution
    //t->ParallelSourceDownload = getSolution().ParallelSourceDownload;

    //auto p = getSolution().getKnownTarget(t->getPackage().ppath);
    //if (!p.toString().empty())
}

Build &TargetBase::getSolution()
{
    return (Build &)*(build ? build : this);
}

const Build &TargetBase::getSolution() const
{
    return build ? *build : (const Build &)*this;
}

path TargetBaseData::getServiceDir() const
{
    return BinaryDir / "misc";
}

int TargetBase::getCommandStorageType() const
{
    if (getSolution().command_storage == builder::Command::CS_DO_NOT_SAVE)
        return builder::Command::CS_DO_NOT_SAVE;
    return (isLocal() && !IsConfig) ? builder::Command::CS_LOCAL : builder::Command::CS_GLOBAL;
}

bool TargetBase::isLocal() const
{
    return Local;
}

const LocalPackage &TargetBase::getPackage() const
{
    if (!pkg)
        throw SW_LOGIC_ERROR("pkg not created");
    return *pkg;
}

LocalPackage &TargetBase::getPackageMutable()
{
    if (!pkg)
        throw SW_LOGIC_ERROR("pkg not created");
    return *pkg;
}

/*Target::Target(const Target &rhs)
    : TargetBase(rhs)
    , ProgramStorage(rhs)
    //, source(rhs.source ? rhs.source->clone() : nullptr)
    , Scope(rhs.Scope)
    , RootDirectory(rhs.RootDirectory)
{
    SW_UNIMPLEMENTED;
}*/

bool Target::isReal() const
{
    return real && !sw_provided && !skip;
}

const Source &Target::getSource() const
{
    if (!source)
        throw SW_LOGIC_ERROR("source is undefined");
    return *source;
}

void Target::setSource(const Source &s)
{
    source = s.clone();

    // apply some defaults
    if (auto g = dynamic_cast<Git*>(source.get()); g && !g->isValid())
    {
        if (getPackage().version.isBranch())
        {
            if (g->branch.empty())
                g->branch = "{v}";
        }
        else
        {
            if (g->tag.empty())
            {
                g->tag = "{v}";
                g->tryVTagPrefixDuringDownload();
            }
        }
    }

    if (auto sd = getSolution().getSourceDir(s, getPackage().version); sd)
        setSourceDirectory(sd.value());

    auto d = getSolution().fetch_dir;
    if (d.empty() || !isLocal())
        return;

    auto s2 = source->clone(); // make a copy!
    s2->applyVersion(getPackage().getVersion());
    d /= s2->getHash();

    if (!fs::exists(d))
    {
        LOG_INFO(logger, "Downloading source:\n" << s2->print());
        s2->download(d);
    }
    d = d / findRootDirectory(d); // pass found regex or files for better root dir lookup
    getSolution().source_dirs_by_source[s2->getHash()] = d;
    setSourceDirectory(d);
}

Target &Target::operator+=(const Source &s)
{
    setSource(s);
    return *this;
}

Target &Target::operator+=(std::unique_ptr<Source> s)
{
    if (s)
        return operator+=(*s);
    return *this;
}

void Target::operator=(const Source &s)
{
    setSource(s);
}

void Target::fetch()
{
    if (DryRun)
        return;

    // move to swctx?
    static SourceDirMap fetched_dirs;

    auto s2 = getSource().clone(); // make a copy!
    auto i = fetched_dirs.find(s2->getHash());
    if (i == fetched_dirs.end())
    {
        path d = s2->getHash();
        d = BinaryDir / d;
        if (!fs::exists(d))
        {
            s2->applyVersion(getPackage().version);
            s2->download(d);
        }
        d = d / findRootDirectory(d);
        setSourceDirectory(d);

        fetched_dirs.emplace(s2->getHash(), d);
    }
    else
    {
        setSourceDirectory(i->second);
    }
}

bool Target::operator==(const TargetSettings &s) const
{
    if (scmp)
        return scmp->equal(ts, s);
    return ts == s;
}

/*bool Target::operator<(const TargetSettings &s) const
{
    if (scmp)
        return scmp->less(ts, s);
    return ts < s;
}*/

void Target::setSettingsComparator(std::unique_ptr<SettingsComparator> cmp)
{
    scmp = std::move(cmp);
}

Files Target::getSourceFiles() const
{
    SW_UNIMPLEMENTED;
}

std::vector<IDependency *> Target::getDependencies() const
{
    std::vector<IDependency *> deps;
    for (auto &d : gatherDependencies())
    {
        if (!d->sw_pushed)
            deps.push_back(d.get());
    }
    for (auto &d : DummyDependencies)
    {
        if (!d->sw_pushed)
            deps.push_back(d.get());
    }
    for (auto &d : SourceDependencies)
    {
        if (!d->sw_pushed)
            deps.push_back(d.get());
    }
    return deps;
}

Program *Target::findProgramByExtension(const String &ext) const
{
    if (!hasExtension(ext))
        return {};
    if (auto p = getProgram(ext))
        return p;
    auto u = getExtPackage(ext);
    if (!u)
        return {};
    // resolve via swctx because it might provide other version rather than cld.find(*u)
    auto pkg = getSolution().swctx.resolve(*u);
    auto &cld = getSolution().getChildren();
    auto tgt = cld.find(pkg, getSolution().getHostSettings());
    if (!tgt)
        return {};
    if (auto t = tgt->as<PredefinedProgram*>())
    {
        return &t->getProgram();
    }
    throw SW_RUNTIME_ERROR("Target without PredefinedProgram: " + pkg.toString());
}

String Target::getConfig() const
{
    return ts.getHash();
}

path Target::getBaseDir() const
{
    return getSolution().BinaryDir / getConfig();
}

path Target::getTargetsDir() const
{
    auto d = getSolution().BinaryDir / "out" / getConfig();
    write_file(d / "cfg.json", nlohmann::json::parse(ts.toString(TargetSettings::Json)).dump(4));
    return d;
}

path Target::getTargetDirShort(const path &root) const
{
    // make t subdir or tgt? or tgts?
    return root / "t" / getConfig() / shorten_hash(blake2b_512(getPackage().toString()), 6);
}

path Target::getTempDir() const
{
    return getServiceDir() / "temp";
}

path Target::getObjectDir() const
{
    return getObjectDir(getPackage(), getConfig());
}

path Target::getObjectDir(const LocalPackage &in) const
{
    return getObjectDir(in, getConfig());
}

path Target::getObjectDir(const LocalPackage &pkg, const String &cfg)
{
    // bld was build
    return pkg.getDirObj() / "bld" / cfg;
}

void Target::setRootDirectory(const path &p)
{
    // FIXME: add root dir to idirs?

    // set always
    RootDirectory = p;
    applyRootDirectory();
}

void Target::applyRootDirectory()
{
    // but append only in some cases
    //if (!DryRun)
    {
        // prevent adding last delimeter
        if (!RootDirectory.empty())
            //setSourceDirectory(SourceDir / RootDirectory);
            SourceDir /= RootDirectory;
    }
}

Commands Target::getCommands() const
{
    auto cmds = getCommands1();
    for (auto &c : cmds)
        c->command_storage = getCommandStorageType();
    return cmds;
}

void Target::registerCommand(builder::Command &c) const
{
    c.command_storage = getCommandStorageType();
}

void Target::removeFile(const path &fn, bool binary_dir)
{
    auto p = fn;
    if (!p.is_absolute())
    {
        if (!binary_dir && fs::exists(SourceDir / p))
            p = SourceDir / p;
        else if (fs::exists(BinaryDir / p))
            p = BinaryDir / p;
    }

    error_code ec;
    fs::remove(p, ec);
}

const BuildSettings &Target::getSettings() const
{
    return bs;
}

FileStorage &Target::getFs() const
{
    return getSolution().swctx.getFileStorage();
}

bool Target::init()
{
    auto get_config_with_deps = [this]() -> String
    {
        StringSet ss;
        /*for (const auto &[unr, res] : getPackageStore().resolved_packages)
        {
            if (res == getPackage())
            {
                for (const auto &[ppath, dep] : res.db_dependencies)
                    ss.insert(dep.toString());
                break;
            }
        }*/
        String s;
        for (auto &v : ss)
            s += v + "\n";
        auto c = getConfig();
        return c;
    };

    // this rd must come from parent!
    // but we take it in copy ctor
    setRootDirectory(RootDirectory); // keep root dir growing
                                        //t->applyRootDirectory();
                                        //t->SourceDirBase = t->SourceDir;

    if (isLocal())
    {
        BinaryDir = getTargetDirShort(getSolution().BinaryDir);
    }
    else if (auto d = getPackage().getOverriddenDir(); d)
    {
        // same as local for testing purposes?
        BinaryDir = getTargetDirShort(d.value() / SW_BINARY_DIR);

        //BinaryDir = d.value() / SW_BINARY_DIR;
        //BinaryDir /= sha256_short(getPackage().toString()); // getPackage() first
        //BinaryDir /= path(getConfig(true));
    }
    else /* package from network */
    {
        BinaryDir = getObjectDir(getPackage(), get_config_with_deps()); // remove 'build' part?
    }

    if (DryRun)
    {
        // we doing some download on server or whatever
        // so, we do not want to touch real existing bdirs
        BinaryDir = getSolution().BinaryDir / "dry" / shorten_hash(blake2b_512(BinaryDir.u8string()), 6);
        fs::remove_all(BinaryDir);
        fs::create_directories(BinaryDir);
    }

    BinaryPrivateDir = BinaryDir / SW_BDIR_PRIVATE_NAME;
    BinaryDir /= SW_BDIR_NAME;

    // we must create it because users probably want to write to it immediately
    fs::create_directories(BinaryDir);
    fs::create_directories(BinaryPrivateDir);

    // make sure we always use absolute paths
    BinaryDir = fs::absolute(BinaryDir);
    BinaryPrivateDir = fs::absolute(BinaryPrivateDir);

    SW_RETURN_MULTIPASS_END;
}

UnresolvedDependenciesType Target::gatherUnresolvedDependencies() const
{
    UnresolvedDependenciesType deps;
    for (auto &d : gatherDependencies())
    {
        if (!*d)
            deps.insert({ d->package, d });
    }
    for (auto &d : DummyDependencies)
    {
        if (!*d)
            deps.insert({ d->package, d });
    }
    for (auto &d : SourceDependencies)
    {
        if (!*d)
            deps.insert({ d->package, d });
    }
    return deps;
}

DependencyPtr Target::getDependency() const
{
    auto d = std::make_shared<Dependency>(*this);
    return d;
}

const TargetSettings &Target::getInterfaceSettings() const
{
    SW_UNIMPLEMENTED;
}

void TargetOptions::add(const IncludeDirectory &i)
{
    path idir = i.i;
    if (!idir.is_absolute())
    {
        //&& !fs::exists(idir))
        idir = target->SourceDir / idir;
        if (!target->DryRun && target->isLocal() && !fs::exists(idir))
            throw SW_RUNTIME_ERROR(target->getPackage().toString() + ": include directory does not exist: " + normalize_path(idir));

        // check if exists, if not add bdir?
    }
    IncludeDirectories.insert(idir);
}

void TargetOptions::remove(const IncludeDirectory &i)
{
    path idir = i.i;
    if (!idir.is_absolute() && !fs::exists(idir))
        idir = target->SourceDir / idir;
    IncludeDirectories.erase(idir);
}

void NativeTargetOptionsGroup::add(const Variable &v)
{
    auto p = v.v.find_first_of(" =");
    if (p == v.v.npos)
    {
        Variables[v.v];
        return;
    }
    auto f = v.v.substr(0, p);
    auto s = v.v.substr(p + 1);
    if (s.empty())
        Variables[f];
    else
        Variables[f] = s;
}

void NativeTargetOptionsGroup::remove(const Variable &v)
{
    auto p = v.v.find_first_of(" =");
    if (p == v.v.npos)
    {
        Variables.erase(v.v);
        return;
    }
    Variables.erase(v.v.substr(0, p));
}

Files NativeTargetOptionsGroup::gatherAllFiles() const
{
    // maybe cache result?
    Files files;
    for (int i = toIndex(InheritanceType::Min); i < toIndex(InheritanceType::Max); i++)
    {
        auto s = getInheritanceStorage().raw()[i];
        if (!s)
            continue;
        for (auto &f : *s)
            files.insert(f.first);
    }
    return files;
}

DependenciesType NativeTargetOptionsGroup::gatherDependencies() const
{
    DependenciesType deps;
    for (int i = toIndex(InheritanceType::Min); i < toIndex(InheritanceType::Max); i++)
    {
        auto s = getInheritanceStorage().raw()[i];
        if (!s)
            continue;
        for (auto &d : s->getRawDependencies())
            deps.insert(d);
    }
    return deps;
}

void Target::addDummyDependency(const DependencyPtr &t)
{
    DummyDependencies.push_back(t);

    auto &hs = getSolution().getHostSettings();
    auto &ds = DummyDependencies.back()->settings;

    bool use_current_settings =
        (
            // same os & arch can run apps
            ts["os"]["kernel"] == hs["os"]["kernel"] && ts["os"]["arch"] == hs["os"]["arch"]
        )
        ||
        (
            // 64-bit windows can run 32-bit apps
            hs["os"]["kernel"] == "com.Microsoft.Windows.NT" && hs["os"]["arch"] == "x86_64" &&
            ts["os"]["arch"] == "x86"
        )
        ;
    ds.merge(use_current_settings ? ts : hs);
}

void Target::addDummyDependency(const Target &t)
{
    addDummyDependency(std::make_shared<Dependency>(t));
}

void Target::addSourceDependency(const DependencyPtr &t)
{
    SourceDependencies.push_back(t);

    auto &hs = getSolution().getHostSettings();
    auto &ds = SourceDependencies.back()->settings;
    ds = {}; // accept everything
}

void Target::addSourceDependency(const Target &t)
{
    addSourceDependency(std::make_shared<Dependency>(t));
}

path Target::getFile(const Target &dep, const path &fn)
{
    addSourceDependency(dep);
    return dep.SourceDir / fn;
}

path Target::getFile(const DependencyPtr &dep, const path &fn)
{
    addSourceDependency(dep);
    return getSolution().swctx.resolve(dep->getPackage()).getDirSrc2() / fn;
}

bool ProjectTarget::init()
{
    current_project = this;
    return Target::init();
}

}
