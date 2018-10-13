// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <target.h>

#include "bazel/bazel.h"
#include "command.h"
#include "jumppad.h"
#include <database.h>
#include <functions.h>
#include <hash.h>
#include <solution.h>
#include <suffix.h>

#include <directories.h>
#include <package_data.h>

#include <boost/algorithm/string.hpp>
#include <nlohmann/json.hpp>
#include <primitives/constants.h>
#include <primitives/sw/settings.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "target");

#define RETURN_PREPARE_PASS \
    do {prepare_pass++; return true;} while (0)

#define CPPAN_FILE_PREFIX ".sw"
#define NATIVE_TARGET_DEF_SYMBOLS_FILE \
    (BinaryDir / CPPAN_FILE_PREFIX ".symbols.def")

static cl::opt<bool> do_not_mangle_object_names("do-not-mangle-object-names");

void createDefFile(const path &def, const Files &obj_files)
#if defined(CPPAN_OS_WINDOWS)
;
#else
{}
#endif

static int create_def_file(path def, Files obj_files)
{
    createDefFile(def, obj_files);
    return 0;
}

SW_DEFINE_VISIBLE_FUNCTION_JUMPPAD(create_def_file, sw_create_def_file)

static int copy_file(path in, path out)
{
    error_code ec;
    fs::create_directories(out.parent_path());
    fs::copy_file(in, out, fs::copy_options::overwrite_existing, ec);
    return 0;
}

SW_DEFINE_VISIBLE_FUNCTION_JUMPPAD(copy_file, sw_copy_file)

namespace sw
{

class CheckPreparedTarget
{
    bool &prepared;

public:
    CheckPreparedTarget(bool &b) : prepared(b) {}
    ~CheckPreparedTarget() { prepared = true; }
};

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
    throw std::logic_error("unreachable code");
}

String TargetBase::SettingsX::getConfig(bool use_short_config) const
{
    auto remove_last_dash = [](auto &c)
    {
        if (c.size() && c.back() == '-')
            c.resize(c.size() - 1);
    };

    String c;

    addConfigElement(c, toString(TargetOS.Type));
    addConfigElement(c, toString(TargetOS.Arch));
    boost::to_lower(c);
    addConfigElement(c, Native.getConfig());

    remove_last_dash(c);

    auto h = hash_config(c);
    if (!use_short_config && c.size() + h.size() < 255/* && !use_short_hash*/) // max path part in many FSes
    {
        // hash
        addConfigElement(c, h);
        remove_last_dash(c);
        return c;
    }
    else
    {
        h = shorten_hash(h);
    }
    return h;
}

TargetBase::TargetBase(const TargetBase &rhs)
    : LanguageStorage(rhs)
    , ProjectDirectories(rhs)
    , Settings(rhs.Settings)
    , pkg(rhs.pkg)
    , source(rhs.source)
    , Scope(rhs.Scope)
    , Local(rhs.Local)
    , UseStorageBinaryDir(rhs.UseStorageBinaryDir)
    , PostponeFileResolving(rhs.PostponeFileResolving)
    , NamePrefix(rhs.NamePrefix)
    , solution(rhs.solution)
    , RootDirectory(rhs.RootDirectory)
{
}

TargetBase::~TargetBase()
{
}

/*TargetBase &TargetBase::operator=(const TargetBase &rhs)
{
    TargetBase tmp(rhs);
    //std::swap(*this, tmp);
    return *this;
}*/

bool TargetBase::hasSameParent(const TargetBase *t) const
{
    if (this == t)
        return true;
    return pkg.ppath.hasSameParent(t->pkg.ppath);
}

TargetBase &TargetBase::addTarget2(const TargetBaseTypePtr &t, const PackagePath &Name, const Version &V)
{
    auto N = constructTargetName(Name);

    t->pkg.ppath = N;
    t->pkg.version = V;
    t->pkg.createNames();

    // set some general settings, then init, then register
    setupTarget(t.get());

    auto set_sdir = [&t, this]()
    {
        if (!t->Local && !t->pkg.target_name.empty()/* && t->pkg.ppath.is_pvt()*/)
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
            t->SourceDir = SourceDir.empty() ? getSolution()->SourceDir : SourceDir;

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
                        throw std::runtime_error("please, recreate package: " + t->pkg.toString());

                    auto j = nlohmann::json::parse(read_file(jf));
                    p = j["path"].get<std::string>();
                    write_file(pf, t->pkg.ppath.toString());
                }

                t->NamePrefix = p.slice(0, 2);

                if (t->pkg.ppath == p.slice(2))
                {
                    throw std::runtime_error("unreachable code");
                    t->pkg.ppath = constructTargetName(Name);
                    t->pkg.createNames();

                    //set_sdir();
                    t->SourceDir = getSolution()->getSourceDir(t->pkg);
                }
            }
        }
    }

    t->applyRootDirectory();
    //t->SourceDirBase = t->SourceDir;

    t->init();
    t->init2();
    addChild(t);
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
    if (getSolution()->exists(t->pkg))
        throw std::runtime_error("Target already exists: " + t->pkg.target_name);

    // find automatic way of copying data?

    // lang storage
    //t->languages = languages;
    //t->extensions = extensions;

    t->Settings = Settings;
    t->solution = getSolution();
    t->Local = Local;
    t->source = source;
    t->PostponeFileResolving = PostponeFileResolving;
    t->UseStorageBinaryDir = UseStorageBinaryDir;
    t->IsConfig = IsConfig;
    t->Scope = Scope;
    //auto p = getSolution()->getKnownTarget(t->pkg.ppath);
    //if (!p.target_name.empty())
}

void TargetBase::add(const TargetBaseTypePtr &t)
{
    t->solution = getSolution();
    addChild(t);
}

bool TargetBase::exists(const PackageId &p) const
{
    throw std::logic_error("unreachable code");
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
    if (d.empty())
        return;

    auto s2 = source; // make a copy!
    checkSourceAndVersion(s2, pkg.getVersion());
    d /= get_source_hash(s2);

    if (fs::exists(d))
        return;

    LOG_INFO(logger, "Downloading source:\n" << print_source(s2));
    fs::create_directories(d);
    ScopedCurrentPath scp(d, CurrentPathScope::Thread);
    download(s2);
    d = d / findRootDirectory(d); // pass found regex or files for better root dir lookup
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
    if (!PostponeFileResolving && Local)
        SourceDir /= RootDirectory;
}

String TargetBase::getConfig(bool use_short_config) const
{
    return Settings.getConfig(use_short_config);
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

path TargetBase::getTargetDirShort() const
{
    // was
    //return getTargetsDir() / pkg.ppath.toString();
#ifdef _WIN32
    return getSolution()->BinaryDir / getConfig(true) / sha256_short(pkg.toString());
#else
    return getTargetsDir() / pkg.ppath.toString();
#endif
}

path TargetBase::getChecksDir() const
{
    return getServiceDir() / "checks";
}

path TargetBase::getTempDir() const
{
    return getServiceDir() / "temp";
}

void TargetBase::fetch()
{
    if (PostponeFileResolving)
        return;

    static std::unordered_map<Source, path> fetched_dirs;
    auto i = fetched_dirs.find(source);
    if (i == fetched_dirs.end())
    {
        path d = get_source_hash(source);
        d = BinaryDir / d;
        if (!fs::exists(d))
        {
            fs::create_directories(d);
            ScopedCurrentPath scp(d, CurrentPathScope::Thread);
            applyVersionToUrl(source, pkg.version);
            download(source);
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

path Target::getPatchDir(bool binary_dir) const
{
    auto base = (binary_dir || Local) ? BinaryDir : SourceDir;
    return base.parent_path() / "patch";
}

void Target::fileWriteOnce(const path &fn, bool binary_dir) const
{
    if (fn.is_absolute())
        fileWriteOnce(fn, "", binary_dir);
    else
        fileWriteOnce((binary_dir ? BinaryDir : SourceDir) / fn, String());
}

void Target::fileWriteOnce(const path &fn, const char *content, bool binary_dir) const
{
    fileWriteOnce(fn, String(content), binary_dir);
}

void Target::fileWriteOnce(const path &fn, const String &content, bool binary_dir) const
{
    path p;
    if (fn.is_absolute())
        p = fn;
    else
        p = (binary_dir ? BinaryDir : SourceDir) / fn;

    // before resolving
    File f(p, *getSolution()->fs);
    f.getFileRecord().setGenerated();

    if (PostponeFileResolving)
        return;

    ::sw::fileWriteOnce(p, content, getPatchDir(binary_dir));
    f.getFileRecord().load();
}

void Target::writeFileOnce(const path &fn, bool binary_dir) const
{
    fileWriteOnce(fn, binary_dir);
}

void Target::writeFileOnce(const path &fn, const String &content, bool binary_dir) const
{
    fileWriteOnce(fn, content, binary_dir);
}

void Target::writeFileOnce(const path &fn, const char *content, bool binary_dir) const
{
    fileWriteOnce(fn, content, binary_dir);
}

void Target::fileWriteSafe(const path &fn, const String &content, bool binary_dir) const
{
    if (PostponeFileResolving)
        return;

    path p;
    if (fn.is_absolute())
        ::sw::fileWriteSafe(p = fn, content, getPatchDir(binary_dir));
    else
        ::sw::fileWriteSafe(p = (binary_dir ? BinaryDir : SourceDir) / fn, content, getPatchDir(binary_dir));

    File f(fn, *getSolution()->fs);
    f.getFileRecord().load();
}

void Target::writeFileSafe(const path &fn, const String &content, bool binary_dir) const
{
    fileWriteSafe(fn, content, binary_dir);
}

void Target::replaceInFileOnce(const path &fn, const String &from, const String &to, bool binary_dir) const
{
    if (PostponeFileResolving)
        return;

    path p;
    if (fn.is_absolute())
        ::sw::replaceInFileOnce(p = fn, from, to, getPatchDir(binary_dir));
    else
        ::sw::replaceInFileOnce(p = (binary_dir ? BinaryDir : SourceDir) / fn, from, to, getPatchDir(binary_dir));

    File f(p, *getSolution()->fs);
    f.getFileRecord().load();
}

void Target::deleteInFileOnce(const path &fn, const String &from, bool binary_dir) const
{
    replaceInFileOnce(fn, from, "", binary_dir);
}

void Target::pushFrontToFileOnce(const path &fn, const String &text, bool binary_dir) const
{
    if (PostponeFileResolving)
        return;

    auto p = (binary_dir ? BinaryDir : SourceDir) / fn;
    ::sw::pushFrontToFileOnce(p, text, getPatchDir(binary_dir));

    File f(p, *getSolution()->fs);
    f.getFileRecord().load();
}

void Target::pushBackToFileOnce(const path &fn, const String &text, bool binary_dir) const
{
    if (PostponeFileResolving)
        return;

    auto p = (binary_dir ? BinaryDir : SourceDir) / fn;
    ::sw::pushBackToFileOnce(p, text, getPatchDir(binary_dir));

    File f(p, *getSolution()->fs);
    f.getFileRecord().load();
}

void Target::removeFile(const path &fn)
{
    error_code ec;
    fs::remove(fn);
}

DependencyPtr NativeTarget::getDependency() const
{
    auto d = std::make_shared<Dependency>(this);
    return d;
}

Commands Events_::getCommands() const
{
    Commands cmds;
    /*for (auto &e : PreBuild)
        cmds.insert(std::make_shared<ExecuteCommand>(*getSolution()->fs, [e] {e(); }));*/
    return cmds;
}

void Events_::clear()
{
    PreBuild.clear();
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

void TargetOptionsGroup::add(const std::function<void(void)> &f)
{
    Events.PreBuild.push_back(f);
}

void TargetOptionsGroup::add(const Variable &v)
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

void TargetOptionsGroup::remove(const Variable &v)
{
    auto p = v.v.find_first_of(" =");
    if (p == v.v.npos)
    {
        Variables.erase(v.v);
        return;
    }
    Variables.erase(v.v.substr(0, p));
}

NativeExecutedTarget::NativeExecutedTarget()
    : NativeTarget()
{
}

NativeExecutedTarget::NativeExecutedTarget(LanguageType L)
    : NativeTarget()
{
    addLanguage(L);
}

void NativeExecutedTarget::init()
{
    if (Local && !UseStorageBinaryDir)
    {
        BinaryDir = getTargetDirShort();
    }
    else
    {
        BinaryDir = pkg.getDirObj() / "build" / getConfig(true);
    }
    BinaryPrivateDir = BinaryDir / SW_BDIR_PRIVATE_NAME;
    BinaryDir /= SW_BDIR_NAME;

    // we must create it because users probably want to write to it immediately
    fs::create_directories(BinaryDir);
    fs::create_directories(BinaryPrivateDir);

    languages = solution->languages;

    //fs::rename();
    //if (UnpackDirectory.empty())

    // propagate this pointer to all
    TargetOptionsGroup::iterate<WithSourceFileStorage, WithoutNativeOptions>([this](auto &v, auto &gs)
    {
        v.target = this;
    });

    addLanguage(LanguageType::ASM);
    addLanguage(LanguageType::C);
    addLanguage(LanguageType::CPP);

    for (auto &l : languages)
    {
        if (l.first == LanguageType::C || l.first == LanguageType::CPP)
        {
            {
                auto L = dynamic_cast<LinkedLanguage<NativeLinker>*>(l.second.get());
                if (!L->linker)
                    throw std::runtime_error("Linker is not set");
                if (!Linker)
                    Linker = std::static_pointer_cast<NativeLinker>(L->linker->clone());
                else if (Linker != L->linker)
                {
                    break;
                    //throw std::runtime_error("Different linkers are set");
                }
            }

            {
                auto L = dynamic_cast<LibrarianLanguage<NativeLinker>*>(l.second.get());
                if (!L->librarian)
                    throw std::runtime_error("Librarian is not set");
                if (!Librarian)
                    Librarian = std::static_pointer_cast<NativeLinker>(L->librarian->clone());
                else if (Librarian != L->librarian)
                {
                    break;
                    //throw std::runtime_error("Different linkers are set");
                }
            }
        }
    }

    //if (!IsConfig/* && PackageDefinitions*/)
        addPackageDefinitions();
}

void NativeExecutedTarget::init2()
{
    setOutputFile();

    if (!Local)
    {
        // activate later?
        /*auto &sdb = getServiceDatabase();
        auto o = getOutputFile();
        auto f = sdb.getInstalledPackageFlags(pkg, getConfig());
        if (f[pfHeaderOnly] || fs::exists(o) && f[pfBuilt])
        {
            already_built = true;
        }*/
    }
}

driver::cpp::CommandBuilder NativeExecutedTarget::addCommand()
{
    driver::cpp::CommandBuilder cb(*getSolution()->fs);
    cb.c->addPathDirectory(getOutputDir() / getConfig());
    cb << *this;
    return cb;
}

void NativeExecutedTarget::addPackageDefinitions()
{
    tm t;
    auto tim = time(0);
#ifdef _WIN32
    gmtime_s(&t, &tim);
#else
    gmtime_r(&tim, &t);
#endif

    auto n2hex = [this](int n, int w)
    {
        std::ostringstream ss;
        ss << std::hex << std::setfill('0') << std::setw(w) << n;
        return ss.str();
    };

    auto ver2hex = [&n2hex](const auto &v, int n)
    {
        std::ostringstream ss;
        ss << n2hex(v.getMajor(), n);
        ss << n2hex(v.getMinor(), n);
        ss << n2hex(v.getPatch(), n);
        return ss.str();
    };

    auto set_pkg_info = [this, &t, &ver2hex, &n2hex](auto &a, bool quotes = false)
    {
        String q;
        if (quotes)
            q = "\"";
        a["PACKAGE"] = q + pkg.ppath.toString() + q;
        a["PACKAGE_NAME"] = q + pkg.ppath.toString() + q;
        a["PACKAGE_NAME_LAST"] = q + pkg.ppath.back() + q;
        a["PACKAGE_VERSION"] = q + pkg.version.toString() + q;
        a["PACKAGE_STRING"] = q + pkg.target_name + q;
        a["PACKAGE_BUILD_CONFIG"] = q + getConfig() + q;
        a["PACKAGE_BUGREPORT"] = q + q;
        a["PACKAGE_URL"] = q + q;
        a["PACKAGE_TARNAME"] = q + pkg.ppath.toString() + q; // must be lowercase version of PACKAGE_NAME
        a["PACKAGE_VENDOR"] = q + pkg.ppath.getOwner() + q;
        a["PACKAGE_COPYRIGHT_YEAR"] = std::to_string(1900 + t.tm_year);

        a["PACKAGE_ROOT_DIR"] = q + normalize_path(pkg.ppath.is_loc() ? RootDirectory : pkg.getDirSrc()) + q;
        a["PACKAGE_NAME_WITHOUT_OWNER"] = q/* + pkg.ppath.slice(2).toString()*/ + q;
        a["PACKAGE_NAME_CLEAN"] = q + (pkg.ppath.is_loc() ? pkg.ppath.slice(2).toString() : pkg.ppath.toString()) + q;

        //"@PACKAGE_CHANGE_DATE@"
            //"@PACKAGE_RELEASE_DATE@"

        a["PACKAGE_VERSION_MAJOR"] = std::to_string(pkg.version.getMajor());
        a["PACKAGE_VERSION_MINOR"] = std::to_string(pkg.version.getMinor());
        a["PACKAGE_VERSION_PATCH"] = std::to_string(pkg.version.getPatch());
        a["PACKAGE_VERSION_TWEAK"] = std::to_string(pkg.version.getTweak());
        a["PACKAGE_VERSION_NUM"] = "0x" + ver2hex(pkg.version, 2) + "LL";
        a["PACKAGE_VERSION_MAJOR_NUM"] = n2hex(pkg.version.getMajor(), 2);
        a["PACKAGE_VERSION_MINOR_NUM"] = n2hex(pkg.version.getMinor(), 2);
        a["PACKAGE_VERSION_PATCH_NUM"] = n2hex(pkg.version.getPatch(), 2);
        a["PACKAGE_VERSION_TWEAK_NUM"] = n2hex(pkg.version.getTweak(), 2);
        a["PACKAGE_VERSION_NUM2"] = "0x" + ver2hex(pkg.version, 4) + "LL";
        a["PACKAGE_VERSION_MAJOR_NUM2"] = n2hex(pkg.version.getMajor(), 4);
        a["PACKAGE_VERSION_MINOR_NUM2"] = n2hex(pkg.version.getMinor(), 4);
        a["PACKAGE_VERSION_PATCH_NUM2"] = n2hex(pkg.version.getPatch(), 4);
        a["PACKAGE_VERSION_TWEAK_NUM2"] = n2hex(pkg.version.getTweak(), 4);
    };
    // https://www.gnu.org/software/autoconf/manual/autoconf-2.67/html_node/Initializing-configure.html
    set_pkg_info(Definitions, true); // false?
    set_pkg_info(Variables, true); // false?
}

path NativeExecutedTarget::getOutputDir() const
{
    if (Settings.TargetOS.Type == OSType::Windows)
        return getUserDirectories().storage_dir_bin;
    else
        return getUserDirectories().storage_dir_lib;
}

void NativeExecutedTarget::setOutputDir(const path &dir)
{
    auto d = getOutputFile().parent_path();
    OutputDir = dir;
    setOutputFile();
    OutputDir = d;
}

void NativeExecutedTarget::setOutputFile()
{
    auto st = getSelectedTool();
    if (st == Librarian.get())
        getSelectedTool()->setOutputFile(getOutputFileName(getUserDirectories().storage_dir_lib));
    else
    {
        getSelectedTool()->setOutputFile(getOutputFileName(getOutputDir()));
        getSelectedTool()->setImportLibrary(getOutputFileName(getUserDirectories().storage_dir_lib));
    }
}

path NativeExecutedTarget::getOutputFileName(const path &root) const
{
    path p;
    if (Local && !UseStorageBinaryDir)
    {
        if (IsConfig)
            p = getTargetsDir() / pkg.ppath.toString() / "out" / pkg.ppath.toString();
        else
            p = getTargetsDir().parent_path() / OutputDir / pkg.ppath.toString();
    }
    else
    {
        if (IsConfig)
            p = pkg.getDir() / "out" / getConfig() / pkg.ppath.toString();
        //p = BinaryDir / "out";
        else
            p = root / getConfig() / OutputDir / pkg.ppath.toString();
    }
    //if (pkg.version.isValid() /* && add version*/)
        p += "-" + pkg.version.toString();
    return p;
}

NativeExecutedTarget::TargetsSet NativeExecutedTarget::gatherDependenciesTargets() const
{
    TargetsSet deps;
    for (auto &d : Dependencies)
    {
        if (d->target.lock().get() == this)
            continue;
        if (d->Dummy)
            continue;

        if (d->IncludeDirectoriesOnly)
            continue;
        deps.insert(d->target.lock().get());
    }
    return deps;
}

NativeExecutedTarget::TargetsSet NativeExecutedTarget::gatherAllRelatedDependencies() const
{
    auto libs = gatherDependenciesTargets();
    while (1)
    {
        auto sz = libs.size();
        for (auto &d : libs)
        {
            auto dt = ((NativeExecutedTarget*)d);
            auto libs2 = dt->gatherDependenciesTargets();

            auto sz2 = libs.size();
            libs.insert(libs2.begin(), libs2.end());
            if (sz2 != libs.size())
                break;
        }
        if (sz == libs.size())
            break;
    }
    return libs;
}

UnresolvedDependenciesType NativeExecutedTarget::gatherUnresolvedDependencies() const
{
    UnresolvedDependenciesType deps;
    ((NativeExecutedTarget*)this)->TargetOptionsGroup::iterate<WithoutSourceFileStorage, WithNativeOptions>(
        [this, &deps](auto &v, auto &s)
    {
        for (auto &d : v.Dependencies)
        {
            if (!d->target.lock())
                deps.insert({ d->package, d });
        }
    });
    return deps;
}

FilesOrdered NativeExecutedTarget::gatherLinkLibraries() const
{
    FilesOrdered libs;
    const auto dirs = gatherLinkDirectories();
    for (auto &l : LinkLibraries)
    {
        if (l.is_absolute())
        {
            libs.push_back(l);
            continue;
        }

        if (!std::any_of(dirs.begin(), dirs.end(), [&l, &libs](auto &d)
        {
            if (fs::exists(d / l))
            {
                libs.push_back(d / l);
                return true;
            }
            return false;
        }))
        {
            LOG_TRACE(logger, "Cannot resolve library: " << l);
        }

#ifndef _WIN32
        libs.push_back(l);
#endif
    }
    return libs;
}

Files NativeExecutedTarget::gatherAllFiles() const
{
    // maybe cache result?
    Files files;
    for (auto &f : *this)
    {
        //if (!isRemoved(f.first))
        files.insert(f.first);
    }
    return files;
}

Files NativeExecutedTarget::gatherIncludeDirectories() const
{
    Files idirs;
    ((NativeExecutedTarget*)this)->TargetOptionsGroup::iterate<WithoutSourceFileStorage, WithNativeOptions>(
        [this, &idirs](auto &v, auto &s)
    {
        auto idirs2 = v.gatherIncludeDirectories();
        for (auto &i2 : idirs2)
            idirs.insert(i2);
    });
    return idirs;
}

NativeExecutedTarget::SourceFilesSet NativeExecutedTarget::gatherSourceFiles() const
{
    // maybe cache result?
    SourceFilesSet files;
    for (auto &f : *this)
    {
        if (f.second->created && !f.second->skip/* && !isRemoved(f.first)*/)
            files.insert((NativeSourceFile*)f.second.get());
    }
    return files;
}

Files NativeExecutedTarget::gatherObjectFilesWithoutLibraries() const
{
    Files obj;
    for (auto &f : gatherSourceFiles())
        obj.insert(f->output.file);
    for (auto &[f, sf] : *this)
    {
#ifdef CPPAN_OS_WINDOWS
        if (f.extension() == ".obj")
        {
            obj.insert(f);
        }
#else
        if (f.extension() == ".o")
        {
            obj.insert(f);
        }
#endif
    }
    return obj;
}

Files NativeExecutedTarget::gatherObjectFiles() const
{
    auto obj = gatherObjectFilesWithoutLibraries();
    auto ll = gatherLinkLibraries();
    obj.insert(ll.begin(), ll.end());
    return obj;
}

FilesOrdered NativeExecutedTarget::gatherLinkDirectories() const
{
    FilesOrdered dirs;
    auto get_ldir = [&dirs](const auto &a)
    {
        for (auto &d : a)
            dirs.push_back(d);
    };

    get_ldir(NativeLinkerOptions::System.gatherLinkDirectories());
    get_ldir(NativeLinkerOptions::gatherLinkDirectories());

    auto dirs2 = getSelectedTool()->gatherLinkDirectories();
    // tool dirs + lib dirs, not vice versa
    dirs2.insert(dirs2.end(), dirs.begin(), dirs.end());
    return dirs2;
}

NativeLinker *NativeExecutedTarget::getSelectedTool() const
{
    if (SelectedTool)
        return SelectedTool;
    if (Linker)
        return Linker.get();
    if (Librarian)
        return Librarian.get();
    throw std::runtime_error("No tool selected");
}

path NativeExecutedTarget::getOutputFile() const
{
    return getSelectedTool()->getOutputFile();
}

path NativeExecutedTarget::getImportLibrary() const
{
    return getSelectedTool()->getImportLibrary();
}

void NativeExecutedTarget::addPrecompiledHeader(const path &h, const path &cpp)
{
    PrecompiledHeader pch;
    pch.header = h;
    pch.source = cpp;
    addPrecompiledHeader(pch);
}

void NativeExecutedTarget::addPrecompiledHeader(const PrecompiledHeader &p)
{
    auto pch = p.source;
    if (!pch.empty())
    {
        if (!fs::exists(pch))
            write_file_if_different(pch, "");
    }
    else
        pch = BinaryDir.parent_path() / "pch" / (p.header.stem().string() + ".cpp");

    auto pch_fn = pch.parent_path() / (pch.stem().string() + ".pch");
    auto obj_fn = pch.parent_path() / (pch.stem().string() + ".obj");
    auto pdb_fn = pch.parent_path() / (pch.stem().string() + ".pdb");

    // before added 'create' pch
    for (auto &f : gatherSourceFiles())
    {
        if (auto sf = f->as<CPPSourceFile>())
        {
            if (auto c = sf->compiler->as<VisualStudioCompiler>())
            {
                c->ForcedIncludeFiles().push_back(p.header);
                c->PrecompiledHeaderFilename() = pch_fn;
                c->PrecompiledHeaderFilename.input_dependency = true;
                c->PrecompiledHeader().use = p.header;
                c->PDBFilename = pdb_fn;
                c->PDBFilename.intermediate_file = false;
            }
            else if (auto c = sf->compiler->as<ClangClCompiler>())
            {
                c->ForcedIncludeFiles().push_back(p.header);
                c->PrecompiledHeaderFilename() = pch_fn;
                c->PrecompiledHeaderFilename.input_dependency = true;
                c->PrecompiledHeader().use = p.header;
            }
            else if (auto c = sf->compiler->as<ClangCompiler>())
            {
                c->ForcedIncludeFiles().push_back(p.header);
                //c->PrecompiledHeaderFilename() = pch_fn;
                //c->PrecompiledHeaderFilename.input_dependency = true;
                //c->PrecompiledHeader().use = p.header;
            }
            else if (auto c = sf->compiler->as<GNUCompiler>())
            {
                c->ForcedIncludeFiles().push_back(p.header);
                //c->PrecompiledHeaderFilename() = pch_fn;
                //c->PrecompiledHeaderFilename.input_dependency = true;
                //c->PrecompiledHeader().use = p.header;
            }
        }
    }

    *this += pch;

    if (auto sf = ((*this)[pch]).as<CPPSourceFile>())
    {
        sf->setOutputFile(obj_fn);
        if (auto c = sf->compiler->as<VisualStudioCompiler>())
        {
            c->ForcedIncludeFiles().push_back(p.header);
            c->PrecompiledHeaderFilename() = pch_fn;
            c->PrecompiledHeaderFilename.output_dependency = true;
            c->PrecompiledHeader().create = p.header;
            c->PDBFilename = pdb_fn;
            c->PDBFilename.intermediate_file = false;
            c->PDBFilename.output_dependency = true;
        }
        else if (auto c = sf->compiler->as<ClangClCompiler>())
        {
            c->ForcedIncludeFiles().push_back(p.header);
            c->PrecompiledHeaderFilename() = pch_fn;
            c->PrecompiledHeaderFilename.output_dependency = true;
            c->PrecompiledHeader().create = p.header;
        }
        else if (auto c = sf->compiler->as<ClangCompiler>())
        {
            c->ForcedIncludeFiles().push_back(p.header);
            //c->PrecompiledHeaderFilename() = pch_fn;
            //c->PrecompiledHeaderFilename.input_dependency = true;
            //c->PrecompiledHeader().create = p.header;
        }
        else if (auto c = sf->compiler->as<GNUCompiler>())
        {
            c->ForcedIncludeFiles().push_back(p.header);
            //c->PrecompiledHeaderFilename() = pch_fn;
            //c->PrecompiledHeaderFilename.input_dependency = true;
            //c->PrecompiledHeader().use = p.header;
        }
    }
}

NativeExecutedTarget &NativeExecutedTarget::operator=(const PrecompiledHeader &pch)
{
    addPrecompiledHeader(pch);
    return *this;
}

std::shared_ptr<builder::Command> NativeExecutedTarget::getCommand() const
{
    if (HeaderOnly && HeaderOnly.value())
        return nullptr;
    return getSelectedTool()->getCommand();
}

Commands NativeExecutedTarget::getGeneratedCommands() const
{
    Commands generated;

    const path def = NATIVE_TARGET_DEF_SYMBOLS_FILE;

    // add generated files
    for (auto &f : *this)
    {
        File p(f.first, *getSolution()->fs);
        if (!p.isGenerated())
            continue;
        if (f.first == def)
            continue;
        auto c = p.getFileRecord().getGenerator();
        generated.insert(c);
    }

    return generated;
}

Commands NativeExecutedTarget::getCommands() const
{
    Commands cmds;
    if (already_built)
        return cmds;

    const path def = NATIVE_TARGET_DEF_SYMBOLS_FILE;

    //DEBUG_BREAK_IF_STRING_HAS(pkg.ppath.toString(), "google.tensorflow.gen_proto_text_functions");

    // add generated files
    auto generated = getGeneratedCommands();

    // also from deps???
    // remove?
    // who is responsible for this? users? program?
    /*for (auto &d : Dependencies)
    {
        if (d->target == this)
            continue;
        if (d->Dummy)
            continue;

        for (auto &f : *(NativeExecutedTarget*)d->target)
        {
            File p(f.first);
            if (!p.isGenerated())
                continue;
            if (f.first.string().find(".symbols.def") != -1)
                continue;
            auto c = p.getFileRecord().getGenerator();
            generated.insert(c);
        }
    }*/

    if (HeaderOnly && HeaderOnly.value())
    {
        cmds.insert(generated.begin(), generated.end());
        return cmds;
    }

    // this source files
    {
        auto sd = normalize_path(SourceDir);
        auto bd = normalize_path(BinaryDir);
        auto bdp = normalize_path(BinaryPrivateDir);
        for (auto &f : gatherSourceFiles())
        {
            auto c = f->getCommand();
            c->args.insert(c->args.end(), f->args.begin(), f->args.end());

            // set fancy name
            if (/*!Local && */!IsConfig && !do_not_mangle_object_names)
            {
                auto p = normalize_path(f->file);
                if (bdp.size() < p.size() && p.find(bdp) == 0)
                {
                    auto n = p.substr(bdp.size());
                    c->name = "[" + pkg.target_name + "]/[bdir_pvt]" + n;
                }
                else if (bd.size() < p.size() && p.find(bd) == 0)
                {
                    auto n = p.substr(bd.size());
                    c->name = "[" + pkg.target_name + "]/[bdir]" + n;
                }
                if (sd.size() < p.size() && p.find(sd) == 0)
                {
                    String prefix;
                    if (f->compiler == Settings.Native.CCompiler)
                        prefix = "Building C object ";
                    else if (f->compiler == Settings.Native.CPPCompiler)
                        prefix = "Building CXX object ";
                    auto n = p.substr(sd.size());
                    if (!n.empty() && n[0] != '/')
                        n = "/" + n;
                    c->name = prefix + "[" + pkg.target_name + "]" + n;
                }
            }
            cmds.insert(c);
        }
    }

    // add generated files
    for (auto &cmd : cmds)
        cmd->dependencies.insert(generated.begin(), generated.end());
    cmds.insert(generated.begin(), generated.end());

    //LOG_DEBUG(logger, "Building target: " + pkg.ppath.toString());
    //move this somewhere

    //DEBUG_BREAK_IF_STRING_HAS(pkg.ppath.toString(), "self_builder");

    // this library, check if nothing to link
    if (auto c = getCommand())
    {
        c->dependencies.insert(cmds.begin(), cmds.end());

        File d(def, *getSolution()->fs);
        if (d.isGenerated())
        {
            auto g = d.getFileRecord().getGenerator();
            c->dependencies.insert(g);
            for (auto &c1 : cmds)
                g->dependencies.insert(c1);
            cmds.insert(g);
        }

        auto get_tgts = [this]()
        {
            TargetsSet deps;
            for (auto &d : Dependencies)
            {
                if (d->target.lock().get() == this)
                    continue;
                if (d->Dummy)
                    continue;

                if (d->IncludeDirectoriesOnly && !d->GenerateCommandsBefore)
                    continue;
                deps.insert(d->target.lock().get());
            }
            return deps;
        };

        // add dependencies on generated commands from dependent targets
        for (auto &l : get_tgts())
        {
            for (auto &c2 : ((NativeExecutedTarget*)l)->getGeneratedCommands())
            {
                for (auto &c : cmds)
                    c->dependencies.insert(c2);
            }
        }

        // link deps
        if (getSelectedTool() != Librarian.get())
        {
            for (auto &l : gatherDependenciesTargets())
            {
                if (auto c2 = l->getCommand())
                    c->dependencies.insert(c2);
            }

            // copy output dlls
            if (Local && Settings.Native.CopySharedLibraries)
            {
                for (auto &l : gatherAllRelatedDependencies())
                {
                    auto dt = ((NativeExecutedTarget*)l);
                    if (dt->Local)
                        continue;
                    if (dt->HeaderOnly.value())
                        continue;
                    if (Settings.Native.LibrariesType != LibraryType::Shared && !dt->isSharedOnly())
                        continue;
                    auto in = dt->getOutputFile();
                    auto o = (OutputDir.empty() ? getOutputFile().parent_path() : OutputDir) / in.filename();
                    SW_MAKE_EXECUTE_BUILTIN_COMMAND(copy_cmd, *this, "sw_copy_file");
                    copy_cmd->args.push_back(in.u8string());
                    copy_cmd->args.push_back(o.u8string());
                    copy_cmd->addInput(dt->getOutputFile());
                    copy_cmd->addOutput(o);
                    copy_cmd->dependencies.insert(c);
                    copy_cmd->name = "copy: " + normalize_path(o);
                    copy_cmd->maybe_unused = builder::Command::MU_ALWAYS;
                    cmds.insert(copy_cmd);
                }
            }

            // check circular, resolve if possible
            for (auto &d : CircularDependencies)
            {
                auto dt = ((NativeExecutedTarget*)d->target.lock().get());
                auto non_circ_cmd = dt->getSelectedTool()->getCommand();

                // one command must be executed after the second to free implib files from any compiler locks
                // we choose it based on ptr address
                //if (c < non_circ_cmd)
                c->dependencies.erase(non_circ_cmd);

                if (dt->CircularLinker)
                {
                    auto cd = dt->CircularLinker->getCommand();
                    c->dependencies.insert(cd);
                }
                //cmds.insert(cd);
            }

            if (CircularLinker)
            {
                // execute this command after unresolved (circular) cmd
                c->dependencies.insert(CircularLinker->getCommand());

                // we reset generator of implib from usual build command (c = getCommand()) to circular linker generator to overcome
                // automatic circular dependency generation in command.cpp
                //File(getImportLibrary()).getFileRecord().generator = CircularLinker->getCommand();
            }
        }

        cmds.insert(c);

        // copy deps
        /*auto cdb = std::make_shared<ExecuteCommand>(true, [p = pkg(), c = getConfig()]
        {
            auto &sdb = getServiceDatabase();
            auto f = sdb.getInstalledPackageFlags(p, c);
            f.set(pfBuilt, true);
            sdb.setInstalledPackageFlags(p, c, f);
        });
        cdb->dependencies.insert(c);
        cmds.insert(cdb);*/
    }

    if (auto evs = Events.getCommands(); !evs.empty())
    {
        for (auto &c : cmds)
            c->dependencies.insert(evs.begin(), evs.end());
        cmds.insert(evs.begin(), evs.end());
    }

    return cmds;
}

Files NativeExecutedTarget::getGeneratedDirs() const
{
    Files dirs;
    dirs.insert(BinaryDir);
    dirs.insert(BinaryPrivateDir);
    for (auto &f : *this)
    {
        File p(f.first, *getSolution()->fs);
        if (p.isGenerated())
        {
            auto d = p.getFileRecord().getGenerator()->getGeneratedDirs();
            dirs.insert(d.begin(), d.end());
        }
        if (!f.second)
            continue;
        auto d = f.second->getGeneratedDirs();
        dirs.insert(d.begin(), d.end());
    }
    dirs.insert(getOutputFile().parent_path());
    dirs.insert(getImportLibrary().parent_path());
    if (CircularLinker)
    {
        dirs.insert(CircularLinker->getOutputFile().parent_path());
        dirs.insert(CircularLinker->getImportLibrary().parent_path());
    }
    return dirs;
}

void NativeExecutedTarget::findSources()
{
    // We add root dir if we postponed resolving and iif it's a local package.
    // Downloaded package already appended root dir.
    //if (PostponeFileResolving && Local)
        //SourceDir /= RootDirectory;

    if (ImportFromBazel)
    {
        path bfn;
        for (auto &f : { "BUILD", "BUILD.bazel" })
        {
            if (fs::exists(SourceDir / f))
            {
                bfn = SourceDir / f;
                remove(SourceDir / f);
                break;
            }
        }
        if (bfn.empty())
            throw std::runtime_error("");

        auto b = read_file(bfn);
        auto f = bazel::parse(b);
        String project_name;
        if (!pkg.ppath.empty())
            project_name = pkg.ppath.back();
        auto add_files = [this, &f](const auto &n)
        {
            auto files = f.getFiles(BazelTargetName.empty() ? n : BazelTargetName, BazelTargetFunction);
            for (auto &f : files)
            {
                path p = f;
                if (check_absolute(p, true))
                    add(p);
            }
        };
        add_files(project_name);
        for (auto &n : BazelNames)
            add_files(n);
    }

    if (!already_built)
        resolve();

    // we autodetect even if already built
    if (!AutoDetectOptions || (AutoDetectOptions && AutoDetectOptions.value()))
        autoDetectOptions();
    //resolveRemoved();

    detectLicenseFile();
}

void NativeExecutedTarget::autoDetectOptions()
{
    // TODO: add dirs with first capital letter:
    // Include, Source etc.

    autodetect = true;

    // with stop string at the end
    static const Strings source_dir_names = { "src", "source", "sources", "lib", "library" };

    // gather things to check
    //bool sources_empty = gatherSourceFiles().empty();
    bool sources_empty = sizeKnown() == 0;
    bool idirs_empty = true;

    // idirs
    if (idirs_empty)
    {
        LOG_TRACE(logger, getPackage().target_name +  ": Autodetecting include dirs");

        if (fs::exists(SourceDir / "include"))
            Public.IncludeDirectories.insert(SourceDir / "include");
        else if (fs::exists(SourceDir / "includes"))
            Public.IncludeDirectories.insert(SourceDir / "includes");
        else if (!SourceDir.empty())
            Public.IncludeDirectories.insert(SourceDir);

        std::function<void(const Strings &)> autodetect_source_dir;
        autodetect_source_dir = [this, &autodetect_source_dir](const Strings &dirs)
        {
            const auto &current = dirs[0];
            const auto &next = dirs[1];
            if (fs::exists(SourceDir / current))
            {
                if (fs::exists(SourceDir / "include"))
                    Private.IncludeDirectories.insert(SourceDir / current);
                else if (fs::exists(SourceDir / "includes"))
                    Private.IncludeDirectories.insert(SourceDir / current);
                else
                    Public.IncludeDirectories.insert(SourceDir / current);
            }
            else
            {
                // now check next dir
                if (!next.empty())
                    autodetect_source_dir({ dirs.begin() + 1, dirs.end() });
            }
        };
        static Strings dirs = []
        {
            Strings dirs(source_dir_names.begin(), source_dir_names.end());
            // keep the empty entry at the end for autodetect_source_dir()
            if (dirs.back() != "")
                dirs.push_back("");
            return dirs;
        }();
        autodetect_source_dir(dirs);
    }

    // files
    if (sources_empty && !already_built)
    {
        LOG_TRACE(logger, getPackage().target_name + ": Autodetecting sources");

        bool added = false;
        if (fs::exists(SourceDir / "include"))
        {
            add("include/.*"_rr);
            added = true;
        }
        else if (fs::exists(SourceDir / "includes"))
        {
            add("includes/.*"_rr);
            added = true;
        }
        for (auto &d : source_dir_names)
        {
            if (fs::exists(SourceDir / d))
            {
                add(FileRegex(d, std::regex(".*"), true));
                added = true;
            }
        }
        if (!added)
        {
            // no include, source dirs
            // try to add all types of C/C++ program files to gather
            // regex means all sources in root dir (without slashes '/')

            auto escape_regex_symbols = [](const String &s)
            {
                return boost::replace_all_copy(s, "+", "\\+");
            };

            // iterate over languages: ASM, C, CPP, ObjC, ObjCPP
            // check that all exts is in languages!

            static const std::set<String> header_file_extensions{
            ".h",
            ".hh",
            ".hm",
            ".hpp",
            ".hxx",
            ".h++",
            ".H++",
            ".HPP",
            ".H",
            };

            static const std::set<String> source_file_extensions{
            ".c",
            ".cc",
            ".cpp",
            ".cxx",
            ".c++",
            ".C++",
            ".CPP",
            // Objective-C
            ".m",
            ".mm",
            ".C",
            };

            static const std::set<String> other_source_file_extensions{
            ".s",
            ".S",
            ".asm",
            ".ipp",
            ".inl",
            };

            for (auto &v : header_file_extensions)
                add(FileRegex(std::regex(".*\\" + escape_regex_symbols(v)), false));
            for (auto &v : source_file_extensions)
                add(FileRegex(std::regex(".*\\" + escape_regex_symbols(v)), false));
            for (auto &v : other_source_file_extensions)
                add(FileRegex(std::regex(".*\\" + escape_regex_symbols(v)), false));
        }
    }
}

void NativeExecutedTarget::detectLicenseFile()
{
    // license
    auto check_license = [this](path name, String *error = nullptr)
    {
        auto license_error = [&error](auto &err)
        {
            if (error)
            {
                *error = err;
                return false;
            }
            throw std::runtime_error(err);
        };
        if (!name.is_absolute())
            name = SourceDir / name;
        if (!fs::exists(name))
            return license_error("license does not exists");
        if (fs::file_size(name) > 512_KB)
            return license_error("license is invalid (should be text/plain and less than 512 KB)");
        return true;
    };

    if (!Local)
    {
        if (!LicenseFilename.empty())
        {
            if (check_license(LicenseFilename))
                add(LicenseFilename);
        }
        else
        {
            String error;
            auto try_license = [&error, &check_license, this](auto &lic)
            {
                if (check_license(lic, &error))
                {
                    add(lic);
                    return true;
                }
                return false;
            };
            if (try_license("LICENSE") ||
                try_license("COPYING") ||
                try_license("Copying.txt") ||
                try_license("LICENSE.txt") ||
                try_license("license.txt") ||
                try_license("LICENSE.md"))
                (void)error;
        }
    }
}

bool NativeExecutedTarget::prepare()
{
    //DEBUG_BREAK_IF_STRING_HAS(pkg.ppath.toString(), "amazon.aws.sdk.core");

    /*{
        auto is_changed = [this](const path &p)
        {
            if (p.empty())
                return false;
            return !(fs::exists(p) && File(p, *getSolution()->fs).isChanged());
        };

        auto i = getImportLibrary();
        auto o = getOutputFile();

        if (!is_changed(i) && !is_changed(o))
        {
            std::cout << "skipping prepare for: " << pkg.toString() << "\n";
            return false;
        }
    }*/

    switch (prepare_pass)
    {
    case 0:
        //restoreSourceDir();
        RETURN_PREPARE_PASS;
    case 1:
    {
        LOG_TRACE(logger, "Preparing target: " + pkg.ppath.toString());

        findSources();

        // make sure we always use absolute paths
        BinaryDir = fs::absolute(BinaryDir);
        BinaryPrivateDir = fs::absolute(BinaryPrivateDir);

        // add pvt binary dir
        IncludeDirectories.insert(BinaryPrivateDir);

        // always add bdir to include dirs
        Public.IncludeDirectories.insert(BinaryDir);

        HeaderOnly = gatherObjectFilesWithoutLibraries().empty();

        for (auto &f : *this)
        {
            if (f.second->created && !f.second->skip/* && !isRemoved(f.first)*/)
            {
                auto ba = ((NativeSourceFile*)f.second.get())->BuildAs;
                if (ba != NativeSourceFile::BasedOnExtension)
                {
                    f.second = languages[(LanguageType)ba]->clone()->createSourceFile(f.first, this);
                }
            }
        }

        if (!Local)
        {
            // activate later?
            /*auto p = pkg;
            auto c = getConfig();
            auto &sdb = getServiceDatabase();
            auto f = sdb.getInstalledPackageFlags(p, c);
            if (already_built)
            {
                HeaderOnly = f[pfHeaderOnly];
            }
            else if (HeaderOnly.value())
            {
                f.set(pfHeaderOnly, HeaderOnly.value());
                sdb.setInstalledPackageFlags(p, c, f);
            }*/
        }

        // default macros
        if (Settings.TargetOS.Type == OSType::Windows)
        {
            Definitions["SW_EXPORT"] = "__declspec(dllexport)";
            Definitions["SW_IMPORT"] = "__declspec(dllimport)";
        }
        else
        {
            Definitions["SW_EXPORT"] = "__attribute__ ((visibility (\"default\")))";
            Definitions["SW_IMPORT"] = "__attribute__ ((visibility (\"default\")))";
        }
        Definitions["SW_STATIC="];

        clearGlobCache();

        //if (HeaderOnly && !HeaderOnly.value())
        //LOG_INFO(logger, "compiling target: " + pkg.ppath.toString());
    }
    RETURN_PREPARE_PASS;
    case 2:
    {
        // resolve unresolved deps
        // not on the first stage!
        TargetOptionsGroup::iterate<WithoutSourceFileStorage, WithNativeOptions>(
            [this](auto &v, auto &s)
        {
            for (auto &d : v.Dependencies)
            {

                // we do this for every dependency no matter it has d->target set
                // because importing from different dlls and selecting specific packages will result in
                // incorrect d->target pointers
                /*auto i = solution->getChildren().find(d->getPackage());
                if (i == solution->getChildren().end())
                    throw std::logic_error("Unresolved package on stage 1: " + d->getPackage().target_name);
                d->target = (NativeTarget *)i->second.get();*/

                /*if (d->target != nullptr)
                    continue;*/

                for (auto &[pp, t] : solution->getChildren())
                {
                    if (d->getPackage().canBe(t->getPackage()))
                    {
                        d->target = std::static_pointer_cast<NativeTarget>(t);
                        break;
                    }
                }
                if (!d->target.lock())
                    throw std::logic_error("Unresolved package on stage 1: " + d->getPackage().toString());
            }
        });
    }
    RETURN_PREPARE_PASS;
    case 3:
    // inheritance
    {
        struct H
        {
            size_t operator()(const DependencyPtr &p) const
            {
                return std::hash<Dependency>()(*p);
            }
        };
        struct L
        {
            size_t operator()(const DependencyPtr &p1, const DependencyPtr &p2) const
            {
                return (*p1) < (*p2);
            }
        };

        //DEBUG_BREAK_IF_STRING_HAS(pkg.toString(), "protoc-");

        // why such sorting (L)?
        //std::unordered_map<DependencyPtr, InheritanceType, H> deps;
        std::map<DependencyPtr, InheritanceType, L> deps;
        //std::map<DependencyPtr, InheritanceType> deps;
        std::vector<DependencyPtr> deps_ordered;

        // set our initial deps
        TargetOptionsGroup::iterate<WithoutSourceFileStorage, WithNativeOptions>(
            [this, &deps, &deps_ordered](auto &v, auto &s)
        {
            //DEBUG_BREAK_IF_STRING_HAS(pkg.ppath.toString(), "sw.server.protos");

            for (auto &d : v.Dependencies)
            {
                if (d->target.lock().get() == this)
                    continue;
                if (d->Dummy)
                    continue;

                deps.emplace(d, s.Inheritance);
                deps_ordered.push_back(d);
            }
        });

        while (1)
        {
            bool new_dependency = false;
            auto deps2 = deps;
            for (auto &[d, _] : deps2)
            {
                if (d->target.lock() == nullptr)
                {
                    throw std::logic_error("Unresolved package on stage 2: " + d->package.toString());
                    /*LOG_ERROR(logger, "Unresolved package on stage 2: " + d->package.toString() + ". Resolving inplace");
                    auto id = d->package.resolve();
                    d->target = std::static_pointer_cast<NativeTarget>(getSolution()->getTargetPtr(id));*/
                }

                // iterate over child deps
                (*(NativeExecutedTarget*)d->target.lock().get()).TargetOptionsGroup::iterate<WithoutSourceFileStorage, WithNativeOptions>(
                    [this, &new_dependency, &deps, &d, &deps_ordered](auto &v, auto &s)
                {
                    // nothing to do with private inheritance
                    if (s.Inheritance == InheritanceType::Private)
                        return;

                    for (auto &d2 : v.Dependencies)
                    {
                        if (d2->target.lock().get() == this)
                            continue;
                        if (d2->Dummy)
                            continue;

                        if (s.Inheritance == InheritanceType::Protected && !hasSameParent(d2->target.lock().get()))
                            continue;

                        auto copy = std::make_shared<Dependency>(*d2);
                        auto[i, inserted] = deps.emplace(copy,
                            s.Inheritance == InheritanceType::Interface ?
                            InheritanceType::Public : s.Inheritance
                        );
                        if (inserted)
                            deps_ordered.push_back(copy);

                        // include directories only handling
                        auto di = i->first;
                        if (inserted)
                        {
                            // new dep is added
                            if (d->IncludeDirectoriesOnly)
                            {
                                // if we inserted 3rd party dep (d2=di) of idir_only dep (d),
                                // we mark it always as idir_only
                                di->IncludeDirectoriesOnly = true;
                            }
                            else
                            {
                                // otherwise we keep idir_only flag as is
                            }
                            new_dependency = true;
                        }
                        else
                        {
                            // we already have this dep
                            if (d->IncludeDirectoriesOnly)
                            {
                                // left as is if parent (d) idir_only
                            }
                            else
                            {
                                // if parent dep is not idir_only, then we choose whether to build dep
                                if (d2->IncludeDirectoriesOnly)
                                {
                                    // left as is if d2 idir_only
                                }
                                else
                                {
                                    if (di->IncludeDirectoriesOnly)
                                    {
                                        // also mark as new dependency (!) if processing changed for it
                                        new_dependency = true;
                                    }
                                    // if d2 is not idir_only, we set so for di
                                    di->IncludeDirectoriesOnly = false;
                                }
                            }
                        }
                    }
                });
            }

            if (!new_dependency)
            {
                for (auto &d : deps_ordered)
                    Dependencies.insert(deps.find(d)->first);
                break;
            }
        }

        // Here we check if some deps are not included in solution target set (children).
        // They could be in dummy children, because of different target scope, not listed on software network,
        // but still in use.
        // We add them back to children.
        // Example: helpers, small tools, code generators.
        {
            auto &c = getSolution()->children;
            auto &dc = getSolution()->dummy_children;
            for (auto &d2 : Dependencies)
            {
                if (d2->target.lock() && c.find(d2->target.lock()->pkg) == c.end() && dc.find(d2->target.lock()->pkg) != dc.end())
                {
                    c[d2->target.lock()->pkg] = dc[d2->target.lock()->pkg];

                    // such packages are not completely independent
                    // they share same source dir (but not binary?) with parent etc.
                    d2->target.lock()->SourceDir = SourceDir;
                }
            }
        }

    }
    RETURN_PREPARE_PASS;
    case 4:
    {
        // merge self
        merge();

        // merge deps' stuff
        for (auto &d : Dependencies)
        {
            if (d->Dummy)
                continue;

            GroupSettings s;
            //s.merge_to_self = false;
            merge(*(NativeExecutedTarget*)d->target.lock().get(), s);
        }
    }
    RETURN_PREPARE_PASS;
    case 5:
    {
        auto files = gatherSourceFiles();

        // copy headers to install dir
        if (!InstallDirectory.empty() && !fs::exists(SourceDir / InstallDirectory))
        {
            auto d = SourceDir / InstallDirectory;
            fs::create_directories(d);
            for (auto &[p, fp] : *this)
            {
                File f(p, *getSolution()->fs);
                if (f.isGenerated())
                    continue;
                // is_header_ext()
                const auto e = f.file.extension();
                if (e == ".h" || e == ".hpp" || e == ".hxx")
                    fs::copy_file(f.file, d / f.file.filename());
            }
        }

        // before merge
        if (Settings.Native.ConfigurationType != ConfigurationType::Debug)
            *this += "NDEBUG"_d;
        // allow to other compilers?
        else if (Settings.Native.CompilerType == CompilerType::MSVC)
            *this += "_DEBUG"_d;

        // merge file compiler options with target compiler options
        for (auto &f : files)
        {
            // set everything before merge!
            f->compiler->merge(*this);

            if (auto c = f->compiler->as<VisualStudioCompiler>())
            {
                switch (Settings.Native.ConfigurationType)
                {
                case ConfigurationType::Debug:
                    c->RuntimeLibrary = vs::RuntimeLibraryType::MultiThreadedDLLDebug;
                    c->Optimizations().Disable = true;
                    break;
                case ConfigurationType::Release:
                    c->Optimizations().FastCode = true;
                    break;
                case ConfigurationType::ReleaseWithDebugInformation:
                    c->Optimizations().FastCode = true;
                    break;
                case ConfigurationType::MinimalSizeRelease:
                    c->Optimizations().SmallCode = true;
                    break;
                }
                c->CPPStandard = CPPVersion;

                if (IsConfig && c->PrecompiledHeader && c->PrecompiledHeader().create)
                {
                    // why?
                    c->IncludeDirectories.erase(BinaryDir);
                    c->IncludeDirectories.erase(BinaryPrivateDir);
                }
            }
            else if (auto c = f->compiler->as<ClangClCompiler>())
            {
                if (Settings.Native.ConfigurationType == ConfigurationType::Debug)
                    c->RuntimeLibrary = vs::RuntimeLibraryType::MultiThreadedDLLDebug;
                c->CPPStandard = CPPVersion;

                if (IsConfig && c->PrecompiledHeader && c->PrecompiledHeader().create)
                {
                    // why?
                    c->IncludeDirectories.erase(BinaryDir);
                    c->IncludeDirectories.erase(BinaryPrivateDir);
                }
            }
            else if (auto c = f->compiler->as<GNUCompiler>())
            {
                switch (Settings.Native.ConfigurationType)
                {
                case ConfigurationType::Debug:
                    c->GenerateDebugInfo = true;
                    break;
                case ConfigurationType::Release:
                    break;
                case ConfigurationType::ReleaseWithDebugInformation:
                    break;
                case ConfigurationType::MinimalSizeRelease:
                    break;
                }
                c->CPPStandard = CPPVersion;
            }
        }

        // setup pch deps
        {
            // gather pch
            struct PCH
            {
                NativeSourceFile *create = nullptr;
                std::set<NativeSourceFile *> use;
            };

            std::map<path /* pch file */, std::map<path, PCH> /* pch hdr */> pchs;
            for (auto &f : files)
            {
                if (auto c = f->compiler->as<VisualStudioCompiler>())
                {
                    if (c->PrecompiledHeader().create)
                        pchs[c->PrecompiledHeaderFilename()][c->PrecompiledHeader().create.value()].create = f;
                    else if (c->PrecompiledHeader().use)
                        pchs[c->PrecompiledHeaderFilename()][c->PrecompiledHeader().use.value()].use.insert(f);
                }
            }

            // set deps
            for (auto &pch : pchs)
            {
                // groups
                for (auto &g : pch.second)
                {
                    for (auto &f : g.second.use)
                        f->dependencies.insert(g.second.create);
                }
            }
        }

        // linker setup - already set up
        //setOutputFile();

        // legit?
        getSelectedTool()->merge(*this);

        // pdb
        if (auto c = getSelectedTool()->as<VisualStudioLinker>())
        {
            c->GenerateDebugInfo = c->GenerateDebugInfo() ||
                Settings.Native.ConfigurationType == ConfigurationType::Debug ||
                Settings.Native.ConfigurationType == ConfigurationType::ReleaseWithDebugInformation;
            if (c->GenerateDebugInfo() && c->PDBFilename.empty())
            {
                auto f = getOutputFile();
                f = f.parent_path() / f.filename().stem();
                f += ".pdb";
                c->PDBFilename = f;// BinaryDir.parent_path() / "obj" / (pkg.ppath.toString() + ".pdb");
            }

            if (Linker->Type == LinkerType::LLD)
            {
                if (c->GenerateDebugInfo)
                    c->InputFiles().insert("msvcrtd.lib");
                else
                    c->InputFiles().insert("msvcrt.lib");
            }
        }

        // export all symbols
        if (ExportAllSymbols && Settings.TargetOS.Type == OSType::Windows && getSelectedTool() == Linker.get())
        {
            const path def = NATIVE_TARGET_DEF_SYMBOLS_FILE;
            Files objs;
            for (auto &f : files)
                objs.insert(f->output.file);
            SW_MAKE_EXECUTE_BUILTIN_COMMAND_AND_ADD(c, *this, "sw_create_def_file");
            c->args.push_back(def.u8string());
            c->push_back(objs);
            c->addInput(objs);
            c->addOutput(def);
            add(def);
        }

        // add def file to linker
        if (auto VSL = getSelectedTool()->as<VisualStudioLibraryTool>())
        {
            for (auto &[p, f] : *this)
            {
                if (!f->skip && p.extension() == ".def")
                {
                    VSL->DefinitionFile = p;
                    HeaderOnly = false;
                }
            }
        }
    }
    RETURN_PREPARE_PASS;
    case 6:
    {
        // add link libraries from deps
        if (!HeaderOnly.value() && getSelectedTool() != Librarian.get())
        {
            String s;
            for (auto &d : Dependencies)
            {
                if (d->target.lock().get() == this)
                    continue;
                if (d->Dummy)
                    continue;
                if (d->IncludeDirectoriesOnly)
                    continue;

                auto dt = ((NativeExecutedTarget*)d->target.lock().get());

                for (auto &d2 : dt->Dependencies)
                {
                    if (d2->target.lock().get() != this)
                        continue;
                    if (d2->IncludeDirectoriesOnly)
                        continue;

                    CircularDependencies.insert(d.get());
                }

                if (!CircularDependencies.empty())
                {
                    CircularLinker = std::static_pointer_cast<NativeLinker>(getSelectedTool()->clone());

                    // set to temp paths
                    auto o = IsConfig;
                    IsConfig = true;
                    CircularLinker->setOutputFile(getOutputFileName(getOutputDir()));
                    CircularLinker->setImportLibrary(getOutputFileName(getUserDirectories().storage_dir_lib));
                    IsConfig = o;

                    if (auto c = CircularLinker->as<VisualStudioLinker>())
                    {
                        c->Force = vs::ForceType::Unresolved;
                    }
                }

                if (!dt->HeaderOnly.value() && !d->IncludeDirectoriesOnly)
                    LinkLibraries.push_back(d.get()->target.lock()->getImportLibrary());

                s += d.get()->target.lock()->pkg.ppath.toString();
                if (d->IncludeDirectoriesOnly)
                    s += ": i";
                s += "\n";
            }
            if (!s.empty())
                write_file(BinaryDir.parent_path() / "deps.txt", s);
        }
    }
    RETURN_PREPARE_PASS;
    case 7:
    {
        // linker setup
#ifndef _WIN32
        /*if (Linker)
        {
            auto libs = gatherLinkLibraries();
            Linker->setLinkLibraries(libs);
        }*/
#endif
        auto obj = gatherObjectFilesWithoutLibraries();
        auto O1 = gatherLinkLibraries();

        if (CircularLinker)
        {
            // O1 -= Li
            for (auto &d : CircularDependencies)
                O1.erase(std::remove(O1.begin(), O1.end(), d->target.lock()->getImportLibrary()), O1.end());

            // CL1 = O1
            CircularLinker->setInputLibraryDependencies(O1);

            // O1 += CLi
            for (auto &d : CircularDependencies)
            {
                if (d->target.lock() && ((NativeExecutedTarget*)d->target.lock().get())->CircularLinker)
                    O1.push_back(((NativeExecutedTarget*)d->target.lock().get())->CircularLinker->getImportLibrary());
            }

            // prepare command here to prevent races
            CircularLinker->getCommand();
        }

        getSelectedTool()->setObjectFiles(obj);
        getSelectedTool()->setInputLibraryDependencies(O1);
    }
    break;
    }

    return false;
}

bool NativeExecutedTarget::prepareLibrary(LibraryType Type)
{
    switch (prepare_pass)
    {
    case 1:
    {
        auto set_api = [this, &Type](const String &api)
        {
            if (api.empty())
                return;

            if (Settings.TargetOS.Type == OSType::Windows)
            {
                if (Type == LibraryType::Shared)
                {
                    Private.Definitions[api] = "SW_EXPORT";
                    Interface.Definitions[api] = "SW_IMPORT";
                }
                else if (ExportIfStatic)
                {
                    Public.Definitions[api] = "SW_EXPORT";
                }
                else
                {
                    Public.Definitions[api + "="];
                }
            }
            else
            {
                if (Type == LibraryType::Shared || ExportIfStatic)
                {
                    Public.Definitions[api] = "SW_EXPORT";
                }
                else
                {
                    Public.Definitions[api + "="];
                }
            }

            Definitions[api + "_EXTERN="];
            Interface.Definitions[api + "_EXTERN"] = "extern";
        };

        if (Type == LibraryType::Shared)
            Definitions["CPPAN_SHARED_BUILD"];
        else if (Type == LibraryType::Static)
            Definitions["CPPAN_STATIC_BUILD"];

        set_api(ApiName);
        for (auto &a : ApiNames)
            set_api(a);
    }
    break;
    }

    return NativeExecutedTarget::prepare();
}

void NativeExecutedTarget::initLibrary(LibraryType Type)
{
    if (Type == LibraryType::Shared)
    {
        if (Linker->Type == LinkerType::MSVC)
        {
            // set machine to target os arch
            auto L = Linker->as<VisualStudioLinker>();
            L->Dll = true;
            // probably setting dll must affect .dll extension automatically
            L->Extension = ".dll";
        }
        else if (Linker->Type == LinkerType::GNU)
        {
            auto L = Linker->as<GNULinker>();
            L->Extension = ".so";
            L->SharedObject = true;
        }
        if (Settings.TargetOS.Type == OSType::Windows)
            Definitions["_WINDLL"];
    }
    else
    {
        SelectedTool = Librarian.get();
    }
}

void NativeExecutedTarget::configureFile(path from, path to, ConfigureFlags flags)
{
    // before resolving
    if (!to.is_absolute())
        to = BinaryDir / to;
    File(to, *getSolution()->fs).getFileRecord().setGenerated();

    if (PostponeFileResolving)
        return;

    if (!from.is_absolute())
    {
        if (fs::exists(SourceDir / from))
            from = SourceDir / from;
        else if (fs::exists(BinaryDir / from))
            from = BinaryDir / from;
        else
            throw std::runtime_error("Package: " + pkg.target_name + ", file not found: " + from.string());
    }

    // we really need ExecuteCommand here!!!
    //auto c = std::make_shared<DummyCommand>();// ([this, from, to, flags]()
    {
        configureFile1(from, to, flags);
    }//);
    //c->addInput(from);
    //c->addOutput(to);

    //if ((int)flags & (int)ConfigureFlags::AddToBuild)
        //Public.add(to);
}

void NativeExecutedTarget::configureFile1(const path &from, const path &to, ConfigureFlags flags) const
{
    static const std::regex cmDefineRegex(R"xxx(#cmakedefine[ \t]+([A-Za-z_0-9]*)[^\r\n]*?[\r\n])xxx");
    static const std::regex cmDefine01Regex(R"xxx(#cmakedefine01[ \t]+([A-Za-z_0-9]*)[^\r\n]*?[\r\n])xxx");
    static const std::regex mesonDefine(R"xxx(#mesondefine[ \t]+([A-Za-z_0-9]*)[^\r\n]*?[\r\n])xxx");
    static const std::regex cmAtVarRegex("@([A-Za-z_0-9/.+-]+)@");
    static const std::regex cmNamedCurly("\\$\\{([A-Za-z0-9/_.+-]+)\\}");

    static const std::set<std::string> offValues{
        "","OFF","0","NO","FALSE","N","IGNORE",
    };

    auto s = read_file(from);

    if ((int)flags & (int)ConfigureFlags::CopyOnly)
    {
        fileWriteOnce(to, s);
        return;
    }

    auto find_repl = [this](const auto &key) -> std::string
    {
        auto v = Variables.find(key);
        if (v != Variables.end())
            return v->second;
        auto d = Definitions.find(key);
        if (d != Definitions.end())
            return d->second;
        return String();
    };

    std::smatch m;

    // @vars@
    while (std::regex_search(s, m, cmAtVarRegex) ||
        std::regex_search(s, m, cmNamedCurly))
    {
        auto repl = find_repl(m[1].str());
        s = m.prefix().str() + repl + m.suffix().str();
    }

    // #cmakedefine
    while (std::regex_search(s, m, cmDefineRegex) || std::regex_search(s, m, mesonDefine))
    {
        auto repl = find_repl(m[1].str());
        if (offValues.find(boost::to_upper_copy(repl)) != offValues.end())
            s = m.prefix().str() + "/* #undef " + m[1].str() + " */" + "\n" + m.suffix().str();
        else
            s = m.prefix().str() + "#define " + m[1].str() + " " + repl + "\n" + m.suffix().str();
    }

    // #cmakedefine01
    while (std::regex_search(s, m, cmDefine01Regex))
    {
        auto repl = find_repl(m[1].str());
        if (offValues.find(boost::to_upper_copy(repl)) != offValues.end())
            s = m.prefix().str() + "#define " + m[1].str() + " 0" + "\n" + m.suffix().str();
        else
            s = m.prefix().str() + "#define " + m[1].str() + " 1" + "\n" + m.suffix().str();
    }

    //for (auto &[k, v] : Variables)
    //boost::replace_all(s, "@" + k + "@", v);

    // handle ${k} vars
    // remove the rest of variables
    //s = std::regex_replace(s, r, "");

    fileWriteOnce(to, s);
}

void NativeExecutedTarget::removeFile(const path &fn)
{
    path p = fn;
    check_absolute(p, true);
    operator-=(fn);
    Target::removeFile(p);
}

void NativeExecutedTarget::setChecks(const String &name)
{
    auto i = solution->Checks.sets.find(name);
    if (i == solution->Checks.sets.end())
        return;
    for (auto &[k, c] : i->second.checks)
    {
        auto d = c->getDefinition(k);
        const auto v = c->Value;
        // make private?
        // remove completely?
        if (d)
        {
            //Public.Definitions[d.value()];
            add(Definition{ d.value() });

            for (auto &p : c->Prefixes)
                add(Definition{ p + d.value() });
            for (auto &d2 : c->Definitions)
            {
                for (auto &p : c->Prefixes)
                    Definitions[p + d2] = v;
            }
        }
        Variables[k] = v;

        for (auto &p : c->Prefixes)
            Variables[p + k] = v;
        for (auto &d2 : c->Definitions)
        {
            for (auto &p : c->Prefixes)
                Variables[p + d2] = v;
        }
    }
}

bool ExecutableTarget::prepare()
{
    switch (prepare_pass)
    {
    case 1:
    {
        auto set_api = [this](const String &api)
        {
            if (api.empty())
                return;
            if (Settings.TargetOS.Type == OSType::Windows)
            {
                Private.Definitions[api] = "SW_EXPORT";
                Interface.Definitions[api] = "SW_IMPORT";
            }
            else
            {
                Public.Definitions[api] = "SW_EXPORT";
            }
        };

        Definitions["CPPAN_EXECUTABLE"];

        set_api(ApiName);
        for (auto &a : ApiNames)
            set_api(a);

        if (Linker->Type == LinkerType::MSVC)
        {
            auto L = Linker->as<VisualStudioLinker>();
            L->Subsystem = vs::Subsystem::Console;
        }
    }
    break;
    }

    return NativeExecutedTarget::prepare();
}

path ExecutableTarget::getOutputDir() const
{
    return getUserDirectories().storage_dir_bin;
}

LibraryTarget::LibraryTarget(LanguageType L)
    : NativeExecutedTarget(L)
{
}

bool LibraryTarget::prepare()
{
    return prepareLibrary(Settings.Native.LibrariesType);
}

void LibraryTarget::init()
{
    NativeExecutedTarget::init();
    initLibrary(Settings.Native.LibrariesType);
}

StaticLibraryTarget::StaticLibraryTarget(LanguageType L)
    : LibraryTargetBase(L)
{
}

void StaticLibraryTarget::init()
{
    if (!Local)
    {
        // we re-use dirs only for non local projects
        // local projects put all files into config folders
        //Settings.Native.LibrariesType = LibraryType::Static;
    }

    NativeExecutedTarget::init();
    initLibrary(LibraryType::Static);
}

SharedLibraryTarget::SharedLibraryTarget(LanguageType L)
    : LibraryTargetBase(L)
{
}

void SharedLibraryTarget::init()
{
    if (!Local)
    {
        // we re-use dirs only for non local projects
        // local projects put all files into config folders
        //Settings.Native.LibrariesType = LibraryType::Shared;
    }
    NativeExecutedTarget::init();
    initLibrary(LibraryType::Shared);
}

}
