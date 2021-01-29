// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "entry_point.h"

#include "build.h"
#include "command.h"
#include "driver.h"
#include "input.h"
#include "rule.h"
#include "suffix.h"
#include "target/all.h"
#include "sw_check_abi_version.h"

#include <sw/core/build.h>
#include <sw/core/sw_context.h>
#include <sw/core/input_database.h>
#include <sw/core/specification.h>
#include <sw/manager/storage.h>

#include <boost/algorithm/string.hpp>
#include <boost/dll.hpp>
#include <nlohmann/json.hpp>
#include <primitives/emitter.h>
#include <primitives/sw/settings_program_name.h>
#include <primitives/symbol.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "entry_point");

#define SW_DRIVER_NAME "org.sw." PACKAGE_NAME "-" PACKAGE_VERSION
#define IMPORT_LIBRARY "sw.dll"

namespace sw
{

/*static String getSwPchContents()
{
#define DECLARE_TEXT_VAR_BEGIN(x) const uint8_t _##x[] = {
#define DECLARE_TEXT_VAR_END(x) }; const std::string x = (const char *)&_##x[0];
    DECLARE_TEXT_VAR_BEGIN(v)
#include <sw.pp.emb>
    DECLARE_TEXT_VAR_END(v);
    return v;
}*/

static bool isDriverDllBuild()
{
#ifdef SW_DRIVER_SHARED_BUILD
    return true;
#else
    return false;
#endif
}

static bool isDriverStaticBuild()
{
    return !isDriverDllBuild();
}

static String getCurrentModuleId()
{
    return shorten_hash(sha1(getProgramName()), 6);
}

static path getPchDir(const Build &b)
{
    return b.getContext().getLocalStorage().storage_dir_tmp / "pch" / std::to_string(sw_get_module_abi_version());
}

static path getImportFilePrefix(const Build &b)
{
    static const String pch_ver = "1";
    String h;
    // takes a lot of disk
    // also sometimes it causes crashes or infinite loops
    //h = "." + b.getContext().getHostSettings().getHash();
    return getPchDir(b) / ("sw." + pch_ver + h + "." + getCurrentModuleId());
}

static path getImportDefinitionsFile(const Build &b)
{
    return getImportFilePrefix(b) += ".def";
}

static path getImportLibraryFile(const Build &b)
{
    return getImportFilePrefix(b) += ".lib";
}

static String getDepsSuffix(PrepareConfig &pc, NativeCompiledTarget &t, const std::unordered_set<UnresolvedPackageName> &deps)
{
    std::set<String> sdeps;
    SW_UNIMPLEMENTED;
    //for (auto &d : t.getDependencies())
        //sdeps.insert(d->getUnresolvedPackage().toString());
    for (auto &d : deps)
        sdeps.insert(d.toString());
    String h;
    String s;
    for (auto &d : sdeps)
        s += d;
    s += std::to_string(pc.lang);
    h = "." + shorten_hash(blake2b_512(s), 6);
    return h;
}

static path getImportPchFile(PrepareConfig &pc, NativeCompiledTarget &t, const std::unordered_set<UnresolvedPackageName> &deps)
{
    // we create separate pch for different target deps
    auto h = getDepsSuffix(pc, t, deps);
    return getImportFilePrefix(t.getSolution()) += h + ".cpp";
}

#ifdef _WIN32
static Strings getExports(HMODULE lib)
{
    auto header = (PIMAGE_NT_HEADERS)((BYTE *)lib + ((PIMAGE_DOS_HEADER)lib)->e_lfanew);
    auto exports = (PIMAGE_EXPORT_DIRECTORY)((BYTE *)lib + header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
    auto names = (int*)((uint64_t)lib + exports->AddressOfNames);
    Strings syms;
    for (DWORD i = 0; i < exports->NumberOfNames; i++)
    {
        const char *n = (const char *)lib + names[i];
        syms.push_back(n);
    }
    return syms;
}
#endif

static CommandStorage &getDriverCommandStorage(const Build &b)
{
    return b.getMainBuild().getCommandStorage(b.getContext().getLocalStorage().storage_dir_tmp / "db" / "service");
}

static PackagePath getSelfTargetName(Build &b, const FilesSorted &files)
{
    auto h = b.module_data.getSettings().getHashString();
    for (auto &fn : files)
        h += to_string(normalize_path(fn));
    h = shorten_hash(blake2b_512(h), 6);
    return "loc.sw.self." + h;
}

static auto getDriverDep()
{
    return std::make_shared<Dependency>(UnresolvedPackageName(SW_DRIVER_NAME));
}

static void addDeps(Build &solution, NativeCompiledTarget &lib)
{
    lib += "pub.egorpugin.primitives.templates"_dep; // for SW_RUNTIME_ERROR

    // uncomment when you need help
    //lib += "pub.egorpugin.primitives.source"_dep;
    //lib += "pub.egorpugin.primitives.version"_dep;
    lib += "pub.egorpugin.primitives.command"_dep;
    lib += "pub.egorpugin.primitives.filesystem"_dep;

    auto d = lib + UnresolvedPackageName(SW_DRIVER_NAME);
    d->IncludeDirectoriesOnly = true;
}

// add Dirs?
static path getDriverIncludeDir(Build &solution, Target &lib)
{
    return lib.getFile(getDriverDep()) / "src";
}

static path getSwDir()
{
    return path("sw") / "driver";
}

static path getSwHeader()
{
    return getSwDir() / "sw.h";
}

static path getSw1Header()
{
    return getSwDir() / "sw1.h";
}

static path getSwCheckAbiVersionHeader()
{
    return getSwDir() / "sw_check_abi_version.h";
}

static path getPackageHeader(const LocalPackage &p, const UnresolvedPackageName &up)
{
    // TODO: add '#pragma sw driver ...' ?

    // depends on upkg, not on pkg!
    // because p is constant, but up might differ
    SW_UNIMPLEMENTED;
    /*auto h = p.getDirSrc() / "gen" / ("pkg_header_" + shorten_hash(sha1(up.toString()), 6) + ".h");
    //if (fs::exists(h))
    //return h;
    auto cfg = p.getDirSrc2() / "sw.cpp";
    auto f = read_file(cfg);
    std::smatch m;
    // replace with while?
    const char on[] = "#pragma sw header on";
    auto pos = f.find(on);
    if (pos == f.npos)
        throw SW_RUNTIME_ERROR("No header for package: " + p.toString());
    auto prefix = f.substr(0, pos);
    auto nlines = std::count(prefix.begin(), prefix.end(), '\n') + 2;
    f = f.substr(pos + sizeof(on));
    pos = f.find("#pragma sw header off");
    if (pos == f.npos)
        throw SW_RUNTIME_ERROR("No end in header for package: " + p.toString());
    f = f.substr(0, pos);
    //static const std::regex r_header("#pragma sw header on(.*)#pragma sw header off");
    //if (std::regex_search(f, m, r_header))
    {
        primitives::Emitter ctx;
        ctx.addLine("#pragma once");
        ctx.addLine();
        ctx.addLine("#line " + std::to_string(nlines) + " \"" + to_string(normalize_path(cfg)) + "\"");

        primitives::Emitter prefix;
        auto ins_pre = "#pragma sw header insert prefix";
        if (f.find(ins_pre) != f.npos)
            boost::replace_all(f, ins_pre, prefix.getText());
        else
            ctx += prefix;

        ctx.addLine(f);
        ctx.addLine();

        write_file_if_different(h, ctx.getText());
    }
    return h;*/
}

static
std::pair<FilesOrdered, std::unordered_set<UnresolvedPackageName>>
getFileDependencies(SwBuild &b, const path &p, std::set<size_t> &gns)
{
    std::unordered_set<UnresolvedPackageName> udeps;
    FilesOrdered headers;

    auto f = read_file(p);
#ifdef _WIN32
    static const std::regex r_pragma("^#pragma +sw +require +(\\S+)( +(\\S+))?");
#else
    static const std::regex r_pragma("#pragma +sw +require +(\\S+)( +(\\S+))?");
#endif
    std::smatch m;
    while (std::regex_search(f, m, r_pragma))
    {
        auto m1 = m[1].str();
        if (m1 == "header")
        {
            auto upkg = extractFromString(m[3].str());
            ResolveRequest rr{ upkg, {} };
            if (!b.resolve(rr))
                throw SW_RUNTIME_ERROR("Not resolved: " + rr.u.toString());
            SW_UNIMPLEMENTED;
            /*auto pkg = b.getContext().install(rr.getPackage());
            auto gn = b.getContext().getInputDatabase().getFileHash(pkg.getDirSrc2() / "sw.cpp");
            if (!gns.insert(gn).second)
                throw SW_RUNTIME_ERROR("#pragma sw header: trying to add same header twice, last one: " + upkg.toString());
            auto h = getPackageHeader(pkg, upkg);
            auto [headers2, udeps2] = getFileDependencies(b, h, gns);
            headers.insert(headers.end(), headers2.begin(), headers2.end());
            udeps.insert(udeps2.begin(), udeps2.end());
            headers.push_back(h);*/
        }
        else if (m1 == "local")
        {
            SW_UNIMPLEMENTED;
            auto [headers2, udeps2] = getFileDependencies(b, m[3].str(), gns);
            headers.insert(headers.end(), headers2.begin(), headers2.end());
            udeps.insert(udeps2.begin(), udeps2.end());
        }
        else
        {
            // to be reconsidered
            SW_UNIMPLEMENTED;
            udeps.insert(extractFromString(m1));
        }
        f = m.suffix().str();
    }

    return { headers, udeps };
}

static std::pair<FilesOrdered, std::unordered_set<UnresolvedPackageName>> getFileDependencies(SwBuild &b, const path &in_config_file)
{
    std::set<size_t> gns;
    return getFileDependencies(b, in_config_file, gns);
}

struct ConfigBuiltinLibraryTarget : StaticLibraryTarget
{
    ConfigBuiltinLibraryTarget(TargetBase &parent, const PackageName &id)
        : StaticLibraryTarget(parent, id)
    {
        IsSwConfig = true;
    }

private:
    path getBinaryParentDir() const override
    {
        return StaticLibraryTarget::getTargetDirShort(getContext().getLocalStorage().storage_dir_tmp / "cfg");
    }
};

void addImportLibrary(Build &b)
{
#ifdef _WIN32
    auto mod = (HMODULE)primitives::getModuleForSymbol(&isDriverDllBuild);
    auto syms = getExports(mod);
    if (syms.empty())
        throw SW_RUNTIME_ERROR("No exports found");
    String defs;
    defs += "LIBRARY " IMPORT_LIBRARY "\n";
    defs += "EXPORTS\n";
    for (auto &s : syms)
        defs += "    "s + s + "\n";
    write_file_if_different(getImportDefinitionsFile(b), defs);

    auto &lib = b.add<ConfigBuiltinLibraryTarget>("implib");
    lib.command_storage = &getDriverCommandStorage(b);
    lib.AutoDetectOptions = false;
    lib += getImportDefinitionsFile(b);
#endif
}

void addDelayLoadLibrary(Build &b)
{
#ifdef _WIN32
    auto &lib = b.add<ConfigBuiltinLibraryTarget>("delay_loader");
    lib.command_storage = &getDriverCommandStorage(b);
    lib.AutoDetectOptions = false;
    lib += Definition("IMPORT_LIBRARY=\""s + IMPORT_LIBRARY + "\"");
    auto driver_idir = getDriverIncludeDir(b, lib);
    auto fn = driver_idir / getSwDir() / "misc" / "delay_load_helper.cpp";
    lib += fn;
    //if (auto nsf = lib[fn].as<NativeSourceFile *>())
        //nsf->setOutputFile(getPchDir(b) / ("delay_load_helper" + getDepsSuffix(*this, lib, deps) + ".obj"));
    lib.WholeArchive = true;
#endif
}

static void addConfigDefs(NativeCompiledTarget &lib)
{
    if (lib.getBuildSettings().TargetOS.is(OSType::Windows))
    {
        lib.Definitions["SW_SUPPORT_API"] = "__declspec(dllimport)";
        lib.Definitions["SW_MANAGER_API"] = "__declspec(dllimport)";
        lib.Definitions["SW_BUILDER_API"] = "__declspec(dllimport)";
        lib.Definitions["SW_CORE_API"] = "__declspec(dllimport)";
        lib.Definitions["SW_DRIVER_CPP_API"] = "__declspec(dllimport)";
        // do not use api name because we use C linkage
        lib.Definitions["SW_PACKAGE_API"] = "__declspec(dllexport)";
    }
    else
    {
        lib.Definitions["SW_SUPPORT_API="];
        lib.Definitions["SW_MANAGER_API="];
        lib.Definitions["SW_BUILDER_API="];
        lib.Definitions["SW_CORE_API="];
        lib.Definitions["SW_DRIVER_CPP_API="];
        // do not use api name because we use C linkage
        lib.Definitions["SW_PACKAGE_API"] = "__attribute__ ((visibility (\"default\")))";
    }

    if (lib.getCompilerType() == CompilerType::MSVC)
        lib.CompileOptions.push_back("/utf-8");
    // for checks
    // prevent "" be convered into bools
    if (lib.getCompilerType() == CompilerType::Clang)
        lib.CompileOptions.push_back("-Werror=string-conversion");
}

void addConfigPchLibrary(Build &b)
{
#ifdef _WIN32
    auto &lib = b.add<ConfigBuiltinLibraryTarget>("config_pch");
    lib.AutoDetectOptions = false;
    lib.CPPVersion = CPPLanguageStandard::CPP20;
    lib.command_storage = &getDriverCommandStorage(b);

    auto driver_idir = getDriverIncludeDir(b, lib);
    auto swh = driver_idir / getSwHeader();
    //lib.Interface += ForceInclude(swh);
    lib += PrecompiledHeader(swh);
    PathOptionsType files;
    files.insert(swh);
    lib.pch.setup(lib, files);
    lib.Interface += lib.pch.pch;
    lib.Interface += lib.pch.pdb;
    auto swhpch = swh += ".hpch";
    File(swhpch, lib.getFs()).setGenerated();
    lib.Interface += swhpch;
    //lib.pch.use_only = true;
    addDeps(b, lib);
    addConfigDefs(lib);
    lib.WholeArchive = true;
#endif
}

ExtendedBuild NativeTargetEntryPoint::createBuild(SwBuild &swb, const PackageSettings &s) const
{
    if (!dd)
        dd = std::make_unique<DriverData>();

    for (auto &[h, d] : s["driver"]["source-dir-for-source"].getMap())
        dd->source_dirs_by_source[h].requested_dir = d.getValue();
    for (auto &[pkg, p] : s["driver"]["source-dir-for-package"].getMap())
        dd->source_dirs_by_package[pkg] = p.getValue();
    if (s["driver"]["force-source"].isValue())
        dd->force_source = load(nlohmann::json::parse(s["driver"]["force-source"].getValue()));

    ExtendedBuild b(swb);
    b.dd = dd.get();
    b.DryRun = s["driver"]["dry-run"] && s["driver"]["dry-run"].get<bool>();

    if (!source_dir.empty())
        b.setSourceDirectory(source_dir);
    else
        b.setSourceDirectory(swb.getBuildDirectory().parent_path());
    b.BinaryDir = swb.getBuildDirectory();

    return b;
}

std::vector<ITargetPtr> NativeTargetEntryPoint::loadPackages(SwBuild &swb, const PackageSettings &s) const
{
    auto b = createBuild(swb, s);
    b.module_data.current_settings = &s;
    loadPackages1(b);
    for (auto &&t : b.module_data.getTargets())
    {
        if (auto t1 = dynamic_cast<Target *>(t.get()))
            t1->prepare1();
    }
    return std::move(b.module_data.getTargets());
}

ITargetPtr NativeTargetEntryPoint::loadPackage(SwBuild &swb, const PackageSettings &s, const Package &p) const
{
    auto b = createBuild(swb, s);
    b.module_data.current_settings = &s; // in any case
    b.module_data.known_target = &p;
    b.NamePrefix = p.getId().getName().getPath().slice(0, p.getData().prefix);
    loadPackages1(b);
    for (auto &&t : b.module_data.getTargets())
    {
        if (auto t1 = dynamic_cast<Target *>(t.get()))
            t1->prepare1();
    }
    if (b.module_data.getTargets().size() != 1)
        throw SW_RUNTIME_ERROR("Bad number of targets: " + p.getId().toString());
    return std::move(b.module_data.getTargets()[0]);
}

NativeBuiltinTargetEntryPoint::NativeBuiltinTargetEntryPoint(BuildFunction bf)
{
    this->bf = bf;
}

void NativeBuiltinTargetEntryPoint::loadPackages1(Build &b) const
{
    if (cf)
        cf(*b.checker);
    if (!bf)
        throw SW_RUNTIME_ERROR("No internal build function set");
    bf(b);
}

NativeModuleTargetEntryPoint::NativeModuleTargetEntryPoint(const Module &m)
    : m(m)
{
}

void NativeModuleTargetEntryPoint::loadPackages1(Build &b) const
{
    m.check(b, *b.checker);
    m.build(b);
}

void PrepareConfig::addInput(Build &b, const Input &i)
{
    InputData d;
    auto files = i.getSpecification().getFiles();
    SW_CHECK(!files.empty());
    d.fn = d.cfn = *files.begin();
    /*if (auto pkgs = i.getPackages(); !pkgs.empty())
    {
        d.link_name = "[" + pkgs.begin()->toString() + "]/[config]";
        d.cl_name = d.link_name + "/" + d.fn.filename().string();
    }*/

    if (d.fn.extension() == ".vala")
        lang = LANG_VALA;
    else if (d.fn.extension() == ".c")
        lang = LANG_C;
    //else if (d.fn.extension() == ".cpp")
        //lang = LANG_CPP;
    // cpp has now some other exts
    // TODO: sync with driver.cpp
    else
        lang = LANG_CPP;
        //SW_UNIMPLEMENTED;
    r[d.fn].dll = one2one(b, d);
    if (fs::exists(r[d.fn].dll))
        inputs_outdated |= i.isOutdated(fs::last_write_time(r[d.fn].dll));
    else
        inputs_outdated = true;
}

template <class T>
struct ConfigSharedLibraryTarget : T
{
    using Base = T;

    ConfigSharedLibraryTarget(TargetBase &parent, const PackageName &id, const PrepareConfig &ep, const PrepareConfig::InputData &d, const path &storage_dir)
        : T(parent, id), ep(ep), d(d)
    {
        Base::IsSwConfig = true;
        Base::IsSwConfigLocal = !is_under_root(d.fn, storage_dir);
    }

private:
    const PrepareConfig &ep;
    PrepareConfig::InputData d;

    /*std::shared_ptr<builder::Command> getCommand() const override
    {
        auto c = T::getCommand();
        if (!d.link_name.empty())
            c->name = d.link_name + Base::getSelectedTool()->Extension;
        return c;
    }*/

    Commands getCommands() const override
    {
        // only for msvc?
        if (getHostOS().is(OSType::Windows))
        {
            // set main cmd dependency on config files
            // otherwise it does not work on windows
            // link.exe uses pdb file and cl.exe cannot proceed
            // fatal error C1041: cannot open program database '*.pdb';
            // if multiple CL.EXE write to the same .PDB file, please use /FS
            // use /Z7 instead?
            /*auto c = getLinker().getCommand();
            for (auto t : ep.targets)
            {
                auto cmd = t->getLinker().getCommand();
                auto cmds = ((ConfigSharedLibraryTarget*)t)->Base::getCommands();
                for (auto &c2 : cmds)
                {
                    if (c2 != cmd)
                        c->dependencies.insert(c2);
                }
            }*/
        }
        return Base::getCommands();
    }

    path getBinaryParentDir() const override
    {
        if (Base::IsSwConfigLocal)
            return Base::getBinaryParentDir();
        return Base::getTargetDirShort(Base::getContext().getLocalStorage().storage_dir_tmp / "cfg");
    }
};

SharedLibraryTarget &PrepareConfig::createTarget(Build &b, const InputData &d)
{
    auto name = getSelfTargetName(b, { d.fn });
    PackageVersion v(primitives::version::Version(0, 0, ::sw_get_module_abi_version()));
    auto &lib =
        /*lang == LANG_VALA
        ? (SharedLibraryTarget&)b.addTarget<ConfigSharedLibraryTarget<ValaSharedLibrary>>(name, v, *this, d, b.getContext().getLocalStorage().storage_dir)
        : */b.addTarget<ConfigSharedLibraryTarget<SharedLibraryTarget>>(name, v, *this, d, b.getContext().getLocalStorage().storage_dir);

    SW_UNIMPLEMENTED;
    /*tgt = lib.getPackage();
    targets.insert(&lib);
    return lib;*/
}

decltype(auto) PrepareConfig::commonActions(Build &b, const InputData &d, const std::unordered_set<UnresolvedPackageName> &deps)
{
    // save udeps
    //udeps = deps;

    auto &fn = d.fn;
    auto &lib = createTarget(b, d);
    lib.GenerateWindowsResource = false;
    lib.command_storage = &getDriverCommandStorage(b);

    // cache idir
    if (driver_idir.empty())
        driver_idir = getDriverIncludeDir(b, lib);

    addDeps(b, lib);
    if (isDriverStaticBuild())
        lib += "implib"_dep;
    lib.AutoDetectOptions = false;
    lib.CPPVersion = CPPLanguageStandard::CPP20;
    lib.NoUndefined = false;

    lib += fn;
    if (lang == LANG_VALA)
    {
        SW_UNIMPLEMENTED;
        //auto cfn = ((ValaSharedLibrary &)lib).getOutputCCodeFileName(fn);
        //File(cfn, lib.getFs()).setGenerated(true);
        //lib += cfn;
        //(path&)d.cfn = cfn; // set c name
    }
    if (!d.cl_name.empty())
        lib[fn].fancy_name = d.cl_name;

    if (lib.getBuildSettings().TargetOS.is(OSType::Windows) && isDriverStaticBuild())
        lib += "delay_loader"_dep;

    if (lang == LANG_VALA)
    {
        lib.CustomTargetOptions[VALA_OPTIONS_NAME].push_back("--vapidir");
        lib.CustomTargetOptions[VALA_OPTIONS_NAME].push_back(to_string(normalize_path(getDriverIncludeDir(b, lib) / "sw/driver/frontend/vala")));
        lib.CustomTargetOptions[VALA_OPTIONS_NAME].push_back("--pkg");
        lib.CustomTargetOptions[VALA_OPTIONS_NAME].push_back("sw");
        // when (cheader_filename = "sw/driver/c/c.h") is present
        //lib.CustomTargetOptions[VALA_OPTIONS_NAME].push_back("--includedir=" + normalize_path(getDriverIncludeDir(b, lib)));

#ifdef _WIN32
        // set dll deps (glib, etc)
        lib.add(CallbackType::EndPrepare, [&lib, this, d]()
        {
            builder::Command c;
            lib.setupCommand(c);
            for (path p : split_string(c.environment["PATH"], ";"))
                r[d.fn].PATH.push_back(p);
        });
#endif
    }

    // pch
    if (lang == LANG_CPP)
    {
        lib += "config_pch"_dep;
        //lib += ForceInclude(driver_idir / getSwHeader());
        //lib += PrecompiledHeader(driver_idir / getSwHeader());

        /*detail::PrecompiledHeader pch;
        pch.name = getImportPchFile(*this, lib, deps).stem();
        pch.dir = getPchDir(b);
        pch.fancy_name = "[config pch]";
        lib.pch = pch;*/
    }

    return lib;
}

// one input file to one dll
path PrepareConfig::one2one(Build &b, const InputData &d)
{
    auto &fn = d.cfn;
    const auto [headers, udeps] = getFileDependencies(b.getMainBuild(), fn);

    auto &lib = commonActions(b, d, udeps);

    // turn on later again
    //if (lib.getSettings().TargetOS.is(OSType::Windows))
        //lib += "_CRT_SECURE_NO_WARNINGS"_def;

    // file deps
    {
        for (auto &h : headers)
            lib += ForceInclude(h);
        // sort deps first!
        std::map<size_t, const UnresolvedPackageName*> deps_sorted;
        for (auto &d : udeps)
            deps_sorted[std::hash<UnresolvedPackageName>()(d)] = &d;
        for (auto &[_,d] : deps_sorted)
            lib += std::make_shared<Dependency>(*d);
    }

    FilesOrdered fi_files;
    if (lang == LANG_CPP)
    {
        fi_files.push_back(driver_idir / getSw1Header());
        fi_files.push_back(driver_idir / getSwCheckAbiVersionHeader());
    }
    else
    {
        fi_files.push_back(driver_idir / getSwDir() / "c" / "c.h"); // main include, goes first
        fi_files.push_back(driver_idir / getSwDir() / "c" / "swc.h");
        fi_files.push_back(driver_idir / getSwCheckAbiVersionHeader()); // TODO: remove it, we don't need abi here
    }

    //
    for (auto &f : fi_files)
        lib += ForceInclude(f);
    // deprecated warning
    // activate later
    // this causes cl warning (PCH is built without it)
    // we must build two PCHs? for storage pks and local pkgs
    //c->Warnings().TreatAsError.push_back(4996);

    //commonActions2
    addConfigDefs(lib);

    BuildSettings bs(b.module_data.getSettings());
    if (bs.TargetOS.is(OSType::Windows))
        lib.NativeLinkerOptions::System.LinkLibraries.insert(LinkLibrary{ "DELAYIMP.LIB"s });

    if (lib.getLinkerType() == LinkerType::MSVC)
    //if (auto L = r->program.as<VisualStudioLinker*>())
    {
        auto &r = lib.getRule("link");
        r.getArguments().push_back("/DELAYLOAD:"s + IMPORT_LIBRARY);
        //#ifdef CPPAN_DEBUG
        r.getArguments().push_back("/DEBUG:FULL");
        //#endif
        if (isDriverStaticBuild())
            r.getArguments().push_back("/FORCE:MULTIPLE");
        else
            r.getArguments().push_back("/FORCE:UNRESOLVED");

        /*L->DelayLoadDlls().push_back(IMPORT_LIBRARY);
        //#ifdef CPPAN_DEBUG
        L->GenerateDebugInformation = vs::link::Debug::Full;
        //#endif
        if (isDriverStaticBuild())
            L->Force = vs::ForceType::Multiple;
        else
            L->Force = vs::ForceType::Unresolved;
        L->IgnoreWarnings().insert(4006); // warning LNK4006: X already defined in Y; second definition ignored
        L->IgnoreWarnings().insert(4070); // warning LNK4070: /OUT:X.dll directive in .EXP differs from output filename 'Y.dll'; ignoring directive
        // cannot be ignored https://docs.microsoft.com/en-us/cpp/build/reference/ignore-ignore-specific-warnings?view=vs-2017
        //L->IgnoreWarnings().insert(4088); // warning LNK4088: image being generated due to /FORCE option; image may not run
        */
    }

    return lib.getOutputFile();
}

bool PrepareConfig::isOutdated() const
{
    if (inputs_outdated)
        return true;

    auto get_lwt = [](const path &p)
    {
        return file_time_type2time_t(fs::last_write_time(p));
    };

    bool not_exists = false;
    size_t t0 = 0;
    size_t t = 0;
    hash_combine(t, get_lwt(boost::dll::program_location()));

    for (auto &[p, out] : r)
    {
        hash_combine(t, get_lwt(p));
        not_exists |= !fs::exists(out.dll);
        if (!not_exists)
            hash_combine(t, get_lwt(out.dll));
    }

    auto f = path(".sw") / "stamp" / (std::to_string(t) + ".txt");
    if (fs::exists(f))
        t0 = std::stoull(read_file(f));
    write_file(f, std::to_string(t));
    return not_exists || t0 != t;
}

}
