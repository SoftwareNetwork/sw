// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "base.h"

#include "../entry_point.h"
#include "../command.h"
#include "../build.h"
#include "../compiler/detect.h"
#include "../compiler/set_settings.h"

#include <sw/builder/jumppad.h>
#include <sw/core/build.h>
#include <sw/core/package.h>
#include <sw/core/sw_context.h>
#include <sw/manager/database.h>
#include <sw/manager/storage.h>
#include <sw/support/hash.h>

#include <boost/algorithm/string.hpp>
#include <nlohmann/json.hpp>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "target");

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

void TargetEvents::add(CallbackType t, const std::function<void()> &cb)
{
    events.push_back({ t, cb });
}

void TargetEvents::call(CallbackType t) const
{
    for (auto &e : events)
    {
        if (e.t == t && e.cb)
            e.cb();
    }
}

/*TargetBaseData::TargetBaseData(const TargetBaseData &rhs)
    : ProjectDirectories(rhs), TargetEvents(rhs)
{
}*/

SwBuild &TargetBaseData::getMainBuild() const
{
    if (!main_build_)
        throw SW_LOGIC_ERROR("main_build is not set");
    return *main_build_;
}

TargetBase::TargetBase()
{
}

TargetBase::TargetBase(const TargetBase &rhs)
    : TargetBaseData(rhs)
{
}

TargetBase::TargetBase(const TargetBase &rhs, const PackageName &inpkg)
{
    auto &parent = rhs;

    // take from parent
    build = &parent.getSolution();
    main_build_ = parent.main_build_;
    Scope = parent.Scope;
    current_project = parent.current_project;

    // take from solution
    DryRun = getSolution().DryRun;
    command_storage = getSolution().command_storage;
    Local = getSolution().NamePrefix.empty();

    // other computations

    // we do not activate targets that are not selected for current builds
    DryRun |= !getSolution().isKnownTarget(inpkg);

    pkg = std::make_unique<PackageName>(inpkg);

    // pkg
    if (!DryRun)
    {
        if (!isLocal())
        {
            PackageSettings s;
            ResolveRequest rr{ inpkg, s };
            //if (!getMainBuild().getResolver().resolve(rr))
                //throw SW_RUNTIME_ERROR("Not resolved: " + inpkg.toString());
            //lpkg = dynamic_cast<LocalPackage&>(rr.getPackage()).clone();
        }
    }

    // after pkg
    if (!current_project)
        current_project = getPackage();

    if (!isLocal())
        thispkg = getSolution().module_data.known_target->clone();
}

TargetBase::~TargetBase()
{
}

PackagePath TargetBase::constructTargetName(const PackagePath &Name) const
{
    return NamePrefix / (pkg ? getPackage().getPath() / Name : Name);
}

void TargetBase::addTarget3(ITargetPtr t)
{
    auto &ref = *t;
    static_cast<ExtendedBuild &>(getSolution()).addTarget(std::move(t));
    if (!getSolution().isKnownTarget(ref.getPackage()))
        getSolution().module_data.markAsDummy(ref);
}

void TargetBase::addTarget2(Target &t)
{
    t.DryRun |= t.ts["dry-run"] && t.ts["dry-run"].get<bool>();

    if (!t.DryRun)
        t.init();

    // after setup
    t.call(CallbackType::CreateTarget);

    // add child
    if (t.getType() == TargetType::Directory || t.getType() == TargetType::Project)
    {
        getSolution().module_data.markAsDummy(t);
        return;
    }

    /*bool dummy = false;
    auto it = getMainBuild().getTargets().find(t.getPackage());
    if (it != getMainBuild().getTargets().end())
    {
        auto i = it->second.findEqual(t.ts);
        dummy = i != it->second.end();
    }*/

    // we do not activate targets that are not selected for current builds
    if (t.DryRun)
        t.ts["dry-run"] = true;

    //if (!t.DryRun)
        //getMainBuild().registerTarget(t);
}

const SwContext &TargetBase::getContext() const
{
    return getMainBuild().getContext();
}

Build &TargetBase::getSolution()
{
    return (Build &)*(build ? build : this);
}

const Build &TargetBase::getSolution() const
{
    return build ? *build : (const Build &)*this;
}

bool TargetBase::isLocal() const
{
    return Local;
}

bool TargetBase::isOverridden() const
{
    return 1
        && !isLocal()
        && !is_under_root_by_prefix_path(getLocalPackage().getRootDirectory(), getContext().getLocalStorage().storage_dir)
        ;
}

const PackageName &TargetBase::getPackage() const
{
    if (!pkg)
        throw SW_LOGIC_ERROR("pkg not created");
    return *pkg;
}

const Package &TargetBase::getLocalPackage() const
{
    if (!thispkg)
        throw SW_LOGIC_ERROR("pkg not created");
    return *thispkg;
}

Target::Target(TargetBase &parent, const PackageName &inpkg)
    : TargetBase(parent, inpkg)
{
    input_ts = static_cast<ExtendedBuild &>(getSolution()).getSettings();
    ts = input_ts;
    bs = ts;
    DryRun |= ts.empty();

    if (auto t0 = dynamic_cast<const Target*>(&parent))
        source = t0->source ? t0->source->clone() : nullptr;

    // sdir
    if (!isLocal())
        setSourceDirectory(getLocalPackage().getSourceDirectory());
    // set source dir
    if (SourceDir.empty() || (getSolution().dd && getSolution().dd->force_source))
    {
        if (getSolution().dd)
        {
            auto i = getSolution().dd->source_dirs_by_package.find(getPackage());
            if (i != getSolution().dd->source_dirs_by_package.end())
                setSourceDirectory(i->second);
        }

        // try to get solution provided source dir
        if (getSolution().dd && getSolution().dd->force_source)
            setSource(*getSolution().dd->force_source);
        if (source)
        {
            if (auto sd = getSolution().getSourceDir(getSource(), getPackage().getVersion()); sd)
                setSourceDirectory(sd.value());
        }
        if (SourceDir.empty())
        {
            //t->SourceDir = SourceDir.empty() ? getSolution().SourceDir : SourceDir;
            //t->SourceDir = getSolution().SourceDir;
            setSourceDirectory(/*getSolution().*/parent.SourceDirBase); // take from parent
        }
    }

    // this RootDirectory must come from parent!
    // but we take it in copy ctor
    setRootDirectory(RootDirectory); // keep root dir growing
}

Target::~Target()
{
}

bool Target::hasSameProject(const ITarget &t) const
{
    if (this == &t)
        return true;
    auto t2 = t.as<const Target*>();
    if (!t2)
        return false;
    return
        current_project && t2->current_project &&
        current_project == t2->current_project;
}

const Source &Target::getSource() const
{
    if (!source)
        throw SW_LOGIC_ERROR(getPackage().toString() + ": source is undefined");
    return *source;
}

void Target::setSource(const Source &s)
{
    source = s.clone();

    // apply some defaults
    if (auto g = dynamic_cast<Git*>(source.get()); g && !g->isValid())
    {
        if (getPackage().getVersion().isBranch())
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

    if (auto sd = getSolution().getSourceDir(getSource(), getPackage().getVersion()); sd)
        setSourceDirectory(sd.value());
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

    // move to getContext()?
    static support::SourceDirMap fetched_dirs;

    auto s2 = getSource().clone(); // make a copy!
    auto i = fetched_dirs.find(s2->getHash());
    if (i == fetched_dirs.end())
    {
        path d = s2->getHash();
        SW_UNIMPLEMENTED;
        d = BinaryDir / d;
        if (!fs::exists(d))
        {
            s2->apply([this](auto &&s) { return getPackage().getVersion().format(s); });
            s2->download(d);
        }
        fetched_dirs[s2->getHash()].root_dir = d;
        d = d / support::findRootDirectory(d);
        setSourceDirectory(d);

        fetched_dirs[s2->getHash()].requested_dir = d;
    }
    else
    {
        setSourceDirectory(i->second.getRequestedDirectory());
    }
}

TargetFiles Target::getFiles() const
{
    /*switch (t)
    {
    case StorageFileType::SourceArchive:
    {
        TargetFiles files;
        for (auto &f : gatherAllFiles())
            files.emplace(f, TargetFile(f
                //, SourceDirBase
                , File(f, getFs()).isGenerated()));
        return files;
    }
    }*/
    SW_UNIMPLEMENTED;
}

std::vector<IDependency *> Target::getDependencies() const
{
    std::vector<IDependency *> deps;
    for (auto &d : gatherDependencies())
        deps.push_back(d);
    for (auto &d : DummyDependencies)
        deps.push_back(d.get());
    for (auto &d : SourceDependencies)
        deps.push_back(d.get());
    auto rd = getRuleDependencies();
    for (auto &d : rd)
    {
        if (d->getUnresolvedPackageId().getSettings().empty())
            setDummyDependencySettings(d);
        deps.push_back(d.get());
    }
    return deps;
}

PackageSettings Target::getHostSettings() const
{
    if (ts_export["use_same_config_for_host_dependencies"])
        return ts_export;
    auto hs = getMainBuild().getContext().getHostSettings();
    // reconsider this?
    // Whole host settings can be taken from user config in ~/.sw/sw.yml
    //hs["resolver"] = ts_export["resolver"];
    //hs["resolver"].setResolver(); // clear resolving, should we?
    //hs.erase("resolver"); // clear resolving, should we?
    addSettingsAndSetHostPrograms((Target&)*this, hs);
    return hs;
}

String Target::getConfig() const
{
    if (isLocal() && !provided_cfg.empty())
        return provided_cfg;
    return ts.getHashString();
}

path Target::getLocalOutputBinariesDirectory() const
{
    path d;
    if (ts["output_directory"])
        d = (const char8_t *)ts["output_directory"].getValue().c_str();
    else
        d = getMainBuild().getBuildDirectory() / "out" / getConfig();
    try
    {
        if (!fs::exists(d / "cfg.json"))
            write_file(d / "cfg.json", nlohmann::json::parse(ts.toString(PackageSettings::Json)).dump(4));
    }
    catch (...) {} // write once
    return d;
}

path Target::getTargetDirShort(const path &root) const
{
    auto tgtdir = shorten_hash(blake2b_512(getPackage().toString()), 6);
    // p to keep the same like in storage
    // p - packages
    return root / "p" / tgtdir / getConfig();
}

path Target::getObjectDir() const
{
    SW_UNIMPLEMENTED;
    //return getObjectDir(getLocalPackage());
}

path Target::getObjectDir(const Package &in) const
{
    SW_UNIMPLEMENTED;
    //return getObjectDir(in, getConfig());
}

path Target::getObjectDir(const Package &pkg, const String &cfg)
{
    SW_UNIMPLEMENTED;
    //return pkg.getDirObj(cfg);
}

void Target::setRootDirectory(const path &p)
{
    // FIXME: add root dir to idirs?

    // set always
    RootDirectory = p;

    // prevent adding last delimeter
    if (!RootDirectory.empty())
        //setSourceDirectory(SourceDir / RootDirectory);
        SourceDir /= RootDirectory;
}

CommandStorage *Target::getCommandStorage() const
{
    if (DryRun)
        return nullptr;
    if (command_storage)
        return *command_storage;
    return &getMainBuild().getCommandStorage(getBinaryDirectory().parent_path());
}

Commands Target::getCommands() const
{
    if (!commands.empty())
        return commands;
    //((Target&)*this).prepare2();
    commands = getCommands1();
    for (auto &c : commands)
    {
        if (!c->command_storage)
        {
            c->command_storage = getCommandStorage();
            if (!c->command_storage)
                c->always = true;
        }
        c->setFileStorage(getFs());
    }
    for (auto &c : commands)
        ((Target*)this)->registerCommand(*c);

    auto cmds = commands;
    for (auto &d : getDependencies())
    {
        if (auto d2 = dynamic_cast<Dependency *>(d))
            cmds.merge(d2->transform->get_commands());
    }

    return cmds;
}

void Target::registerCommand(builder::Command &c)
{
    c.setFileStorage(getFs());
    Storage.push_back(c.shared_from_this());
}

void Target::removeFile(const path &fn, bool binary_dir)
{
    auto p = fn;
    if (!p.is_absolute())
    {
        if (!binary_dir && fs::exists(SourceDir / p))
            p = SourceDir / p;
        else if (fs::exists(getBinaryDirectory() / p))
            p = getBinaryDirectory() / p;
    }

    error_code ec;
    fs::remove(p, ec);
}

const BuildSettings &Target::getBuildSettings() const
{
    return bs;
}

FileStorage &Target::getFs() const
{
    return getMainBuild().getFileStorage();
}

void Target::init()
{
    if (ts["name"])
        provided_cfg = ts["name"].getValue();
    if (ts["reproducible-build"])
        ReproducibleBuild = ts["reproducible-build"].get<bool>();

    ts_export = ts;
    //ts_export.erase("resolver");

    //BinaryDir = getBinaryParentDir();

    // remove whole condition block?
    /*if (DryRun)
    {
        // we doing some download on server or whatever
        // so, we do not want to touch real existing bdirs
        BinaryDir = getMainBuild().getBuildDirectory() / "dry" / shorten_hash(blake2b_512(BinaryDir.u8string()), 6);
        std::error_code ec;
        fs::remove_all(BinaryDir, ec);
        //fs::create_directories(BinaryDir);
    }*/

    //BinaryPrivateDir = BinaryDir / SW_BDIR_PRIVATE_NAME;
    //BinaryDir /= SW_BDIR_NAME;
    //setBinaryDirectory(BinaryDir);

    // we must create it because users probably want to write to it immediately
    //fs::create_directories(BinaryDir);
    //fs::create_directories(BinaryPrivateDir);
}

path Target::getBinaryParentDir() const
{
    if (isLocal())
        return getTargetDirShort(getMainBuild().getBuildDirectory());
    else
    {
        if (isOverridden())
            return getTargetDirShort(getLocalPackage().getRootDirectory() / SW_BINARY_DIR);

        auto cfg = getConfig();
        auto basecfgdir = getLocalPackage().getRootDirectory().parent_path();
        return basecfgdir / getConfig();
    }
}

DependencyPtr Target::getDependency() const
{
    auto d = std::make_shared<Dependency>(UnresolvedPackageId{ getPackage() });
    return d;
}

PackageSettings &Target::getSettings()
{
    if (!can_update_settings)
        throw SW_RUNTIME_ERROR("Cannot update settings anymore");
    return ts;
}

const PackageSettings &Target::getSettings() const
{
    return ts;
}

const PackageSettings &Target::getInterfaceSettings() const
{
    return interface_settings;
}

void TargetOptions::add(const IncludeDirectory &i)
{
    path dir = i.i;
    if (!dir.is_absolute())
    {
        //&& !fs::exists(dir))
        dir = getTarget().SourceDir / dir;
        if (!getTarget().DryRun && getTarget().isLocal() && !fs::exists(dir))
            throw SW_RUNTIME_ERROR(getTarget().getPackage().toString() + ": include directory does not exist: " + to_string(normalize_path(dir)));

        // check if exists, if not add bdir?
    }
    IncludeDirectories.insert(dir);
}

void TargetOptions::remove(const IncludeDirectory &i)
{
    path dir = i.i;
    if (!dir.is_absolute() && !fs::exists(dir))
        dir = getTarget().SourceDir / dir;
    IncludeDirectories.erase(dir);
}

void TargetOptions::add(const ForceInclude &i)
{
    path fi = i.i;
    check_absolute(fi);
    ForceIncludes.push_back(fi);
}

void TargetOptions::remove(const ForceInclude &i)
{
    SW_UNIMPLEMENTED;
    path fi = i.i;
    check_absolute(fi);
    //ForceIncludes.erase(fi);
}

void TargetOptions::add(const LinkDirectory &i)
{
    path dir = i.d;
    if (!dir.is_absolute())
    {
        //&& !fs::exists(dir))
        dir = getTarget().SourceDir / dir;
        if (!getTarget().DryRun && getTarget().isLocal() && !fs::exists(dir))
            throw SW_RUNTIME_ERROR(getTarget().getPackage().toString() + ": link directory does not exist: " + to_string(normalize_path(dir)));

        // check if exists, if not add bdir?
    }
    LinkDirectories.insert(dir);
}

void TargetOptions::remove(const LinkDirectory &i)
{
    path dir = i.d;
    if (!dir.is_absolute() && !fs::exists(dir))
        dir = getTarget().SourceDir / dir;
    LinkDirectories.erase(dir);
}

void TargetOptions::add(const SystemLinkLibrary &l)
{
    auto l2 = l;
    if (l2.l.extension() == ".lib" && getTarget().getBuildSettings().TargetOS.getStaticLibraryExtension() == l2.l.extension())
        l2.l = boost::to_upper_copy(l.l.u8string());
    NativeOptions::add(l2);
}

void TargetOptions::remove(const SystemLinkLibrary &l)
{
    auto l2 = l;
    if (l2.l.extension() == ".lib" && getTarget().getBuildSettings().TargetOS.getStaticLibraryExtension() == l2.l.extension())
        l2.l = boost::to_upper_copy(l.l.u8string());
    NativeOptions::remove(l2);
}

void TargetOptions::add(const PrecompiledHeader &i)
{
    if (getTarget().DryRun)
        return;

    if (i.h.empty())
        throw SW_RUNTIME_ERROR("empty pch fn");
    if (i.h[0] == '<' && i.h.back() == '>')
    {
        PrecompiledHeaders.insert(i.h);
        return;
    }
    if (i.h[0] == '\"' && i.h.back() == '\"')
    {
        PrecompiledHeaders.insert(i.h);
        return;
    }

    path p = i.h;
    check_absolute(p);
    PrecompiledHeaders.insert(p);
}

void TargetOptions::remove(const PrecompiledHeader &i)
{
    if (getTarget().DryRun)
        return;

    if (i.h.empty())
        throw SW_RUNTIME_ERROR("empty pch fn");
    if (i.h[0] == '<' && i.h.back() == '>')
    {
        PrecompiledHeaders.erase(i.h);
        return;
    }
    if (i.h[0] == '\"' && i.h.back() == '\"')
    {
        PrecompiledHeaders.erase(i.h);
        return;
    }

    path p = i.h;
    check_absolute(p);
    PrecompiledHeaders.erase(p);
}

void TargetOptions::add(const Framework &f)
{
    Frameworks.push_back(f.f);
}

void TargetOptions::remove(const Framework &f)
{
    Frameworks.erase(f.f);
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
    for (auto &f : getMergeObject())
        files.insert(f.first);
    return files;
}

std::set<Dependency*> NativeTargetOptionsGroup::gatherDependencies() const
{
    std::set<Dependency*> deps;
    for (int i = toIndex(InheritanceType::Min); i < toIndex(InheritanceType::Max); i++)
    {
        auto s = getInheritanceStorage().raw()[i];
        if (!s)
            continue;
        for (auto &d : s->getRawDependencies())
            deps.insert(d.get());
    }
    return deps;
}

DependencyPtr Target::addDummyDependencyRaw(const DependencyPtr &t)
{
    DummyDependencies.push_back(t);
    return t;
}

DependencyPtr Target::addDummyDependency(const DependencyPtr &t)
{
    if (DryRun)
        return t;

    auto t2 = addDummyDependencyRaw(t);
    setDummyDependencySettings(t2);
    return t2;
}

DependencyPtr Target::addDummyDependency(const Target &t)
{
    return addDummyDependency(std::make_shared<Dependency>(UnresolvedPackageId{ t.getPackage() }));
}

void Target::setDummyDependencySettings(DependencyPtr &t2) const
{
    t2->getUnresolvedPackageId().getSettings().mergeMissing(getHostSettings());
}

void Target::addSourceDependency(const DependencyPtr &t)
{
    return; // ignore for now
    SourceDependencies.push_back(t);

    auto &ds = SourceDependencies.back()->getUnresolvedPackageId().getSettings();
    ds = {}; // accept everything
}

void Target::addSourceDependency(const Target &t)
{
    addSourceDependency(std::make_shared<Dependency>(UnresolvedPackageId{ t.getPackage() }));
}

Resolver &Target::getResolver() const
{
    return getSolution().getResolver();
}

bool Target::resolve(ResolveRequest &rr, bool add_to_resolver)
{
    auto &r = getResolver();
    auto &ssr = getSettings()["resolver"];

    if (ssr.isEmpty())
        ssr.setResolver();

    auto id = ssr.resolve(rr);
    if (!id)
    {
        auto ret = r.resolve(rr);
        if (ret && add_to_resolver)
            ssr.addResolvedPackage(rr.getUnresolvedPackageName(), rr.getSettings(), rr.getPackage().getId());
        return ret;
    }

    ResolveRequest rrnew{id->getName(), id->getSettings()};
    auto ret = r.resolve(rrnew);
    if (ret)
        rr.setPackageForce(std::move(rrnew.r));
    return ret;
}

void Target::resolveDependency(IDependency &d)
{
    if (DryRun)
        return;

    LOG_TRACE(logger, "Resolving " << d.getUnresolvedPackageId().getName().toString()
        << ": " << d.getUnresolvedPackageId().getSettings().toString());

    if (d.getUnresolvedPackageId().getName().getPath().isAbsolute())
    {
        ResolveRequest rr{ d.getUnresolvedPackageId() };
        if (!resolve(rr, true))
        {
            // try to resolve sources
            PackageSettings s;
            ResolveRequest rr2{ d.getUnresolvedPackageId().getName(), s };
            if (!resolve(rr2, false))
                throw SW_RUNTIME_ERROR("Cannot resolve package " + rr.toString() + " and " + rr2.toString());
            auto installed = getContext().getLocalStorage().install(rr2.getPackage());
            auto &p2 = installed ? *installed : rr2.getPackage();

            auto loader = getContext().load_package(p2);
            auto transform = loader->load(d.getUnresolvedPackageId().getSettings());
            ((Dependency&)d).setTarget(std::move(transform));
            //auto &t = getMainBuild().load(*p);
            //d.setTarget(t);

            // we save original request to resolver
            getSettings()["resolver"].addResolvedPackage(rr.getUnresolvedPackageName(), rr.getSettings(), PackageId{ p2.getId().getName(), rr.getSettings() });
        }
        else
        {
            auto loader = getContext().load_package(rr.getPackage());
            auto transform = loader->load(d.getUnresolvedPackageId().getSettings());
            ((Dependency&)d).setTarget(std::move(transform));
        }
        return;
    }

    // local package
    ResolveRequest rr{ d.getUnresolvedPackageId() };
    SW_UNIMPLEMENTED;
    //auto &t = getMainBuild().resolveAndLoad(rr);
    //d.setTarget(t);
    return;
}

path Target::getFile(const Target &dep, const path &fn)
{
    if (DryRun)
        return {};

    addSourceDependency(dep); // main trick is to add a dependency
    auto p = dep.SourceDir;
    if (!fn.empty())
        p /= fn;
    return p;
}

path Target::getFile(const DependencyPtr &dep, const path &fn)
{
    if (DryRun)
        return {};

    addSourceDependency(dep); // main trick is to add a dependency
    ResolveRequest rr{ dep->getUnresolvedPackageId() };
    resolve(rr, true);
    auto p2 = getMainBuild().getContext().getLocalStorage().install(rr.getPackage());
    auto &lp = p2 ? *p2 : rr.getPackage();
    auto p = lp.getSourceDirectory();
    // allow to get dirs
    if (!fn.empty())
        p /= fn;
    return p;
}

PackageSettings &Target::getOptions()
{
    // only export options are changeable
    return getExportOptions()["options"].getMap();
}

const PackageSettings &Target::getOptions() const
{
    return getSettings()["options"].getMap();
}

PackageSettings &Target::getExportOptions()
{
    return ts_export;
}

const PackageSettings &Target::getExportOptions() const
{
    return ts_export;
}

driver::CommandBuilder Target::addCommand(const std::shared_ptr<builder::Command> &in)
{
    driver::CommandBuilder cb(*this, in);
    // set as default
    // source dir contains more files than bdir?
    // sdir or bdir?
    cb->working_directory = SourceDir;
    //setupCommand(*cb.c);
    return cb;
}

driver::CommandBuilder Target::addCommand(const String &func_name, void *f, int version)
{
    auto c = std::make_shared<BuiltinCommand>(func_name, f, version);
    return addCommand(c);
}

void Target::addGeneratedCommand(const std::shared_ptr<::sw::builder::Command> &c)
{
    generated_commands1.insert(c);
    //Storage.push_back(c);
}

String Target::getTestName(const String &name) const
{
    return name.empty() ? std::to_string(tests.size() + 1) : name;
}

Test Target::addTest()
{
    return addTest(*this);
}

Test Target::addTest(const String &name)
{
    return addTest1(getTestName(name), *this);
}

Test Target::addTest(const Target &tgt, const String &name)
{
    return addTest1(getTestName(name), tgt);
}

Test Target::addTest1(const String &name, const Target &tgt)
{
    // add into that target, so executable will be set up correctly?
    auto c = /*tgt.*/addCommand();

    // erase from gencom and put into storage
    generated_commands1.erase(c.getCommand());
    Storage.push_back(c.getCommand());

    // test only local targets
    return c; // for now
    SW_UNIMPLEMENTED;
    //if (!isLocal() || getPackage().getOverriddenDir())
        //return c;

    auto d = std::make_shared<Dependency>(UnresolvedPackageId{ tgt.getPackage() });
    d->getUnresolvedPackageId().getSettings() = getSettings(); // same settings!
    SW_UNIMPLEMENTED;
    //d->setTarget(tgt); // "resolve" right here
    // manual setup
    std::dynamic_pointer_cast<::sw::driver::Command>(c.getCommand())->setProgram(d);
    Storage.push_back(d); // keep dependency safe, because there's weak ptr in command
    Test t(c);
    addTest(t, name);
    return t;
}

void Target::addTest(Test &cb, const String &name)
{
    auto &c = cb.getCommand();
    c->name = name;
    tests.insert(c);
}

DependencyPtr Target::constructThisPackageDependency(const String &name)
{
    PackageName id(NamePrefix / name, getPackage().getVersion());
    return std::make_shared<Dependency>(UnresolvedPackageId{ id });
}

void ProjectTarget::init()
{
    current_project = getPackage();
    return Target::init();
}

path getOutputFileName(const Target &t)
{
    return t.getPackage().toString();
}

path getBaseOutputDirNameForLocalOnly(const Target &t, const path &root, const path &OutputDir)
{
    path p;
    /*if (auto d = t.getPackage().getOverriddenDir(); d)
    {
        p = *d / SW_BINARY_DIR / "out" / t.getConfig() / OutputDir;
    }
    else */if (t.isLocal())
    {
        p = t.getLocalOutputBinariesDirectory() / OutputDir;
    }
    else
    {
        SW_UNIMPLEMENTED;
        p = root / t.getConfig() / OutputDir;
    }
    return p;
}

path getBaseOutputDirName(const Target &t, const path &OutputDir, const path &subdir)
{
    if (t.isLocal())
        return getBaseOutputDirNameForLocalOnly(t, {}, OutputDir);
    else
        return t.BinaryDir.parent_path() / subdir;
}

path getBaseOutputFileNameForLocalOnly(const Target &t, const path &root, const path &OutputDir)
{
    return getBaseOutputDirNameForLocalOnly(t, root, OutputDir) / ::sw::getOutputFileName(t);
}

path getBaseOutputFileName(const Target &t, const path &OutputDir, const path &subdir)
{
    return getBaseOutputDirName(t, OutputDir, subdir) / ::sw::getOutputFileName(t);
}

}
