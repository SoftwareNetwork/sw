// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <target/all.h>

#include "command.h"
#include "jumppad.h"
#include <database.h>
#include <hash.h>
#include <suffix.h>
#include <solution.h>

#include <directories.h>
#include <package_data.h>
#include <resolver.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "target");

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

        CASE(Build);
        CASE(Solution);
        CASE(Project);
        CASE(Directory);
        CASE(NativeLibrary);
        CASE(NativeExecutable);

#undef CASE
    }
    throw SW_RUNTIME_ERROR("unreachable code");
}

TargetBase::TargetBase(const TargetBase &rhs)
    : LanguageStorage(rhs)
    , ProjectDirectories(rhs)
    , pkg(rhs.pkg)
    , source(rhs.source)
    , Scope(rhs.Scope)
    , Local(rhs.Local)
    , UseStorageBinaryDir(rhs.UseStorageBinaryDir)
    , PostponeFileResolving(rhs.PostponeFileResolving)
    , DryRun(rhs.DryRun)
    , NamePrefix(rhs.NamePrefix)
    , solution(rhs.solution)
    , RootDirectory(rhs.RootDirectory)
{
}

TargetBase::~TargetBase()
{
}

bool TargetBase::hasSameParent(const TargetBase *t) const
{
    if (this == t)
        return true;
    return pkg.ppath.hasSameParent(t->pkg.ppath);
}

path TargetBase::getObjectDir() const
{
    return getObjectDir(pkg, getConfig(true));
}

path TargetBase::getObjectDir(const PackageId &in) const
{
    return getObjectDir(in, getConfig(true));
}

path TargetBase::getObjectDir(const PackageId &pkg, const String &cfg)
{
    return pkg.getDirObj() / "build" / cfg;
}

TargetBase &TargetBase::addTarget2(const TargetBaseTypePtr &t, const PackagePath &Name, const Version &V)
{
    auto N = constructTargetName(Name);

    t->pkg.ppath = N;
    t->pkg.version = V;
    //t->pkg.createNames();

    // this relaxes our requirements, reconsider?
    /*if (getSolution()->isKnownTarget(t->pkg))
    {
        auto i = getSolution()->dummy_children.find(t->pkg);
        if (i != getSolution()->dummy_children.end())
        {
            // we are adding same target for the second time
            // if it was in dummy_children, we remove and re-create it
            //getSolution()->dummy_children.erase(i);

            // we are adding same target for the second time
            // if it was in dummy_children, we add it to children and simply return reference to it
            addChild(i->second);
            getSolution()->dummy_children.erase(i);
            return *i->second;
        }
        else
        {
            auto i = getSolution()->children.find(t->pkg);
            if (i != getSolution()->children.end())
            {
                // we are adding same target for the second time
                // if it was in children, we lock it = set PostponeFileResolving
                //i->second->PostponeFileResolving = true;

                // we are adding same target for the second time
                // if it was in children we simply return reference to it
                return *i->second;
            }
        }
    }*/

    // set some general settings, then init, then register
    setupTarget(t.get());

    getSolution()->call_event(*t, CallbackType::CreateTarget);

    auto set_sdir = [&t, this]()
    {
        if (!t->Local && !t->pkg.toString().empty()/* && t->pkg.ppath.is_pvt()*/)
        {
            t->SourceDir = getSolution()->getSourceDir(t->pkg);
        }
        else if (!Local)
        {
            // if we building non-local package that is not
            //t->PostponeFileResolving = true;
        }

        // set source dir
        if (t->SourceDir.empty())
            //t->SourceDir = SourceDir.empty() ? getSolution()->SourceDir : SourceDir;
            t->SourceDir = getSolution()->SourceDir;

        // try to get solution provided source dir
        if (auto sd = getSolution()->getSourceDir(t->source, t->pkg.version); sd)
            t->SourceDir = sd.value();
    };

    set_sdir();

    // try to guess, very naive
    if (!IsConfig)
    {
        // do not create projects under storage yourself!
        //if (Local)
        t->Local = !is_under_root(t->SourceDir, getDirectories().storage_dir_pkg);

        // try to set again
        if (!t->Local)
        {
            // TODO: reconsider and remove
            if (t->pkg.ppath.is_pvt() || t->pkg.ppath[PackagePath::ElementType::Namespace] != "demo")
            {
                set_sdir();
            }
            else
            {
                PackagePath p;
                auto pf = t->SourceDir.parent_path() / "cache" / "path.txt";
                auto jf = t->SourceDir.parent_path() / "sw.json";
                if (fs::exists(pf))
                {
                    p = read_file(pf);
                }
                else
                {
                    if (!fs::exists(jf))
                        throw SW_RUNTIME_ERROR("please, recreate package: " + t->pkg.toString());

                    auto j = nlohmann::json::parse(read_file(jf));
                    p = j["path"].get<std::string>();
                    write_file(pf, t->pkg.ppath.toString());
                }

                t->NamePrefix = p.slice(0, 2);

                if (t->pkg.ppath == p.slice(2))
                {
                    throw SW_RUNTIME_ERROR("unreachable code");
                    t->pkg.ppath = constructTargetName(Name);
                    //t->pkg.createNames();

                    //set_sdir();
                    t->SourceDir = getSolution()->getSourceDir(t->pkg);
                }
            }
        }
    }

    t->setRootDirectory(RootDirectory); // keep root dir growing
    //t->applyRootDirectory();
    //t->SourceDirBase = t->SourceDir;

    while (t->init())
        ;
    addChild(t);

    getSolution()->call_event(*t, CallbackType::CreateTargetInitialized);

    return *t;
}

void TargetBase::addChild(const TargetBaseTypePtr &t)
{
    bool bad_type = t->getType() <= TargetType::Directory;
    // we do not activate targets that are not for current builds
    bool unknown_tgt = /*!IsConfig && */!Local && !getSolution()->isKnownTarget(t->pkg);
    if (bad_type || unknown_tgt)
    {
        // also disable resolving for such targets
        if (!bad_type && unknown_tgt)
        {
            t->PostponeFileResolving = true;
        }
        getSolution()->dummy_children[t->pkg] = t;
    }
    else
        getSolution()->children[t->pkg] = t;
}

void TargetBase::setupTarget(TargetBaseType *t) const
{
    bool exists = getSolution()->exists(t->pkg);
    if (exists)
        throw SW_RUNTIME_ERROR("Target already exists: " + t->pkg.toString());

    // find automatic way of copying data?

    // lang storage
    //t->languages = languages;
    //t->extensions = extensions;

    t->solution = getSolution();
    t->Local = Local;
    t->source = source;
    t->PostponeFileResolving = PostponeFileResolving;
    t->DryRun = DryRun;
    t->UseStorageBinaryDir = UseStorageBinaryDir;
    t->IsConfig = IsConfig;
    t->Scope = Scope;
    t->ParallelSourceDownload = ParallelSourceDownload;
    //auto p = getSolution()->getKnownTarget(t->pkg.ppath);
    //if (!p.toString().empty())
}

void TargetBase::add(const TargetBaseTypePtr &t)
{
    t->solution = getSolution();
    addChild(t);
}

bool TargetBase::exists(const PackageId &p) const
{
    throw SW_RUNTIME_ERROR("unreachable code");
}

TargetBase::TargetMap &TargetBase::getChildren()
{
    return getSolution()->getChildren();
}

const TargetBase::TargetMap &TargetBase::getChildren() const
{
    return getSolution()->getChildren();
}

PackagePath TargetBase::constructTargetName(const PackagePath &Name) const
{
    //is_under_root(SourceDir, getDirectories().storage_dir_pkg)
    return NamePrefix / (solution ? this->pkg.ppath / Name : Name);
}

Solution *TargetBase::getSolution()
{
    return (Solution *)(solution ? solution : this);
}

const Solution *TargetBase::getSolution() const
{
    return solution ? solution : (const Solution *)this;
}

void TargetBase::setRootDirectory(const path &p)
{
    // FIXME: add root dir to idirs?

    // set always
    RootDirectory = p;
    applyRootDirectory();
}

void TargetBase::setSource(const Source &s)
{
    source = s;
    auto d = getSolution()->fetch_dir;
    if (d.empty()/* || !ParallelSourceDownload*/ || !isLocal())
        return;

    auto s2 = source; // make a copy!
    checkSourceAndVersion(s2, pkg.getVersion());
    d /= get_source_hash(s2);

    if (!fs::exists(d))
    {
        LOG_INFO(logger, "Downloading source:\n" << print_source(s2));
        download(s2, d);
    }
    d = d / findRootDirectory(d); // pass found regex or files for better root dir lookup
    d /= getSolution()->prefix_source_dir;
    getSolution()->source_dirs_by_source[s2] = d;
    /*getSolution()->SourceDir = */SourceDir = d;
}

TargetBase &TargetBase::operator+=(const Source &s)
{
    setSource(s);
    return *this;
}

void TargetBase::operator=(const Source &s)
{
    setSource(s);
}

void TargetBase::applyRootDirectory()
{
    // but append only in some cases
    if (!PostponeFileResolving/* && Local*/)
        SourceDir /= RootDirectory;
}

String TargetBase::getConfig(bool use_short_config) const
{
    return getSolution()->Settings.getConfig(this, use_short_config);
}

path TargetBase::getBaseDir() const
{
    return getSolution()->BinaryDir / getConfig();
}

path TargetBase::getServiceDir() const
{
    return BinaryDir / "misc";
}

path TargetBase::getTargetsDir() const
{
    return getSolution()->BinaryDir / getConfig() / "targets";
}

path TargetBase::getTargetDirShort(const path &root) const
{
    // make t subdir or tgt? or tgts?
    return root / "t" / getConfig(true) / sha256_short(pkg.toString());
}

path TargetBase::getTempDir() const
{
    return getServiceDir() / "temp";
}

void TargetBase::fetch()
{
    if (PostponeFileResolving || DryRun)
        return;

    static SourceDirMap fetched_dirs;
    auto i = fetched_dirs.find(source);
    if (i == fetched_dirs.end())
    {
        path d = get_source_hash(source);
        d = BinaryDir / d;
        if (!fs::exists(d))
        {
            applyVersionToUrl(source, pkg.version);
            download(source, d);
        }
        d = d / findRootDirectory(d);
        SourceDir = d;

        fetched_dirs[source] = d;
    }
    else
    {
        SourceDir = i->second;
    }
}

int TargetBase::getCommandStorageType() const
{
    if (getSolution()->command_storage == builder::Command::CS_DO_NOT_SAVE)
        return builder::Command::CS_DO_NOT_SAVE;
    return (isLocal() && !IsConfig) ? builder::Command::CS_LOCAL : builder::Command::CS_GLOBAL;
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

bool Target::init()
{
    auto get_config_with_deps = [this]()
    {
        StringSet ss;
        for (const auto &[unr, res] : getPackageStore().resolved_packages)
        {
            if (res == pkg)
            {
                for (const auto &[ppath, dep] : res.db_dependencies)
                    ss.insert(dep.toString());
                break;
            }
        }
        String s;
        for (auto &v : ss)
            s += v + "\n";
        bool short_config = true;
        auto c = getConfig(short_config);
        //if (!s.empty())
            //addConfigElement(c, s);
        c = hashConfig(c, short_config);
        return c;
    };

    if (SW_IS_LOCAL_BINARY_DIR)
    {
        BinaryDir = getTargetDirShort(getSolution()->BinaryDir);
    }
    else if (auto d = pkg.getOverriddenDir(); d)
    {
        // same as local for testing purposes?
        BinaryDir = getTargetDirShort(d.value() / SW_BINARY_DIR);

        //BinaryDir = d.value() / SW_BINARY_DIR;
        //BinaryDir /= sha256_short(pkg.toString()); // pkg first
        //BinaryDir /= path(getConfig(true));
    }
    else /* package from network */
    {
        BinaryDir = getObjectDir(pkg, get_config_with_deps()); // remove 'build' part?
    }

    if (DryRun)
    {
        // we doing some download on server or whatever
        // so, we do not want to touch real existing bdirs
        BinaryDir = getSolution()->BinaryDir / "dry" / sha256_short(BinaryDir.u8string());
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
        if (/*!getSolution()->resolveTarget(d->package) && */!d->target)
            deps.insert({ d->package, d });
    }
    return deps;
}

DependencyPtr Target::getDependency() const
{
    auto d = std::make_shared<Dependency>(*this);
    return d;
}

void TargetOptions::add(const IncludeDirectory &i)
{
    path idir = i.i;
    if (!idir.is_absolute())
    {
        //&& !fs::exists(idir))
        idir = target->SourceDir / idir;
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
        for (auto &d : s->Dependencies)
            deps.insert(d);
    }
    return deps;
}

}
