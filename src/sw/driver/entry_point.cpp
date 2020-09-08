// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "entry_point.h"

#include "build.h"
#include "command.h"
#include "driver.h"
#include "suffix.h"
#include "target/all.h"
#include "sw_check_abi_version.h"

#include <sw/core/sw_context.h>
#include <sw/core/input_database.h>
#include <sw/core/input.h>
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
    return b.getContext().getLocalStorage().storage_dir_tmp / "pch";
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

static String getDepsSuffix(PrepareConfig &pc, NativeCompiledTarget &t, const UnresolvedPackages &deps)
{
    std::set<String> sdeps;
    for (auto &d : t.getDependencies())
        sdeps.insert(d->getUnresolvedPackage().toString());
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

static path getImportPchFile(PrepareConfig &pc, NativeCompiledTarget &t, const UnresolvedPackages &deps)
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

void addImportLibrary(const Build &b, NativeCompiledTarget &t)
{
#ifdef _WIN32
    auto lib = (HMODULE)primitives::getModuleForSymbol(&isDriverDllBuild);
    auto syms = getExports(lib);
    if (syms.empty())
        throw SW_RUNTIME_ERROR("No exports found");
    String defs;
    defs += "LIBRARY " IMPORT_LIBRARY "\n";
    defs += "EXPORTS\n";
    for (auto &s : syms)
        defs += "    "s + s + "\n";
    write_file_if_different(getImportDefinitionsFile(b), defs);

    auto c = t.addCommand();
    c->working_directory = getImportDefinitionsFile(b).parent_path();
    c << t.Librarian->file
        << cmd::in(getImportDefinitionsFile(b), cmd::Prefix{ "-DEF:" }, cmd::Skip)
        << cmd::out(getImportLibraryFile(b), cmd::Prefix{ "-OUT:" })
        ;
    t.LinkLibraries.push_back(LinkLibrary{ getImportLibraryFile(b) });
#endif
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

static path getPackageHeader(const LocalPackage &p, const UnresolvedPackage &up)
{
    // TODO: add '#pragma sw driver ...' ?

    // depends on upkg, not on pkg!
    // because p is constant, but up might differ
    auto h = p.getDirSrc() / "gen" / ("pkg_header_" + shorten_hash(sha1(up.toString()), 6) + ".h");
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
    return h;
}

static std::pair<FilesOrdered, UnresolvedPackages>
getFileDependencies(const SwCoreContext &swctx, const path &p, std::set<size_t> &gns)
{
    UnresolvedPackages udeps;
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
            auto pkg = swctx.resolve(upkg);
            auto gn = swctx.getInputDatabase().getFileHash(pkg.getDirSrc2() / "sw.cpp");
            if (!gns.insert(gn).second)
                throw SW_RUNTIME_ERROR("#pragma sw header: trying to add same header twice, last one: " + upkg.toString());
            auto h = getPackageHeader(pkg, upkg);
            auto [headers2,udeps2] = getFileDependencies(swctx, h, gns);
            headers.insert(headers.end(), headers2.begin(), headers2.end());
            udeps.insert(udeps2.begin(), udeps2.end());
            headers.push_back(h);
        }
        else if (m1 == "local")
        {
            SW_UNIMPLEMENTED;
            auto [headers2, udeps2] = getFileDependencies(swctx, m[3].str(), gns);
            headers.insert(headers.end(), headers2.begin(), headers2.end());
            udeps.insert(udeps2.begin(), udeps2.end());
        }
        else
            udeps.insert(extractFromString(m1));
        f = m.suffix().str();
    }

    return { headers, udeps };
}

static std::pair<FilesOrdered, UnresolvedPackages> getFileDependencies(const SwCoreContext &swctx, const path &in_config_file)
{
    std::set<size_t> gns;
    return getFileDependencies(swctx, in_config_file, gns);
}

Build NativeTargetEntryPoint::createBuild(SwBuild &swb, const TargetSettings &s, const PackageIdSet &pkgs, const PackagePath &prefix) const
{
    // we need to fix some settings before they go to targets
    auto settings = s;

    if (!dd)
        dd = std::make_unique<DriverData>();

    for (auto &[h, d] : settings["driver"]["source-dir-for-source"].getMap())
        dd->source_dirs_by_source[h].requested_dir = d.getValue();
    for (auto &[pkg, p] : settings["driver"]["source-dir-for-package"].getMap())
        dd->source_dirs_by_package[pkg] = p.getValue();
    if (settings["driver"]["force-source"].isValue())
        dd->force_source = load(nlohmann::json::parse(settings["driver"]["force-source"].getValue()));

    Build b(swb);
    b.dd = dd.get();
    // leave as b. setting
    b.DryRun = settings["driver"]["dry-run"] == "true";

    //settings.erase("driver");
    settings["driver"].serializable(false);

    b.module_data.known_targets = pkgs;
    b.module_data.current_settings = settings;
    b.NamePrefix = prefix;

    if (!source_dir.empty())
        b.setSourceDirectory(source_dir);
    else
        b.setSourceDirectory(swb.getBuildDirectory().parent_path());
    b.BinaryDir = swb.getBuildDirectory();

    return b;
}

std::vector<ITargetPtr> NativeTargetEntryPoint::loadPackages(SwBuild &swb, const TargetSettings &s, const PackageIdSet &pkgs, const PackagePath &prefix) const
{
    auto b = createBuild(swb, s, pkgs, prefix);
    loadPackages1(b);
    return b.module_data.added_targets;
}

NativeBuiltinTargetEntryPoint::NativeBuiltinTargetEntryPoint(BuildFunction bf)
    : bf(bf)
{
}

void NativeBuiltinTargetEntryPoint::loadPackages1(Build &b) const
{
    if (cf)
        cf(b.checker);
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
    m.check(b, b.checker);
    m.build(b);
}

static PackagePath getSelfTargetName(Build &b, const FilesSorted &files)
{
    String h = b.module_data.current_settings.getHash();
    for (auto &fn : files)
        h += to_string(normalize_path(fn));
    h = shorten_hash(blake2b_512(h), 6);
    return "loc.sw.self." + h;
}

static auto getDriverDep()
{
    return std::make_shared<Dependency>(UnresolvedPackage(SW_DRIVER_NAME));
}

// add Dirs?
path getDriverIncludeDir(Build &solution, Target &lib)
{
    return lib.getFile(getDriverDep()) / "src";
}

static void addDeps(Build &solution, NativeCompiledTarget &lib)
{
    lib += "pub.egorpugin.primitives.templates-master"_dep; // for SW_RUNTIME_ERROR

    // uncomment when you need help
    //lib += "pub.egorpugin.primitives.source-master"_dep;
    //lib += "pub.egorpugin.primitives.version-master"_dep;
    lib += "pub.egorpugin.primitives.command-master"_dep;
    lib += "pub.egorpugin.primitives.filesystem-master"_dep;

    auto d = lib + UnresolvedPackage(SW_DRIVER_NAME);
    d->IncludeDirectoriesOnly = true;
    //d->GenerateCommandsBefore = true;
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

    ConfigSharedLibraryTarget(TargetBase &parent, const PackageId &id, const PrepareConfig &ep, const PrepareConfig::InputData &d, const path &storage_dir)
        : T(parent, id), ep(ep), d(d)
    {
        Base::IsSwConfig = true;
        Base::IsSwConfigLocal = !is_under_root(d.fn, storage_dir);
    }

private:
    const PrepareConfig &ep;
    PrepareConfig::InputData d;

    std::shared_ptr<builder::Command> getCommand() const override
    {
        auto c = T::getCommand();
        if (!d.link_name.empty())
            c->name = d.link_name + Base::getSelectedTool()->Extension;
        return c;
    }

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
            auto c = getCommand();
            for (auto t : ep.targets)
            {
                auto cmd = t->getCommand();
                auto cmds = ((ConfigSharedLibraryTarget*)t)->Base::getCommands();
                for (auto &c2 : cmds)
                {
                    if (c2 != cmd)
                        c->dependencies.insert(c2);
                }
            }
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
    Version v(0, 0, ::sw_get_module_abi_version());
    auto &lib =
        lang == LANG_VALA
        ? (SharedLibraryTarget&)b.addTarget<ConfigSharedLibraryTarget<ValaSharedLibrary>>(name, v, *this, d, b.getContext().getLocalStorage().storage_dir)
        : b.addTarget<ConfigSharedLibraryTarget<SharedLibraryTarget>>(name, v, *this, d, b.getContext().getLocalStorage().storage_dir);
    tgt = lib.getPackage();
    targets.insert(&lib);
    return lib;
}

decltype(auto) PrepareConfig::commonActions(Build &b, const InputData &d, const UnresolvedPackages &deps)
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
        addImportLibrary(b, lib);
    lib.AutoDetectOptions = false;
    lib.CPPVersion = CPPLanguageStandard::CPP20;
    lib.NoUndefined = false;

    lib += fn;
    if (lang == LANG_VALA)
    {
        auto cfn = ((ValaSharedLibrary &)lib).getOutputCCodeFileName(fn);
        File(cfn, lib.getFs()).setGenerated(true);
        lib += cfn;
        (path&)d.cfn = cfn; // set c name
    }
    if (!d.cl_name.empty())
        lib[fn].fancy_name = d.cl_name;

    if (lib.getCompilerType() == CompilerType::MSVC)
        lib.CompileOptions.push_back("/utf-8");
    // for checks
    // prevent "" be convered into bools
    if (lib.getCompilerType() == CompilerType::Clang)
        lib.CompileOptions.push_back("-Werror=string-conversion");

    if (lib.getBuildSettings().TargetOS.is(OSType::Windows) && isDriverStaticBuild())
    {
        lib += Definition("IMPORT_LIBRARY=\""s + IMPORT_LIBRARY + "\"");
        auto fn = driver_idir / getSwDir() / "misc" / "delay_load_helper.cpp";
        lib += fn;
        if (auto nsf = lib[fn].as<NativeSourceFile *>())
            nsf->setOutputFile(getPchDir(b) / ("delay_load_helper" + getDepsSuffix(*this, lib, deps) + ".obj"));
    }

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
        lib += PrecompiledHeader(driver_idir / getSwHeader());

        detail::PrecompiledHeader pch;
        pch.name = getImportPchFile(*this, lib, deps).stem();
        pch.dir = getPchDir(b);
        pch.fancy_name = "[config pch]";
        lib.pch = pch;
    }

    return lib;
}

// one input file to one dll
path PrepareConfig::one2one(Build &b, const InputData &d)
{
    auto &fn = d.cfn;
    auto [headers, udeps] = getFileDependencies(b.getContext(), fn);

    auto &lib = commonActions(b, d, udeps);

    // turn on later again
    //if (lib.getSettings().TargetOS.is(OSType::Windows))
        //lib += "_CRT_SECURE_NO_WARNINGS"_def;

    // file deps
    {
        for (auto &h : headers)
        {
            // TODO: refactor this and same cases below
            if (auto sf = lib[fn].template as<NativeSourceFile *>())
            {
                if (auto c = sf->compiler->template as<VisualStudioCompiler *>())
                {
                    c->ForcedIncludeFiles().push_back(h);
                }
                else if (auto c = sf->compiler->template as<ClangClCompiler *>())
                {
                    c->ForcedIncludeFiles().push_back(h);
                }
                else if (auto c = sf->compiler->template as<ClangCompiler *>())
                {
                    c->ForcedIncludeFiles().push_back(h);
                }
                else if (auto c = sf->compiler->template as<GNUCompiler *>())
                {
                    c->ForcedIncludeFiles().push_back(h);
                }
            }
        }
        // sort deps first!
        for (auto &d : std::set<UnresolvedPackage>(udeps.begin(), udeps.end()))
            lib += std::make_shared<Dependency>(d);
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

    if (auto sf = lib[fn].template as<NativeSourceFile*>())
    {
        if (auto c = sf->compiler->template as<VisualStudioCompiler*>())
        {
            for (auto &f : fi_files)
                c->ForcedIncludeFiles().push_back(f);

            // deprecated warning
            // activate later
            // this causes cl warning (PCH is built without it)
            // we must build two PCHs? for storage pks and local pkgs
            //c->Warnings().TreatAsError.push_back(4996);
        }
        else if (auto c = sf->compiler->template as<ClangClCompiler*>())
        {
            for (auto &f : fi_files)
                c->ForcedIncludeFiles().push_back(f);
        }
        else if (auto c = sf->compiler->template as<ClangCompiler*>())
        {
            for (auto &f : fi_files)
                c->ForcedIncludeFiles().push_back(f);
        }
        else if (auto c = sf->compiler->template as<GNUCompiler*>())
        {
            for (auto &f : fi_files)
                c->ForcedIncludeFiles().push_back(f);
        }
    }

    //commonActions2
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

    BuildSettings bs(b.module_data.current_settings);
    if (bs.TargetOS.is(OSType::Windows))
        lib.NativeLinkerOptions::System.LinkLibraries.insert(LinkLibrary{ "DELAYIMP.LIB"s });

    if (auto L = lib.Linker->template as<VisualStudioLinker*>())
    {
        L->DelayLoadDlls().push_back(IMPORT_LIBRARY);
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
