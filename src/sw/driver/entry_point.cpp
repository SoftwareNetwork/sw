// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "entry_point.h"

#include "build.h"
#include "command.h"
#include "driver.h"
#include "suffix.h"
#include "target/native.h"

#include <sw/core/sw_context.h>
#include <sw/manager/storage.h>

#include <boost/dll.hpp>
#include <nlohmann/json.hpp>
#include <primitives/emitter.h>
#include <primitives/sw/settings_program_name.h>
#include <primitives/symbol.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "entry_point");

bool gVerbose;

#define SW_DRIVER_NAME "org.sw." PACKAGE_NAME "-" PACKAGE_VERSION

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

static String getDepsSuffix(NativeCompiledTarget &t, const UnresolvedPackages &deps)
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
    h = "." + shorten_hash(blake2b_512(s), 6);
    return h;
}

static path getImportPchFile(NativeCompiledTarget &t, const UnresolvedPackages &deps)
{
    // we create separate pch for different target deps
    auto h = getDepsSuffix(t, deps);
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
    return b.getContext().getCommandStorage(b.getContext().getLocalStorage().storage_dir_tmp / "db" / "service");
}

static void addImportLibrary(const Build &b, NativeCompiledTarget &t)
{
#ifdef _WIN32
    auto lib = (HMODULE)primitives::getModuleForSymbol();
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
    c.c->working_directory = getImportDefinitionsFile(b).parent_path();
    c << t.Librarian->file
        << cmd::in(getImportDefinitionsFile(b), cmd::Prefix{ "-DEF:" }, cmd::Skip)
        << cmd::out(getImportLibraryFile(b), cmd::Prefix{ "-OUT:" })
        ;
    t.LinkLibraries.push_back(getImportLibraryFile(b));
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
        ctx.addLine("#line " + std::to_string(nlines) + " \"" + normalize_path(cfg) + "\"");

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

static std::pair<FilesOrdered, UnresolvedPackages> getFileDependencies(const SwBuilderContext &swctx, const path &p, std::set<PackageVersionGroupNumber> &gns)
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
            if (pkg.getData().group_number == 0)
            {
                auto gn = get_specification_hash(read_file(pkg.getDirSrc2() / "sw.cpp"));
                pkg.setGroupNumber(gn);
                ((PackageData &)pkg.getData()).group_number = gn;
            }
            if (!gns.insert(pkg.getData().group_number).second)
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

static std::pair<FilesOrdered, UnresolvedPackages> getFileDependencies(const SwBuilderContext &swctx, const path &in_config_file)
{
    std::set<PackageVersionGroupNumber> gns;
    return getFileDependencies(swctx, in_config_file, gns);
}

std::vector<ITargetPtr> NativeTargetEntryPoint::loadPackages(SwBuild &swb, const TargetSettings &s, const PackageIdSet &pkgs, const PackagePath &prefix) const
{
    Build b(swb);

    // we need to fix some settings before they go to targets
    auto settings = s;

    b.DryRun = settings["driver"]["dry-run"] == "true";
    for (auto &[h, d] : settings["driver"]["source-dir-for-source"].getSettings())
        b.source_dirs_by_source[h].requested_dir = d.getValue();
    for (auto &[pkg, p] : settings["driver"]["source-dir-for-package"].getSettings())
        b.source_dirs_by_package[pkg] = p.getValue();
    if (settings["driver"]["force-source"].isValue())
        b.force_source = load(nlohmann::json::parse(settings["driver"]["force-source"].getValue()));
    //settings.erase("driver");
    settings["driver"].useInHash(false);
    settings["driver"].ignoreInComparison(true);

    b.module_data.known_targets = pkgs;
    b.module_data.current_settings = settings;
    b.NamePrefix = prefix;

    if (!source_dir.empty())
        b.setSourceDirectory(source_dir);
    else
        b.setSourceDirectory(swb.getBuildDirectory().parent_path());
    b.BinaryDir = swb.getBuildDirectory();

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

static auto getFilesHash(const FilesSorted &files)
{
    String h;
    for (auto &fn : files)
        h += fn.u8string();
    return shorten_hash(blake2b_512(h), 6);
}

static PackagePath getSelfTargetName(const FilesSorted &files)
{
    return "loc.sw.self." + getFilesHash(files);
}

static auto getDriverDep()
{
    return std::make_shared<Dependency>(UnresolvedPackage(SW_DRIVER_NAME));
}

// add Dirs?
static path getDriverIncludeDir(Build &solution, NativeCompiledTarget &lib)
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

PrepareConfigEntryPoint::PrepareConfigEntryPoint(const std::unordered_set<LocalPackage> &pkgs)
    : pkgs_(pkgs)
{}

PrepareConfigEntryPoint::PrepareConfigEntryPoint(const Files &files)
    : files_(files)
{}

void PrepareConfigEntryPoint::loadPackages1(Build &b) const
{
    if (files_.empty())
        many2one(b, pkgs_);
    else
        many2many(b, files_);
}

SharedLibraryTarget &PrepareConfigEntryPoint::createTarget(Build &b, const String &name) const
{
    struct ConfigSharedLibraryTarget : SharedLibraryTarget
    {
        ConfigSharedLibraryTarget()
        {
            IsSwConfig = true;
        }
    };

    auto &lib = b.addTarget<ConfigSharedLibraryTarget>(name, "local");
    tgt = lib.getPackage();
    return lib;
}

decltype(auto) PrepareConfigEntryPoint::commonActions(Build &b, const FilesSorted &files, const UnresolvedPackages &deps) const
{
    // record udeps
    udeps = deps;

    auto &lib = createTarget(b, getSelfTargetName(files));
    lib.GenerateWindowsResource = false;
    lib.command_storage = &getDriverCommandStorage(b);

    addDeps(b, lib);
    addImportLibrary(b, lib);
    lib.AutoDetectOptions = false;
    lib.CPPVersion = CPPLanguageStandard::CPP17;
    lib.NoUndefined = false;

    if (lib.getBuildSettings().TargetOS.isApple())
    {
        lib.LinkOptions.push_back("-undefined");
        lib.LinkOptions.push_back("dynamic_lookup");
    }

    for (auto &fn : files)
        lib += fn;

    if (lib.getCompilerType() == CompilerType::MSVC)
        lib.CompileOptions.push_back("/utf-8");

    if (lib.getBuildSettings().TargetOS.is(OSType::Windows))
    {
        auto fn = getDriverIncludeDir(b, lib) / getSwDir() / "misc" / "delay_load_helper.cpp";
        lib += fn;
        if (auto nsf = lib[fn].as<NativeSourceFile *>())
            nsf->setOutputFile(getPchDir(b) / ("delay_load_helper" + getDepsSuffix(lib, deps) + ".obj"));
    }

    // pch
    lib += PrecompiledHeader(getDriverIncludeDir(b, lib) / getSwHeader());

    detail::PrecompiledHeader pch;
    pch.name = getImportPchFile(lib, deps).stem();
    pch.dir = getPchDir(b);
    pch.fancy_name = "[config pch]";
    lib.pch = pch;

    return lib;
}

void PrepareConfigEntryPoint::commonActions2(Build &b, SharedLibraryTarget &lib) const
{
    if (lib.getBuildSettings().TargetOS.is(OSType::Windows))
    {
        lib.Definitions["SW_SUPPORT_API"] = "__declspec(dllimport)";
        lib.Definitions["SW_MANAGER_API"] = "__declspec(dllimport)";
        lib.Definitions["SW_BUILDER_API"] = "__declspec(dllimport)";
        lib.Definitions["SW_DRIVER_CPP_API"] = "__declspec(dllimport)";
        // do not use api name because we use C linkage
        lib.Definitions["SW_PACKAGE_API"] = "__declspec(dllexport)";
    }
    else
    {
        lib.Definitions["SW_SUPPORT_API="];
        lib.Definitions["SW_MANAGER_API="];
        lib.Definitions["SW_BUILDER_API="];
        lib.Definitions["SW_DRIVER_CPP_API="];
        // do not use api name because we use C linkage
        lib.Definitions["SW_PACKAGE_API"] = "__attribute__ ((visibility (\"default\")))";
    }

    BuildSettings bs(b.module_data.current_settings);
    if (bs.TargetOS.is(OSType::Windows))
        lib.NativeLinkerOptions::System.LinkLibraries.insert("Delayimp.lib");

    if (auto L = lib.Linker->template as<VisualStudioLinker*>())
    {
        L->DelayLoadDlls().push_back(IMPORT_LIBRARY);
        //#ifdef CPPAN_DEBUG
        L->GenerateDebugInformation = vs::link::Debug::Full;
        //#endif
        L->Force = vs::ForceType::Multiple;
        L->IgnoreWarnings().insert(4006); // warning LNK4006: X already defined in Y; second definition ignored
        L->IgnoreWarnings().insert(4070); // warning LNK4070: /OUT:X.dll directive in .EXP differs from output filename 'Y.dll'; ignoring directive
                                            // cannot be ignored https://docs.microsoft.com/en-us/cpp/build/reference/ignore-ignore-specific-warnings?view=vs-2017
                                            //L->IgnoreWarnings().insert(4088); // warning LNK4088: image being generated due to /FORCE option; image may not run
    }

    /*auto i = b.getChildren().find(lib.getPackage());
    if (i == b.getChildren().end())
        throw std::logic_error("config target not found");*/

    out = lib.getOutputFile();
}

// many input files to many dlls
void PrepareConfigEntryPoint::many2many(Build &b, const Files &files) const
{
    for (auto &fn : files)
    {
        one2one(b, fn);
        r[fn] = out;
    }
}

// many input files into one dll
void PrepareConfigEntryPoint::many2one(Build &b, const std::unordered_set<LocalPackage> &pkgs) const
{
    // make parallel?
    //std::unordered_map<PackageVersionGroupNumber, path> gn_files;

    SW_UNIMPLEMENTED;

    struct data
    {
        LocalPackage pkg;
        PackageVersionGroupNumber gn;
        path p;
    };

    auto get_package_config = [&b](const LocalPackage &pkg) -> data
    {
        if (!pkg.getData().group_number)
            throw SW_RUNTIME_ERROR("Missing group number");

        SW_UNIMPLEMENTED;
        /*auto pkg2 = pkg.getGroupLeader();
        auto d = driver::cpp::findConfig(pkg2.getDirSrc2(), driver::cpp::Driver::getAvailableFrontendConfigFilenames());
        if (!d)
            throw SW_RUNTIME_ERROR("cannot find config for package " + pkg.toString() + " in dir " + normalize_path(pkg2.getDirSrc2()));
        return data{ {b.getSolution().getContext().getLocalStorage(), pkg2}, pkg.getData().group_number, *d };*/
    };

    // ordered map!
    std::map<path, data> output_names;
    for (auto &pkg : pkgs)
    {
        auto p = get_package_config(pkg);
        pkg_files_.insert(p.p);
        output_names.emplace(p.p, p);
    }

    UnresolvedPackages udeps2;
    std::unordered_map<path, std::pair<FilesOrdered, UnresolvedPackages>> output_names_info;
    for (auto &[fn, d] : output_names)
    {
        output_names_info[fn] = getFileDependencies(b.getContext(), fn);
        udeps2.insert(output_names_info[fn].second.begin(), output_names_info[fn].second.end());
    }

    auto &lib = commonActions(b, pkg_files_, udeps2);

    // make fancy names
    for (auto &[fn, d] : output_names)
    {
        lib[fn].fancy_name = "[" + output_names.find(fn)->second.pkg.toString() + "]/[config]";
        // configs depend on pch, and pch depends on getCurrentModuleId(), so we add name to the file
        // to make sure we have different config .objs for different pchs
        lib[fn].as<NativeSourceFile>().setOutputFile(lib, fn.u8string() + "." + getCurrentModuleId(), lib.getObjectDir(d.pkg) / "self");
        if (gVerbose)
            lib[fn].fancy_name += " (" + normalize_path(fn) + ")";
    }

    // file deps
    auto gnu_setup = [&b, &lib](auto *c, const auto &headers, const path &fn, const auto &gn)
    {
        // we use pch, but cannot add more defs on CL
        // so we create a file with them
        auto hash = gn2suffix(gn);
        path h;
        // cannot create aux dir on windows; auxl = auxiliary
        if (is_under_root(fn, b.getContext().getLocalStorage().storage_dir_pkg))
            h = fn.parent_path().parent_path() / "auxl" / ("defs" + hash + ".h");
        else
            h = b.getMainBuild().getBuildDirectory() / "auxl" / ("defs" + hash + ".h");
        primitives::CppEmitter ctx;

        ctx.addLine("#define configure configure" + hash);
        ctx.addLine("#define build build" + hash);
        ctx.addLine("#define check check" + hash);
        ctx.addLine("#define sw_get_module_abi_version sw_get_module_abi_version" + hash);

        write_file_if_different(h, ctx.getText());

        c->ForcedIncludeFiles().push_back(h);
        c->ForcedIncludeFiles().push_back(getDriverIncludeDir(b, lib) / getSw1Header());

        for (auto &h : headers)
            c->ForcedIncludeFiles().push_back(h);
        c->ForcedIncludeFiles().push_back(getDriverIncludeDir(b, lib) / getSwCheckAbiVersionHeader());
    };

    for (auto &[fn, d] : output_names)
    {
        auto [headers, udeps] = output_names_info[fn];
        if (auto sf = lib[fn].template as<NativeSourceFile*>())
        {
            if (auto c = sf->compiler->template as<VisualStudioCompiler*>())
            {
                gnu_setup(c, headers, fn, d.gn);
            }
            else if (auto c = sf->compiler->template as<ClangClCompiler*>())
            {
                gnu_setup(c, headers, fn, d.gn);
            }
            else if (auto c = sf->compiler->template as<ClangCompiler*>())
            {
                gnu_setup(c, headers, fn, d.gn);
            }
            else if (auto c = sf->compiler->template as<GNUCompiler*>())
            {
                gnu_setup(c, headers, fn, d.gn);
            }
        }
        // sort deps first!
        for (auto &d : std::set<UnresolvedPackage>(udeps.begin(), udeps.end()))
            lib += std::make_shared<Dependency>(d);
    }

    commonActions2(b, lib);
}

// one input file to one dll
void PrepareConfigEntryPoint::one2one(Build &b, const path &fn) const
{
    auto [headers, udeps] = getFileDependencies(b.getContext(), fn);

    auto &lib = commonActions(b, { fn }, udeps);

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

    if (auto sf = lib[fn].template as<NativeSourceFile*>())
    {
        if (auto c = sf->compiler->template as<VisualStudioCompiler*>())
        {
            c->ForcedIncludeFiles().push_back(getDriverIncludeDir(b, lib) / getSw1Header());
            c->ForcedIncludeFiles().push_back(getDriverIncludeDir(b, lib) / getSwCheckAbiVersionHeader());

            // deprecated warning
            // activate later
            // this causes cl warning (PCH is built without it)
            // we must build two PCHs? for storage pks and local pkgs
            //c->Warnings().TreatAsError.push_back(4996);
        }
        else if (auto c = sf->compiler->template as<ClangClCompiler*>())
        {
            c->ForcedIncludeFiles().push_back(getDriverIncludeDir(b, lib) / getSw1Header());
            c->ForcedIncludeFiles().push_back(getDriverIncludeDir(b, lib) / getSwCheckAbiVersionHeader());
        }
        else if (auto c = sf->compiler->template as<ClangCompiler*>())
        {
            c->ForcedIncludeFiles().push_back(getDriverIncludeDir(b, lib) / getSw1Header());
            c->ForcedIncludeFiles().push_back(getDriverIncludeDir(b, lib) / getSwCheckAbiVersionHeader());
        }
        else if (auto c = sf->compiler->template as<GNUCompiler*>())
        {
            c->ForcedIncludeFiles().push_back(getDriverIncludeDir(b, lib) / getSw1Header());
            c->ForcedIncludeFiles().push_back(getDriverIncludeDir(b, lib) / getSwCheckAbiVersionHeader());
        }
    }

    commonActions2(b, lib);
}

bool PrepareConfigEntryPoint::isOutdated() const
{
    auto get_lwt = [](const path &p)
    {
        return file_time_type2time_t(fs::last_write_time(p));
    };

    bool not_exists = false;
    size_t t0 = 0;
    size_t t = 0;
    hash_combine(t, get_lwt(boost::dll::program_location()));

    for (auto &f : pkg_files_)
        hash_combine(t, get_lwt(f));
    for (auto &f : FilesSorted(files_.begin(), files_.end()))
        hash_combine(t, get_lwt(f));

    if (!out.empty())
    {
        not_exists |= !fs::exists(out);
        if (!not_exists)
            hash_combine(t, get_lwt(out));
    }
    else
    {
        LOG_INFO(logger, __FILE__ << ":" << __LINE__ <<  ": not implemeted yet");
        return true;
    }

    auto f = path(".sw") / "stamp" / (std::to_string(t) + ".txt");
    if (fs::exists(f))
        t0 = std::stoull(read_file(f));
    write_file(f, std::to_string(t));
    return not_exists || t0 != t;
}

}
