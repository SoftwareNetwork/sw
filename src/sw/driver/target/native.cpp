// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "native.h"

#include "../suffix.h"
#include "../bazel/bazel.h"
#include "../functions.h"
#include "../build.h"
#include "../extensions.h"
#include "../command.h"
#include "../rule.h"
#include "../compiler/detect.h"
#include "../compiler/set_settings.h"

#include <sw/builder/jumppad.h>
#include <sw/core/sw_context.h>
#include <sw/manager/storage.h>
#include <sw/manager/yaml.h>

#include <boost/algorithm/string.hpp>
#include <nlohmann/json.hpp>
#include <primitives/constants.h>
#include <primitives/emitter.h>
#include <primitives/debug.h>
#include <primitives/lock.h>
#include <pystring.h>

#include <charconv>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "target.native");

#define RETURN_PREPARE_MULTIPASS_NEXT_PASS SW_RETURN_MULTIPASS_NEXT_PASS(prepare_pass)
#define RETURN_INIT_MULTIPASS_NEXT_PASS SW_RETURN_MULTIPASS_NEXT_PASS(init_pass)

static int copy_file(path in, path out)
{
    error_code ec;
    fs::create_directories(out.parent_path());
    fs::copy_file(in, out, fs::copy_options::overwrite_existing, ec);
    return !!ec;
}
SW_DEFINE_VISIBLE_FUNCTION_JUMPPAD(sw_copy_file, copy_file)

static int remove_file(path f)
{
    std::error_code ec;
    fs::remove(f, ec);
    return 0;
}
SW_DEFINE_VISIBLE_FUNCTION_JUMPPAD(sw_remove_file, remove_file)

const int symbol_len_max = 240; // 256 causes errors
const int symbol_len_len = 2; // 256 causes errors

#ifdef _WIN32
#include <DbgHelp.h>

static DWORD rva2Offset(DWORD rva, PIMAGE_SECTION_HEADER psh, PIMAGE_NT_HEADERS pnt)
{
    if (!rva)
        return rva;
    auto pSeh = psh;
    for (WORD i = 0; i < pnt->FileHeader.NumberOfSections; i++)
    {
        if (rva >= pSeh->VirtualAddress && rva < pSeh->VirtualAddress + pSeh->Misc.VirtualSize)
            break;
        pSeh++;
    }
    return rva - pSeh->VirtualAddress + pSeh->PointerToRawData;
}

static int replace_dll_import(path in, path out, Strings indlls)
{
    if (indlls.empty())
    {
        fs::copy_file(in, out, fs::copy_options::overwrite_existing);
        return 0;
    }

    if (indlls.size() % 2 == 1)
        throw SW_RUNTIME_ERROR("Number of inputs is not even");

    std::map<String, String> dlls;
    for (int i = 0; i < indlls.size(); i += 2)
        dlls[indlls[i]] = indlls[i+1];

    auto f = read_file(in);
    void *h = f.data();

    auto ntheaders = (PIMAGE_NT_HEADERS)(PCHAR(h) + PIMAGE_DOS_HEADER(h)->e_lfanew);
    auto pSech = IMAGE_FIRST_SECTION(ntheaders);

    ULONG sz;
    PIMAGE_SECTION_HEADER sh;
    auto pImportDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)ImageDirectoryEntryToDataEx(
        h, false, IMAGE_DIRECTORY_ENTRY_IMPORT, &sz, &sh);
    if (!pImportDescriptor)
        throw SW_RUNTIME_ERROR("Bad import descriptor");

    while (pImportDescriptor->Name)
    {
        auto ptr = (PCHAR)((DWORD_PTR)h + rva2Offset(pImportDescriptor->Name, pSech, ntheaders));
        String s = ptr;
        int sz;
        if (auto [p, ec] = std::from_chars(ptr, ptr + symbol_len_len, sz, 16); ec == std::errc() && p == ptr + symbol_len_len)
        {
            s = s.substr(symbol_len_len, sz);
            auto i = dlls.find(s);
            if (i != dlls.end())
            {
                auto &repl = i->second;
                if (repl.size() > symbol_len_max)
                {
                    throw SW_RUNTIME_ERROR("replacement size (" + std::to_string(sz) +
                        ") is greater than max (" + std::to_string(symbol_len_max) + ")");
                }
                memcpy(ptr, repl.data(), repl.size() + 1);
            }
        }
        pImportDescriptor++;
    }

    write_file(out, f);
    return 0;
}
SW_DEFINE_VISIBLE_FUNCTION_JUMPPAD(sw_replace_dll_import, replace_dll_import)

#endif

// these are the same on win/macos, maybe change somehow?
static const Strings include_dir_names =
{
    // sort by rarity
    "include",
    "includes",

    "Include",
    "Includes",

    "headers",
    "Headers",

    "inc",
    "Inc",

    "hdr",
    "Hdr",
};

// these are the same on win/macos, maybe change somehow?
static const Strings source_dir_names =
{
    // sort by rarity
    "src",
    "source",
    "sources",
    "lib",
    "library",

    "Src",
    "Source",
    "Sources",
    "Lib",
    "Library",

    // keep the empty entry at the end
    // this will add current source dir as include directory
    "",
};

namespace sw
{

NativeTarget::NativeTarget(TargetBase &parent, const PackageId &id)
    : Target(parent, id)
{
}

NativeTarget::~NativeTarget()
{
}

void NativeTarget::setOutputFile()
{
    if (!isLocal())
    try
    {
        if (!fs::exists(BinaryDir.parent_path() / "cfg.json"))
            write_file(BinaryDir.parent_path() / "cfg.json", nlohmann::json::parse(ts.toString(TargetSettings::Json)).dump(4));
    }
    catch (...) {} // write once
}

path NativeTarget::getOutputFileName(const path &root) const
{
    return getBaseOutputFileNameForLocalOnly(*this, root, OutputDir);
}

path NativeTarget::getOutputFileName2(const path &subdir) const
{
    return getBaseOutputFileName(*this, OutputDir, subdir);
}

path NativeTarget::getOutputFile() const
{
    SW_UNIMPLEMENTED;
}

void NativeTarget::addRuleDependencyRaw(const String &name, const DependencyPtr &from_dep, const String &from_name)
{
    rule_dependencies[name].dep = from_dep;
    rule_dependencies[name].target_rule_name = from_name;
}

void NativeTarget::addRuleDependency(const String &name, const DependencyPtr &from_dep, const String &from_name)
{
    addDummyDependency(from_dep);
    addRuleDependencyRaw(name, from_dep, from_name);
}

void NativeTarget::addRuleDependency(const String &name, const DependencyPtr &from_dep)
{
    addRuleDependency(name, from_dep, name);
}

void NativeTarget::addRuleDependency(const String &name, const UnresolvedPackage &from_dep)
{
    addRuleDependency(name, std::make_shared<Dependency>(from_dep));
}

void NativeTarget::addRuleDependency(const String &name)
{
    addRuleDependency(name, getSettings()["rule"][name]["package"].getValue());
}

DependencyPtr NativeTarget::getRuleDependency(const String &name) const
{
    auto i = rule_dependencies.find(name);
    if (i == rule_dependencies.end())
        throw SW_RUNTIME_ERROR("No rule dep: " + name);
    return i->second.dep;
}

IRulePtr NativeTarget::getRuleFromDependency(const String &ruledepname, const String &rulename) const
{
    auto dep = getRuleDependency(ruledepname);
    if (auto t = dep->getTarget().as<PredefinedProgram *>())
        return t->getRule1(rulename);
    else
        SW_UNIMPLEMENTED;
}

IRulePtr NativeTarget::getRuleFromDependency(const String &rulename) const
{
    return getRuleFromDependency(rulename, rulename);
}

NativeCompiledTarget::NativeCompiledTarget(TargetBase &parent, const PackageId &id)
    : NativeTarget(parent, id), NativeTargetOptionsGroup((Target &)*this)
{
}

NativeCompiledTarget::~NativeCompiledTarget()
{
    // incomplete type cannot be in default dtor
    // in our case it is nlohmann::json member
}

path NativeCompiledTarget::getBinaryParentDir() const
{
    if (IsSwConfigLocal)
        return getTargetDirShort(getMainBuild().getBuildDirectory() / "cfg");
    return Target::getBinaryParentDir();
}

path NativeCompiledTarget::getOutputFileName(const path &root) const
{
    path p;
    if (IsSwConfig)
    {
        path root;
        if (IsSwConfigLocal)
            root = getMainBuild().getBuildDirectory();
        else
            root = getContext().getLocalStorage().storage_dir_tmp;
        p = root / "cfg" / getConfig() / ::sw::getOutputFileName(*this);
    }
    else
    {
        p = NativeTarget::getOutputFileName(root);
    }
    return p;
}

path NativeCompiledTarget::getOutputFileName2(const path &subdir) const
{
    if (IsSwConfig)
        return getOutputFileName("");
    else
        return NativeTarget::getOutputFileName2(subdir);
}

bool NativeCompiledTarget::isStaticLibrary() const
{
    return getType() == TargetType::NativeStaticLibrary;
}

bool NativeCompiledTarget::isStaticOrHeaderOnlyLibrary() const
{
    return isStaticLibrary() || isHeaderOnly();
}

static bool isStaticOrHeaderOnlyLibrary(const TargetSettings &s)
{
    return s["header_only"] == "true" || s["type"] == "native_static_library";
}

void NativeCompiledTarget::setOutputDir(const path &dir)
{
    //SwapAndRestore sr(OutputDir, dir);
    OutputDir = dir;
    setOutputFile();
}

static void targetSettings2Command(primitives::Command &c, const TargetSetting &s)
{
    if (s["program"])
        c.setProgram(s["program"].getValue());

    if (s["arguments"])
    {
        for (auto &a : s["arguments"].getArray())
        {
            if (a.isValue())
                c.push_back(a.getValue());
            else
            {
                auto &m = a.getMap();

                auto a2 = std::make_unique<::primitives::command::SimplePositionalArgument>(m["argument"].getValue());
                if (m["position"].isValue())
                    a2->getPosition().push_back(std::stoi(m["position"].getValue()));
                else if (m["position"].isArray())
                {
                    for (auto &p : m["position"].getArray())
                        a2->getPosition().push_back(std::stoi(p.getValue()));
                }
                c.push_back(std::move(a2));
            }
        }
    }
}

static auto get_settings_package_id(const TargetSetting &s)
{
    if (!s)
        throw SW_RUNTIME_ERROR("Empty setting");
    bool extended_desc = s.isObject();
    UnresolvedPackage id;
    if (extended_desc)
        id = s["package"].getValue();
    else
        id = s.getValue();
    return id;
}

static auto get_compiler_type(const String &p)
{
    auto t = CompilerType::Unspecified;
    if (0);
    else if (p == "msvc")
        t = CompilerType::MSVC;
    else if (p == "clang")
        t = CompilerType::Clang;
    else if (p == "clangcl")
        t = CompilerType::ClangCl;
    else if (p == "appleclang")
        t = CompilerType::AppleClang;
    else if (p == "gnu")
        t = CompilerType::GNU;
    return t;
}

static auto get_linker_type(const String &p)
{
    auto t = LinkerType::Unspecified;
    if (0);
    else if (p == "msvc")
        t = LinkerType::MSVC;
    return t;
}

std::unique_ptr<NativeCompiler> NativeCompiledTarget::activateCompiler(const TargetSetting &s, const UnresolvedPackage &id, const StringSet &exts, bool extended_desc)
{
    SW_UNIMPLEMENTED;

    /*auto create_command = [this, &created, &t, &s, extended_desc](auto &c)
    {
        if (created)
            return;
        c->file = t->getProgram().file;
        auto C = c->createCommand(getMainBuild());
        static_cast<primitives::Command&>(*C) = *t->getProgram().getCommand();
        created = true;

        if (extended_desc && s["command"])
            targetSettings2Command(*C, s["command"]);
    };*/

    /*else if (id.ppath == "org.gnu.gcc" || id.ppath == "org.gnu.gpp")
    {
        c = std::make_unique<GNUCompiler>();
        auto &nc = (GNUCompiler&)*c;
        if (getBuildSettings().TargetOS.isApple())
        {
            if (getBuildSettings().TargetOS.Version)
            {
                auto c = nc.createCommand(getMainBuild());
                c->push_back("-mmacosx-version-min=" + getBuildSettings().TargetOS.Version->toString());
            }
            //C->VisibilityHidden = false;
            //C->VisibilityInlinesHidden = false;
            //C->PositionIndependentCode = false;
        }
    }
    else if (id.ppath == "com.intel.compiler.c" || id.ppath == "com.intel.compiler.cpp")
    {
        c = std::make_unique<VisualStudioCompiler>();
        auto &nc = (VisualStudioCompiler&)*c;
        nc.ForceSynchronousPDBWrites = false;
        if (getSettings()["native"]["stdlib"]["cpp"].getValue() == "com.Microsoft.VisualStudio.VC.libcpp")
        {
            // take same ver as cl
            UnresolvedPackage up(getSettings()["native"]["stdlib"]["cpp"].getValue());
            up.range = id.range;
            *this += up;
            libstdcppset = true;
        }
    }*/
}

std::unique_ptr<NativeLinker> NativeCompiledTarget::activateLinker(const TargetSetting &s, const UnresolvedPackage &id, bool extended_desc)
{
    SW_UNIMPLEMENTED;
    /*
        auto C = std::make_unique<GNULinker>();
        // actually it is depends on -fuse-ld option
        // do we need it at all?
        // probably yes, because user might provide different commands to ld and lld
        // is it true?
        C->Type = LinkerType::GNU;
        C->Prefix = getBuildSettings().TargetOS.getLibraryPrefix();
        if (getBuildSettings().TargetOS.isApple())
        {
            C->use_start_end_groups = false;

            // for linker also!
            if (getBuildSettings().TargetOS.Version)
            {
                auto c = C->createCommand(getMainBuild());
                c->push_back("-mmacosx-version-min=" + getBuildSettings().TargetOS.Version->toString());
            }
        }

        //
        c = std::move(C);

        if (lt == LinkerType::LLD)
        {
            create_command();
            auto cmd = c->createCommand(getMainBuild());
            cmd->push_back("-target");
            cmd->push_back(getBuildSettings().getTargetTriplet());
        }
        // TODO: find -fuse-ld option and set c->Type accordingly
    */
}

std::unique_ptr<NativeLinker> NativeCompiledTarget::activateLibrarian(LinkerType t)
{
    SW_UNIMPLEMENTED;
    /*
    else if (t != LinkerType::Unspecified)
    {
        auto C = std::make_unique<GNULibrarian>();
        C->Type = LinkerType::GNU;
        C->Prefix = getBuildSettings().TargetOS.getLibraryPrefix();
        c = std::move(C);
    }
    */
}

std::unique_ptr<NativeLinker> NativeCompiledTarget::activateLinker(LinkerType t)
{
    SW_UNIMPLEMENTED;
    /*
    else if (t != LinkerType::Unspecified)
    {
        auto C = std::make_unique<GNULinker>();
        // actually it is depends on -fuse-ld option
        // do we need it at all?
        // probably yes, because user might provide different commands to ld and lld
        // is it true?
        C->Type = LinkerType::GNU;
        C->Prefix = getBuildSettings().TargetOS.getLibraryPrefix();
        if (getBuildSettings().TargetOS.isApple())
        {
            C->use_start_end_groups = false;

            // for linker also!
            if (getBuildSettings().TargetOS.Version)
            {
                auto c = C->createCommand(getMainBuild());
                c->push_back("-mmacosx-version-min=" + getBuildSettings().TargetOS.Version->toString());
            }
        }

        //
        c = std::move(C);

        if (t == LinkerType::LLD)
        {
            create_command();
            auto cmd = c->createCommand(getMainBuild());
            cmd->push_back("-target");
            cmd->push_back(getBuildSettings().getTargetTriplet());
        }
        // TODO: find -fuse-ld option and set c->Type accordingly
    }
    */
}

void NativeCompiledTarget::findCompiler()
{
    ct = get_compiler_type(getSettings()["rule"]["cpp"]["type"].getValue());
    if (ct == CompilerType::Unspecified)
        ct = get_compiler_type(getSettings()["rule"]["c"]["type"].getValue());
    if (ct == CompilerType::Unspecified)
        throw SW_RUNTIME_ERROR("Cannot determine compiler type " + get_settings_package_id(getSettings()["rule"]["cpp"]["package"]).toString() + " for settings: " + getSettings().toString());

    lt = get_linker_type(getSettings()["rule"]["link"]["type"].getValue());
    if (lt == LinkerType::Unspecified)
        throw SW_RUNTIME_ERROR("Cannot determine compiler type " + get_settings_package_id(getSettings()["rule"]["link"]["package"]).toString() + " for settings: " + getSettings().toString());
}

bool NativeCompiledTarget::init()
{
    // before target init
    addSettingsAndSetPrograms((SwContext&)getContext(), ts);

    if (!isHeaderOnly())
        findCompiler();

    // after compilers
    Target::init();

    if (getSettings()["export-if-static"] == "true")
    {
        ExportIfStatic = true;
        getExportOptions()["export-if-static"].use();
    }

    if (getSettings()["static-deps"] == "true")
    {
        getExportOptions()["native"]["library"] = "static";
        getExportOptions()["static-deps"].use();
    }

    addPackageDefinitions();
    setOutputFile();

    SW_RETURN_MULTIPASS_END(init_pass);
}

void NativeCompiledTarget::setupCommand(builder::Command &c) const
{
    NativeTarget::setupCommand(c);

    // default win32 paths are not enough
    if (getBuildSettings().TargetOS.is(OSType::Mingw))
    {
        // we must find reliable anchor here (.exe or .dll) that is present in all mingw setups
        // use gcc for now
        static auto p = resolveExecutable("gcc");
        if (p.empty())
            throw SW_RUNTIME_ERROR("Mingw PATH: cannot add default bin dir");
        c.addPathDirectory(p.parent_path());
    }

    // perform this after prepare?
    auto for_deps = [this, &c](auto f)
    {
        for (auto &d : all_deps_normal)
        {
            if (&d->getTarget() == this)
                continue;
            if (auto nt = d->getTarget().as<NativeCompiledTarget *>())
            {
                if (!isStaticOrHeaderOnlyLibrary())
                    f(nt->getOutputFile());
            }
            else if (auto nt = d->getTarget().as<PredefinedTarget *>())
            {
                auto &ts = nt->getInterfaceSettings();
                if (ts["header_only"] != "true" && ts["type"] == "native_shared_library")
                {
                    f(ts["output_file"].getPathValue(getContext().getLocalStorage()));
                }
            }
            else
                throw SW_RUNTIME_ERROR("missing predefined target code");
        }
    };

    if (1/*getMainBuild().getSettings()["standalone"] == "true"*/)
    {
        for_deps([this, &c](const path &output_file)
        {
            if (getContext().getHostOs().is(OSType::Windows))
                c.addPathDirectory(output_file.parent_path());
            // disable for now, because we set rpath
            //else if (getContext().getHostOs().isApple())
                //c.environment["DYLD_LIBRARY_PATH"] += normalize_path(output_file.parent_path()) + ":";
            //else // linux and others
                //c.environment["LD_LIBRARY_PATH"] += normalize_path(output_file.parent_path()) + ":";
        });
        return;
    }

    // more under if (createWindowsRpath())?
    c.addPathDirectory(getContext().getLocalStorage().storage_dir);

    if (createWindowsRpath())
    {
        for_deps([&c](const path &output_file)
        {
            // dlls, when emulating rpath, are created after executables and commands running them
            // so we put explicit dependency on them
            c.addInput(output_file);
        });
    }
}

void NativeCompiledTarget::addPackageDefinitions(bool defs)
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
        a["PACKAGE"] = q + getPackage().getPath().toString() + q;
        a["PACKAGE_NAME"] = q + getPackage().getPath().toString() + q;
        a["PACKAGE_NAME_LAST"] = q + getPackage().getPath().back() + q;
        a["PACKAGE_VERSION"] = q + getPackage().getVersion().toString() + q;
        a["PACKAGE_STRING"] = q + getPackage().toString() + q;
        a["PACKAGE_BUILD_CONFIG"] = q + getConfig() + q;
        a["PACKAGE_BUGREPORT"] = q + q;
        a["PACKAGE_URL"] = q + q;
        a["PACKAGE_SUFFIX"] = q + q;
        a["PACKAGE_DATADIR"] = q + q;
        a["PACKAGE_TARNAME"] = q + getPackage().getPath().toString() + q; // must be lowercase version of PACKAGE_NAME
        a["PACKAGE_VENDOR"] = q + getPackage().getPath().getOwner() + q;
        a["PACKAGE_YEAR"] = std::to_string(1900 + t.tm_year); // custom
        a["PACKAGE_COPYRIGHT_YEAR"] = std::to_string(1900 + t.tm_year);

        a["PACKAGE_ROOT_DIR"] = q + to_string(normalize_path(getPackage().getPath().is_loc() ? RootDirectory : getPackage().getDirSrc())) + q;
        a["PACKAGE_NAME_WITHOUT_OWNER"] = q/* + getPackage().getPath().slice(2).toString()*/ + q;
        a["PACKAGE_NAME_CLEAN"] = q + (getPackage().getPath().is_loc() ? getPackage().getPath().slice(2).toString() : getPackage().getPath().toString()) + q;

        //"@PACKAGE_CHANGE_DATE@"
            //"@PACKAGE_RELEASE_DATE@"

        a["PACKAGE_VERSION_MAJOR"] = std::to_string(getPackage().getVersion().getMajor());
        a["PACKAGE_VERSION_MINOR"] = std::to_string(getPackage().getVersion().getMinor());
        a["PACKAGE_VERSION_PATCH"] = std::to_string(getPackage().getVersion().getPatch());
        a["PACKAGE_VERSION_TWEAK"] = std::to_string(getPackage().getVersion().getTweak());
        a["PACKAGE_VERSION_NUM"] = "0x" + ver2hex(getPackage().getVersion(), 2) + "LL";
        a["PACKAGE_VERSION_MAJOR_NUM"] = n2hex(getPackage().getVersion().getMajor(), 2);
        a["PACKAGE_VERSION_MINOR_NUM"] = n2hex(getPackage().getVersion().getMinor(), 2);
        a["PACKAGE_VERSION_PATCH_NUM"] = n2hex(getPackage().getVersion().getPatch(), 2);
        a["PACKAGE_VERSION_TWEAK_NUM"] = n2hex(getPackage().getVersion().getTweak(), 2);
        a["PACKAGE_VERSION_NUM2"] = "0x" + ver2hex(getPackage().getVersion(), 4) + "LL";
        a["PACKAGE_VERSION_MAJOR_NUM2"] = n2hex(getPackage().getVersion().getMajor(), 4);
        a["PACKAGE_VERSION_MINOR_NUM2"] = n2hex(getPackage().getVersion().getMinor(), 4);
        a["PACKAGE_VERSION_PATCH_NUM2"] = n2hex(getPackage().getVersion().getPatch(), 4);
        a["PACKAGE_VERSION_TWEAK_NUM2"] = n2hex(getPackage().getVersion().getTweak(), 4);
    };

    // https://www.gnu.org/software/autoconf/manual/autoconf-2.67/html_node/Initializing-configure.html
    if (defs)
    {
        set_pkg_info(Definitions, true); // false?
        PackageDefinitions = false;
    }
    else
        set_pkg_info(Variables, false); // false?
}

void NativeCompiledTarget::add(const ApiNameType &i)
{
    ApiNames.insert(i.a);
}

void NativeCompiledTarget::remove(const ApiNameType &i)
{
    ApiNames.erase(i.a);
    if (ApiName == i.a)
        ApiName.clear();
}

TargetFiles NativeCompiledTarget::getFiles(StorageFileType t) const
{
    switch (t)
    {
    case StorageFileType::SourceArchive:
    {
        return Target::getFiles(t);
    }
    case StorageFileType::BinaryArchive:
    {
        TargetFiles files;
        auto add_file = [this, &files](const path &f)
        {
            files.emplace(f, TargetFile(f/*, getPackage().getDirObj()*/, File(f, getFs()).isGenerated()));
        };
        add_file(getOutputFile());
        add_file(getImportLibrary());
        return files;
    }
    }
    SW_UNIMPLEMENTED;
}

bool NativeCompiledTarget::isHeaderOnly() const
{
    return HeaderOnly && *HeaderOnly;
}

path NativeCompiledTarget::getOutputDir1() const
{
    if (OutputDir.empty())
        return getOutputFile().parent_path();
    return getLocalOutputBinariesDirectory() / OutputDir;
}

void NativeCompiledTarget::setOutputFile()
{
    if (isHeaderOnly())
        return;

    implibfile = getOutputFileName2("lib") += getBuildSettings().TargetOS.getStaticLibraryExtension();
    if (isStaticLibrary())
        outputfile = implibfile;
    else
    {
        if (isExecutable())
            outputfile = getOutputFileName2("bin") += getBuildSettings().TargetOS.getExecutableExtension();
        else
            outputfile = getOutputFileName2("bin") += getBuildSettings().TargetOS.getSharedLibraryExtension();

        if (getBuildSettings().TargetOS.Type != OSType::Windows)
            implibfile = outputfile;
    }

    NativeTarget::setOutputFile();
}

path NativeCompiledTarget::getOutputFile() const
{
    if (!outputfile.empty())
        return outputfile;
    return NativeTarget::getOutputFile();
}

path NativeCompiledTarget::getImportLibrary() const
{
    if (!implibfile.empty())
        return implibfile;
    SW_UNIMPLEMENTED;
}

bool NativeCompiledTarget::hasSourceFiles() const
{
    bool r = false;

    auto exts = get_cpp_exts(getBuildSettings().TargetOS.isApple());
    exts.insert(".c");
    exts.insert(".def");
    exts.insert(getBuildSettings().TargetOS.getObjectFileExtension());

    auto check = [this, &r, &exts](auto &o)
    {
        if (!r)
        {
            r |= std::any_of(o.begin(), o.end(), [&exts](const auto &f) {
                return f.second->isActive() && exts.contains(f.second->file.extension().string());
            });
        }
    };

    TargetOptionsGroup::iterate([this, &check](auto &v, auto i)
    {
        if (((int)i & (int)InheritanceScope::Package) == 0)
            return;
        check(v);
    });
    check(getMergeObject());
    return r;
}

void NativeCompiledTarget::createPrecompiledHeader()
{
    // disabled with PP
    if (PreprocessStep)
        return;

    auto &files = getMergeObject().PrecompiledHeaders;
    if (files.empty())
        return;

    if (pch.name.empty())
        pch.name = "sw.pch";
    if (pch.dir.empty())
        pch.dir = BinaryDir.parent_path() / "pch";

    String h;
    for (auto &f : files)
    {
        if (f.string()[0] == '<' || f.string()[0] == '\"')
            h += "#include " + f.string() + "\n";
        else
            h += "#include \"" + to_string(normalize_path(f)) + "\"\n";
    }
    pch.header = pch.get_base_pch_path() += ".h";
    {
        ScopedFileLock lk(pch.header);
        write_file_if_different(pch.header, h);
    }

    pch.source = pch.get_base_pch_path() += ".cpp"; // msvc
    {
        ScopedFileLock lk(pch.source);
        write_file_if_different(pch.source, "");
    }

    // pch goes first on force includes list
    getMergeObject().ForceIncludes.insert(getMergeObject().ForceIncludes.begin(), pch.header);

    //
    if (pch.pch.empty())
    {
        if (getCompilerType() == CompilerType::MSVC || getCompilerType() == CompilerType::ClangCl)
            pch.pch = pch.get_base_pch_path() += ".pch";
        else if (isClangFamily(getCompilerType()))
            pch.pch = path(pch.header) += ".pch";
        else // gcc
            pch.pch = path(pch.header) += ".gch";
    }
    if (pch.obj.empty())
        pch.obj = pch.get_base_pch_path() += ".obj";
    if (pch.pdb.empty())
        pch.pdb = pch.get_base_pch_path() += ".pdb";

    //
    getMergeObject() += pch.source;
    if (!pch.fancy_name.empty())
        getMergeObject()[pch.source].fancy_name = pch.fancy_name;
    else
        getMergeObject()[pch.source].fancy_name = "[" + getPackage().toString() + "]/[pch]";
}

Commands NativeCompiledTarget::getGeneratedCommands() const
{
    if (generated_commands)
        return generated_commands.value();
    generated_commands = Target::generated_commands1;
    return *generated_commands;

    Commands generated;

    // still some generated commands must be run before others,
    // (syncqt must be run before mocs when building qt)
    // so we introduce this order
    std::map<int, std::vector<std::shared_ptr<builder::Command>>> order;

    // add generated commands
    for (auto &[f, _] : getMergeObject())
    {
        File p(f, getFs());
        if (!p.isGenerated())
            continue;
        /*auto c = p.getGenerator();
        if (c->strict_order > 0)
            order[c->strict_order].push_back(c);
        else
            generated.insert(c);*/
    }

    // respect ordering
    /*for (auto i = order.rbegin(); i != order.rend(); i++)
    {
        auto &cmds = i->second;
        for (auto &c : generated)
            c->dependencies.insert(cmds.begin(), cmds.end());
        generated.insert(cmds.begin(), cmds.end());
    }*/

    generated_commands = generated;
    return generated;
}

Commands NativeCompiledTarget::getCommands1() const
{
    //if (getSolution().skipTarget(Scope))
        //return {};
    if (DryRun)
        return {};
    if (already_built)
        return {};

    // add generated files
    auto generated = getGeneratedCommands();

    Commands cmds;
    if (isHeaderOnly())
    {
        cmds.merge(generated);
        return cmds;
    }

    //
    cmds.merge(RuleSystem::getRuleCommands());

    // add generated files
    for (auto &cmd : cmds)
    {
        for (auto &g : generated)
            cmd->dependencies.insert(g.get());
    }
    cmds.merge(generated);

    // deps' generated commands
    auto get_tgts = [this]()
    {
        TargetsSet deps;
        for (auto &d : all_deps_normal)
            deps.insert(&d->getTarget());
        for (auto &d : all_deps_idir_only)
        {
            // this means that for idirs generated commands won't be used!
            if (!d->GenerateCommandsBefore)
                continue;
            deps.insert(&d->getTarget());
        }
        return deps;
    };

    // add dependencies on generated commands from dependent targets
    for (auto &l : get_tgts())
    {
        if (auto nt = l->as<NativeCompiledTarget*>())
        {
            // for idir deps generated commands won't be used!
            auto cmds2 = nt->getGeneratedCommands();
            for (auto &c : cmds)
            {
                for (auto &g : cmds2)
                    c->dependencies.insert(g.get());
            }
        }
    }

    return cmds;




    // link deps
    if (hasCircularDependency() || createWindowsRpath())
        SW_UNIMPLEMENTED;
        //cmds.insert(Librarian->getCommand(*this));

    //cmds.insert(this->cmds.begin(), this->cmds.end());
    return cmds;
}

bool NativeCompiledTarget::hasCircularDependency() const
{
    return circular_dependency;
}

bool NativeCompiledTarget::createWindowsRpath() const
{
    // http://nibblestew.blogspot.com/2019/05/emulating-rpath-on-windows-via-binary.html

    //SW_UNIMPLEMENTED;
    return false;
    return
        1
        && !IsSwConfig
        && getBuildSettings().TargetOS.is(OSType::Windows)
        //&& getSelectedTool() == Linker.get()
        && 0//!(getMainBuild().getSettings()["standalone"] == "true")
        ;
}

void NativeCompiledTarget::findSources()
{
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
            throw SW_RUNTIME_ERROR("No bazel file found in SourceDir: " + to_string(normalize_path(SourceDir)));

        auto b = read_file(bfn);
        auto f = bazel::parse(b);

        /*static std::mutex m;
        static std::unordered_map<String, bazel::File> files;
        auto h = sha1(b);
        auto i = files.find(h);
        bazel::File *f = nullptr;
        if (i == files.end())
        {
            std::unique_lock lk(m);
            files[h] = bazel::parse(b);
            f = &files[h];
        }
        else
            f = &i->second;*/

        String project_name;
        if (!getPackage().getPath().empty())
            project_name = getPackage().getPath().back();
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

    // we autodetect even if already built
    if (!AutoDetectOptions || (AutoDetectOptions && AutoDetectOptions.value()))
        autoDetectOptions();

    detectLicenseFile();
}

void NativeCompiledTarget::autoDetectOptions()
{
    autodetect = true;

    autoDetectSources(); // sources first
    autoDetectIncludeDirectories();
}

void NativeCompiledTarget::autoDetectSources()
{
    // gather things to check
    //bool sources_empty = gatherSourceFiles().empty();
    bool sources_empty = sizeKnown() == 0;

    if (!(sources_empty && !already_built))
        return;

    // make additional log level for this
    //LOG_TRACE(logger, getPackage().toString() + ": Autodetecting sources");

    // all files except starting from point
    static const auto files_regex = "[^\\.].*";

    bool added = false;
    for (auto &d : include_dir_names)
    {
        if (fs::exists(SourceDir / d))
        {
            // add files for non building
            remove(FileRegex(d, std::regex(files_regex), true));
            added = true;
            break; // break here!
        }
    }
    for (auto &d : source_dir_names)
    {
        if (fs::exists(SourceDir / d))
        {
            // if build dir is "" or "." we do not do recursive search
            add(FileRegex(d, std::regex(files_regex), d != ""s));
            added = true;
            break; // break here!
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

        static const std::set<String> other_source_file_extensions{
            ".s",
            ".sx",
            ".S",
            ".asm",
            ".ipp",
            ".inl",
        };

        static auto source_file_extensions = []()
        {
            auto source_file_extensions = getCppSourceFileExtensions();
            source_file_extensions.insert(".c");
            return source_file_extensions;
        }();

        for (auto &v : getCppHeaderFileExtensions())
            add(FileRegex(std::regex(".*\\" + escape_regex_symbols(v)), false));
        for (auto &v : source_file_extensions)
            add(FileRegex(std::regex(".*\\" + escape_regex_symbols(v)), false));
        for (auto &v : other_source_file_extensions)
            add(FileRegex(std::regex(".*\\" + escape_regex_symbols(v)), false));
    }

    // erase config file, add a condition to not perform this code
    // get proper config file
    path f = "sw.cpp";
    if (check_absolute(f, true))
        operator^=(f);
}

void NativeCompiledTarget::autoDetectIncludeDirectories()
{
    auto &is = getInheritanceStorage().raw();
    if (std::any_of(is.begin(), is.end(), [this](auto *ptr)
    {
        if (!ptr || ptr->IncludeDirectories.empty())
            return false;
        return !std::all_of(ptr->IncludeDirectories.begin(), ptr->IncludeDirectories.end(), [this](const auto &i)
        {
            // tools may add their idirs to bdirs
            return
                i.u8string().find(BinaryDir.u8string()) == 0 ||
                i.u8string().find(BinaryPrivateDir.u8string()) == 0;
        });
    }))
    {
        return;
    }

    // make additional log level for this
    //LOG_TRACE(logger, getPackage().toString() + ": Autodetecting include dirs");

    // public idirs
    for (auto &d : include_dir_names)
    {
        if (fs::exists(SourceDir / d))
        {
            Public.IncludeDirectories.insert(SourceDir / d);
            break;
        }
    }

    // source (private) idirs
    for (auto &d : source_dir_names)
    {
        if (!fs::exists(SourceDir / d))
            continue;

        if (!Public.IncludeDirectories.empty())
            Private.IncludeDirectories.insert(SourceDir / d);
        else
            Public.IncludeDirectories.insert(SourceDir / d);
        break;
    }
}

void NativeCompiledTarget::detectLicenseFile()
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
            throw SW_RUNTIME_ERROR(err);
        };
        if (!name.is_absolute())
            name = SourceDir / name;
        if (!fs::exists(name))
            return license_error("license does not exists");
        if (fs::file_size(name) > 512_KB)
            return license_error("license is invalid (should be text/plain and less than 512 KB)");
        return true;
    };

    if (!isLocal())
    {
        if (!Description.LicenseFilename.empty())
        {
            if (check_license(Description.LicenseFilename))
                add(Description.LicenseFilename);
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

static auto createDependency(const DependencyPtr &d, InheritanceType i, const Target &t)
{
    TargetDependency td;
    td.dep = d;
    td.inhtype = i;
    td.dep->settings.mergeMissing(t.getExportOptions());
    /*auto s = td.dep->settings;
    td.dep->settings.mergeAndAssign(t.getExportOptions());
    td.dep->settings.mergeAndAssign(s);*/
    return td;
}

DependenciesType NativeCompiledTarget::gatherDependencies() const
{
    // take all
    // while getActiveDependencies() takes only active
    std::vector<TargetDependency> deps;
    TargetOptionsGroup::iterate([this, &deps](auto &v, auto i)
    {
        for (auto &d : v.getRawDependencies())
            deps.push_back(createDependency(d, i, *this));
    });
    DependenciesType deps2;
    for (auto &d : deps)
        deps2.insert(d.dep);
    return deps2;
}

NativeCompiledTarget::ActiveDeps &NativeCompiledTarget::getActiveDependencies()
{
    if (!active_deps)
    {
        ActiveDeps deps;
        if (!DryRun)
        {
            TargetOptionsGroup::iterate([this, &deps](auto &v, auto i)
            {
                for (auto &d : v.getRawDependencies())
                {
                    if (d->isDisabled())
                        continue;
                    deps.push_back(createDependency(d, i, *this));
                }
            });
        }
        active_deps = deps;
    }
    return *active_deps;
}

const NativeCompiledTarget::ActiveDeps &NativeCompiledTarget::getActiveDependencies() const
{
    if (!active_deps)
        throw SW_RUNTIME_ERROR(getPackage().toString() + ": no active deps calculated");
    return *active_deps;
}

const TargetSettings &NativeCompiledTarget::getInterfaceSettings() const
{
    // Do not export any private information.
    // It MUST be extracted from getCommands() call.

    auto &s = interface_settings;
    // info may change during prepare, so we create it every time for now
    // TODO: deny calls during prepare()
    if (interface_settings_set)
        return s;

    bool prepared = prepare_pass_done;
    s = {};

    s["source_dir"].setPathValue(getContext().getLocalStorage(), SourceDirBase);
    s["binary_dir"].setPathValue(getContext().getLocalStorage(), BinaryDir);
    s["binary_private_dir"].setPathValue(getContext().getLocalStorage(), BinaryPrivateDir);

    if (Publish && !*Publish)
        s["skip_upload"] = "true";

    switch (getType())
    {
    case TargetType::NativeExecutable:
        s["type"] = "native_executable";
        break;
    case TargetType::NativeLibrary:
        if (getBuildSettings().Native.LibrariesType == LibraryType::Shared)
            s["type"] = "native_shared_library";
        else
            s["type"] = "native_static_library";
        break;
    case TargetType::NativeStaticLibrary:
        s["type"] = "native_static_library";
        break;
    case TargetType::NativeSharedLibrary:
        s["type"] = "native_shared_library";
        break;
    default:
        SW_UNIMPLEMENTED;
    }

    if (isHeaderOnly())
        s["header_only"] = "true";
    else
    {
        if (getType() != TargetType::NativeExecutable) // skip for exe atm
            s["import_library"].setPathValue(getContext().getLocalStorage(), getImportLibrary());
        s["output_file"].setPathValue(getContext().getLocalStorage(), getOutputFile());
        if (!OutputDir.empty())
            s["output_dir"] = to_string(normalize_path(OutputDir));
    }

    // remove deps section?
    if (prepared)
    {
        for (auto &d : getActiveDependencies())
        {
            if (d.dep->IncludeDirectoriesOnly || d.dep->LinkLibrariesOnly)
                continue;
            if (auto t = d.dep->getTarget().as<const NativeCompiledTarget *>())
            {
                if (!t->DryRun/* && t->getType() != TargetType::NativeExecutable*/)
                    s["dependencies"]["link"][boost::to_lower_copy(d.dep->getTarget().getPackage().toString())] = d.dep->getTarget().getSettings();
            }
            else
                continue;
                //throw SW_RUNTIME_ERROR("missing predefined target code");
        }
        for (auto &d : DummyDependencies)
        {
            // rename dummy?
            s["dependencies"]["dummy"][boost::to_lower_copy(d->getTarget().getPackage().toString())] = d->getTarget().getSettings();
        }
        for (auto &d : SourceDependencies)
        {
            // commented for now
            //s["dependencies"]["source"].push_back(d->getTarget().getPackage().toString());
        }
    }

    // add ide settings to s["ide"]
    if (StartupProject)
        s["ide"]["startup_project"] = "true";
    for (auto &f : configure_files)
    {
        TargetSetting ts;
        ts.setPathValue(getContext().getLocalStorage(), f);
        s["ide"]["configure_files"].push_back(ts);
    }

    if (getType() == TargetType::NativeExecutable && !isHeaderOnly())
    {
        builder::Command c;
        setupCommandForRun(c);
        if (c.isProgramSet())
            s["run_command"]["program"].setPathValue(getContext().getLocalStorage(), c.getProgram());
        else
            s["run_command"]["program"].setPathValue(getContext().getLocalStorage(), getOutputFile());
        if (!c.working_directory.empty())
            s["run_command"]["working_directory"].setPathValue(getContext().getLocalStorage(), c.working_directory);
        for (auto &a : c.getArguments())
            s["run_command"]["arguments"].push_back(a->toString());
        for (auto &[k, v] : c.environment)
            s["run_command"]["environment"][k] = v;
        if (c.create_new_console)
            s["run_command"]["create_new_console"] = "true";
    }

    // newer settings
    if (prepared)
    {
        auto &ts = s["properties"];
        TargetOptionsGroup::iterate([this, &ts](auto &g, auto i)
        {
            auto is = std::to_string((int)i);
            auto &s = ts[is];

            auto print_deps = [&s](auto &g)
            {
                for (auto &d : g.getRawDependencies())
                {
                    if (d->isDisabled())
                        continue;
                    TargetSettings j;
                    auto &ds = j[boost::to_lower_copy(d->getTarget().getPackage().toString())];
                    ds = d->getTarget().getSettings();
                    if (d->IncludeDirectoriesOnly)
                    {
                        ds["include_directories_only"] = "true";
                        //ds["include_directories_only"].ignoreInComparison(true);
                        ds["include_directories_only"].useInHash(false);
                    }
                    if (d->LinkLibrariesOnly)
                    {
                        ds["link_libraries_only"] = "true";
                        //ds["link_libraries_only"].ignoreInComparison(true);
                        ds["link_libraries_only"].useInHash(false);
                    }
                    s["dependencies"].push_back(j);
                }
            };

            // for private, we skip some variables
            // we do not need them completely
            if (i != InheritanceType::Private)
            {
                for (auto &[p, f] : g)
                {
                    TargetSetting ts;
                    ts.setPathValue(getContext().getLocalStorage(), p);
                    s["source_files"].push_back(ts);
                }

                for (auto &[k, v] : g.Definitions)
                    s["definitions"][k] = v;
                for (auto &d : g.CompileOptions)
                    s["compile_options"].push_back(d);
                for (auto &d : g.IncludeDirectories)
                {
                    TargetSetting ts;
                    ts.setPathValue(getContext().getLocalStorage(), d);
                    s["include_directories"].push_back(ts);
                }
            }

            // for static libs we print their linker settings,
            // so users will take these settings
            if (i != InheritanceType::Private || isStaticOrHeaderOnlyLibrary())
            {
                for (auto &d : g.LinkLibraries)
                {
                    TargetSetting ts;
                    ts.setPathValue(getContext().getLocalStorage(), d.l);
                    s["link_libraries"].push_back(ts);
                }
                for (auto &d : g.NativeLinkerOptions::System.LinkLibraries)
                    s["system_link_libraries"].push_back(d.l);
                for (auto &d : g.Frameworks)
                    s["frameworks"].push_back(d);
            }

            if (i != InheritanceType::Private)
                print_deps(g);
        });
    }

    if (prepared)
        interface_settings_set = true;

    return s;
}

void NativeCompiledTarget::postConfigureActions()
{
    Strings rules = { "c", "cpp", "asm", "lib", "link" };
    if (getBuildSettings().TargetOS.is(OSType::Windows) && getSettings()["rule"]["rc"])
        addRuleDependency("rc");
    for (auto &r : rules)
    {
        // was populated
        if (getRuleDependencies().contains(r))
            continue;

        // actually must check proper rule type
        if (ct == CompilerType::MSVC
            // we take raw settings to get ml instead of ml64 on x86 config
            // and other listed rules (programs) from x86 toolchain
            && (r == "c" || r == "cpp" || r == "asm")
            )
        {
            if (r == "asm" &&
                getBuildSettings().TargetOS.Arch != ArchType::x86 &&
                getBuildSettings().TargetOS.Arch != ArchType::x86_64
                )
                continue;
            auto u = getSettings()["rule"][r]["package"].getValue();
            auto d = std::make_shared<Dependency>(u);
            d->getSettings() = getSettings();
            addRuleDependencyRaw(r, d, r);
            addDummyDependencyRaw(d);
        }
        else
            addRuleDependency(r);
    }

    // c++ goes first for correct include order
    /*UnresolvedPackage cppcl = getSettings()["rule"]["cpp"]["package"].getValue();
    if (cppcl.getPath() == "com.Microsoft.VisualStudio.VC.cl")
    {
        libstdcppset = true;

        // take same ver as cl
        {
            UnresolvedPackage up("com.Microsoft.VisualStudio.VC.libcpp");
            up.range = cppcl.range;
            *this += up;
        }
        {
            UnresolvedPackage up("com.Microsoft.VisualStudio.VC.runtime");
            up.range = cppcl.range;
            *this += up;
        }
    }*/
    if (!libstdcppset && getSettings()["native"]["stdlib"]["cpp"].isValue())
    {
        if (IsSwConfig && getBuildSettings().TargetOS.is(OSType::Linux))
        {
            // to prevent ODR violation
            // we have stdlib builtin into sw binary
            auto d = *this + UnresolvedPackage(getSettings()["native"]["stdlib"]["cpp"].getValue());
            d->IncludeDirectoriesOnly = true;
        }
        else
        {
            *this += UnresolvedPackage(getSettings()["native"]["stdlib"]["cpp"].getValue());
        }
    }

    // goes last
    if (getSettings()["native"]["stdlib"]["c"].isValue())
        *this += UnresolvedPackage(getSettings()["native"]["stdlib"]["c"].getValue());

    // compiler runtime
    if (getSettings()["native"]["stdlib"]["compiler"])
    {
        if (getSettings()["native"]["stdlib"]["compiler"].isValue())
            *this += UnresolvedPackage(getSettings()["native"]["stdlib"]["compiler"].getValue());
        else if (getSettings()["native"]["stdlib"]["compiler"].isArray())
        {
            for (auto &s : getSettings()["native"]["stdlib"]["compiler"].getArray())
                *this += UnresolvedPackage(s.getValue());
        }
    }

    // kernel headers
    if (getSettings()["native"]["stdlib"]["kernel"].isValue())
        *this += UnresolvedPackage(getSettings()["native"]["stdlib"]["kernel"].getValue());
    /*if (getBuildSettings().TargetOS.is(OSType::Windows))
    {
        *this += UnresolvedPackage("com.Microsoft.Windows.SDK.ucrt"s); // c
        *this += UnresolvedPackage("com.Microsoft.Windows.SDK.um"s); // kernel
    }*/

    Target::postConfigureActions();
}

bool NativeCompiledTarget::prepare()
{
    if (!isPostConfigureActionsCalled())
        throw SW_RUNTIME_ERROR("post configure action was not called");

    if (DryRun)
    {
        getActiveDependencies();
        return false;
    }

    switch (prepare_pass)
    {
    case 1:
        prepare_pass1();
    RETURN_PREPARE_MULTIPASS_NEXT_PASS;
    case 2:
        // resolve dependencies
        prepare_pass2();
    RETURN_PREPARE_MULTIPASS_NEXT_PASS;
    case 3:
        // calculate all (link) dependencies for target
        prepare_pass3();
    RETURN_PREPARE_MULTIPASS_NEXT_PASS;
    case 4:
        // merge
        prepare_pass4();
    RETURN_PREPARE_MULTIPASS_NEXT_PASS;
    case 5:
        // source files
        prepare_pass5();
    RETURN_PREPARE_MULTIPASS_NEXT_PASS;
    case 6:
        // link libraries
        prepare_pass6();
    RETURN_PREPARE_MULTIPASS_NEXT_PASS;
    case 7:
        // linker 1
        prepare_pass7();
    RETURN_PREPARE_MULTIPASS_NEXT_PASS;
    case 8:
        // linker 2
        prepare_pass8();
    RETURN_PREPARE_MULTIPASS_NEXT_PASS;
    case 9:
        prepare_pass9();
        // TODO: create prepare endgames method that always will be the last one
        getGeneratedCommands(); // create g.commands
        call(CallbackType::EndPrepare);
    SW_RETURN_MULTIPASS_END(prepare_pass);
    }

    SW_RETURN_MULTIPASS_END(prepare_pass);
}

void NativeCompiledTarget::prepare_pass1()
{
    // make additional log level for this
    //LOG_TRACE(logger, "Preparing target: " + getPackage().getPath().toString());

    call(CallbackType::BeginPrepare);

    if (UseModules)
    {
        if (getCompilerType() != CompilerType::MSVC)
            throw SW_RUNTIME_ERROR("Currently modules are implemented for MSVC only");
        CPPVersion = CPPLanguageStandard::CPP2a;
    }

    if (ReproducibleBuild)
    {
        if (isClangFamily(getCompilerType()) || getCompilerType() == CompilerType::GNU)
        {
            // use pkg add timestamp later
            CompileOptions.push_back("-Wno-builtin-macro-redefined");
            // we set to empty string because
            // auto v = __DATE__; will cause an error in case -D__DATE__=
            add("__DATE__=\"\""_def);
            add("__TIME__=\"\""_def);
            add("__TIMESTAMP__=\"\""_def);
        }

        if (getCompilerType() == CompilerType::GNU)
        {
            CompileOptions.push_back("-ffile-prefix-map="
                + to_string(normalize_path(getContext().getLocalStorage().storage_dir))
                + "="
                // on windows we use the same path, but the root disk is also must be provided
                // not here, but in general
                // our windows paths are REALLY OK with starting slash '/' too
#define SW_DEFAULT_INSTALLATION_PATH "/sw/storage"
                // make a global definition and later a setting
                // this will be the default sw storage path on clean installations
                + SW_DEFAULT_INSTALLATION_PATH
            );

            // ld inserts timestamp by default for PE, we disable it
            if (getBuildSettings().TargetOS.is(OSType::Cygwin) || getBuildSettings().TargetOS.is(OSType::Mingw))
                LinkOptions.push_back("-Wl,--no-insert-timestamp");
        }

        // TODO: for *nix we probably must strip (debug) symbols also
    }

    findSources();

    if (!Publish)
        Publish = Scope == TargetScope::Build;

    if (NoUndefined)
    {
        if (getBuildSettings().TargetOS.is(OSType::Linux))
        {
            if (getCompilerType() == CompilerType::Clang || getCompilerType() == CompilerType::AppleClang)
                LinkOptions.push_back("--no-undefined");
            else // gcc and others
                LinkOptions.push_back("-Wl,--no-undefined");
        }
    }
    else
    {
        if (getBuildSettings().TargetOS.isApple())
        {
            LinkOptions.push_back("-undefined");
            LinkOptions.push_back("dynamic_lookup");
        }
    }

    if (!IsSwConfig)
    {
        // if we add include dir, we MUST create it
        // some programs consider an error if idir does not exist (cmake)

        // add pvt binary dir
        // do not check for existence, because generated files may go there
        // and we do not know about it right now
        IncludeDirectories.insert(BinaryPrivateDir);
        fs::create_directories(BinaryPrivateDir);

        // always add bdir to include dirs
        // do not check for existence, because generated files may go there
        // and we do not know about it right now
        Public.IncludeDirectories.insert(BinaryDir);
        fs::create_directories(BinaryDir);
    }

    if (!HeaderOnly || !*HeaderOnly)
        HeaderOnly = !hasSourceFiles();
    if (isHeaderOnly())
    {
        //SW_UNIMPLEMENTED;
        /*Linker.reset();
        Librarian.reset();
        SelectedTool = nullptr;*/
    }
    else
    {
        LinkLibrary l(getImportLibrary());
        l.whole_archive = WholeArchive;
        if (l.whole_archive)
        {
            if (getCompilerType() == CompilerType::MSVC || getCompilerType() == CompilerType::ClangCl)
                l.style = l.MSVC;
            // remove clang check? any apple platform?
            else if (getBuildSettings().TargetOS.isApple() &&
                (getCompilerType() == CompilerType::Clang || getCompilerType() == CompilerType::AppleClang)
                )
                l.style = l.AppleLD;
            else
                l.style = l.GNU;
        }
        if (isStaticLibrary())
            l.static_ = true;
        Interface += l;
    }

    if (UnityBuildBatchSize < 0)
        UnityBuildBatchSize = 0;

    if (PackageDefinitions)
        addPackageDefinitions(true);

    // mingw system link libraries must be specified without explicit .lib suffix
    if (getBuildSettings().TargetOS.Type == OSType::Mingw)
    {
        TargetOptionsGroup::iterate([this](auto &v, auto i)
        {
            for (auto &l : v.NativeLinkerOptions::System.LinkLibraries)
            {
                if (!l.l.parent_path().empty())
                    l.l = l.l.parent_path() / l.l.stem();
                else
                    l.l = l.l.stem();
            }
        });
    }

    // default export/import macros
    // public to make sure integrations also take these
    if (getBuildSettings().TargetOS.Type == OSType::Windows)
    {
        if (getCompilerType() == CompilerType::Clang)
        {
            // must be an attribute
            // must be     __attribute__ ((dllexport))
            // must not be __attribute__ ((visibility (\"default\"))) - in such case symbols are not exported
            Public.Definitions["SW_EXPORT"] = "__attribute__ ((dllexport))";
            Public.Definitions["SW_IMPORT"] = "__attribute__ ((dllimport))";
        }
        else
        {
            Public.Definitions["SW_EXPORT"] = "__declspec(dllexport)";
            Public.Definitions["SW_IMPORT"] = "__declspec(dllimport)";
        }
    }
    else if (0
        || getBuildSettings().TargetOS.Type == OSType::Cygwin
        || getBuildSettings().TargetOS.Type == OSType::Mingw
        )
    {
        Public.Definitions["SW_EXPORT"] = "__attribute__ ((dllexport))";
        Public.Definitions["SW_IMPORT"] = "__attribute__ ((dllimport))";
    }
    else
    {
        Public.Definitions["SW_EXPORT"] = "__attribute__ ((visibility (\"default\")))";
        Public.Definitions["SW_IMPORT"] = "__attribute__ ((visibility (\"default\")))";
    }

    // gather deps into one list of active deps

    // set our initial deps
    getActiveDependencies();
}

void NativeCompiledTarget::prepare_pass2()
{
    // resolve deps
    for (auto &d : getActiveDependencies())
    {
        auto t = getMainBuild().getTargets().find(d.dep->getPackage(), d.dep->settings);
        if (!t)
        {
            SW_UNIMPLEMENTED;
            //t = getContext().getPredefinedTargets().find(d.dep->getPackage(), d.dep->settings);
            //if (!t)
                //throw SW_RUNTIME_ERROR("No such target: " + d.dep->getPackage().toString());
        }
        d.dep->setTarget(*t);
    }

    // force cpp standard
    // some stdlibs require *minimal* cpp std to be set
    if (getSettings()["native"]["stdlib"]["cpp"].isValue() &&
        UnresolvedPackage(getSettings()["native"]["stdlib"]["cpp"].getValue()).getPath() == "com.Microsoft.VisualStudio.VC.libcpp")
    {
        for (auto &d : getActiveDependencies())
        {
            auto pkg = d.dep->getResolvedPackage();
            if (pkg.getPath() == "com.Microsoft.VisualStudio.VC.libcpp")
            {
                if (pkg.getVersion() > Version(19) && CPPVersion < CPPLanguageStandard::CPP14)
                    CPPVersion = CPPLanguageStandard::CPP14;
                break;
            }
        }
    }

    // propagate deps
    if (isStaticOrHeaderOnlyLibrary())
    {
        auto ad = getActiveDependencies();
        for (auto &d : ad)
        {
            Dependency d2(d.dep->getUnresolvedPackage());
            d2.settings = d.dep->getSettings();
            d2.setTarget(d.dep->getTarget());
            if (d.dep->IncludeDirectoriesOnly)
                continue;
            d2.LinkLibrariesOnly = true;
            auto d3 = std::make_shared<Dependency>(d2);
            Interface += d3;
            active_deps->push_back(createDependency(d3, InheritanceType::Interface, *this));
        }
    }
}

struct H
{
    size_t operator()(const DependencyPtr &p) const
    {
        return std::hash<PackageId>()(p->getTarget().getPackage());
    }
};

struct EQ
{
    size_t operator()(const DependencyPtr &p1, const DependencyPtr &p2) const
    {
        return &p1->getTarget() == &p2->getTarget();
    }
};

using PreparePass3Deps = DependencyPtr;
using PreparePass3DepsType = std::unordered_map<PreparePass3Deps, InheritanceType, H, EQ>;
using PreparePass3DepsOrderedType = std::vector<PreparePass3Deps>;

void NativeCompiledTarget::prepare_pass3()
{
    // calculate all (link) dependencies for target

    if (isHeaderOnly())
        return;

    prepare_pass3_1(); // normal deps
    prepare_pass3_2(); // idirs only deps
    prepare_pass3_3(); // llibs only deps
}

void NativeCompiledTarget::prepare_pass3_1()
{
    // process normal deps

    // we have ptrs, so do custom sorting
    PreparePass3DepsType deps(0, H{});
    PreparePass3DepsOrderedType deps_ordered;

    // set our initial deps
    // we have only active deps now
    for (auto &d : getActiveDependencies())
    {
        // skip both of idir only libs and llibs only
        if (d.dep->IncludeDirectoriesOnly)
            continue;
        if (d.dep->LinkLibrariesOnly)
            continue;
        auto copy = std::make_shared<Dependency>(*d.dep);
        auto [i, inserted] = deps.emplace(copy, d.inhtype);
        if (inserted)
            deps_ordered.push_back(copy);
        else
            i->second |= d.inhtype;
    }

    //
    while (1)
    {
        bool new_dependency = false;
        auto deps2 = deps;
        for (auto &[d, _] : deps2)
        {
            auto calc_deps = [this, &deps, &deps_ordered, &new_dependency](Dependency &d, Dependency &d2, InheritanceType Inheritance)
            {
                // nothing to do with private inheritance
                // before d2->getTarget()!
                if (Inheritance == InheritanceType::Private)
                    return;
                if (&d2.getTarget() == this)
                    return;
                // check same with d, not d2!
                if (Inheritance == InheritanceType::Protected && !hasSameProject(d.getTarget()))
                    return;

                auto copy = std::make_shared<Dependency>(d2);
                auto newinh = Inheritance == InheritanceType::Interface ? InheritanceType::Public : Inheritance;
                auto [i, inserted] = deps.emplace(copy, newinh);
                if (inserted)
                {
                    deps_ordered.push_back(copy);
                    // new dep is added
                    new_dependency = true;
                }
                else
                    i->second |= newinh;
            };

            if (auto t = d->getTarget().as<const NativeCompiledTarget *>())
            {
                // iterate over child deps
                for (auto &dep : t->getActiveDependencies())
                {
                    // skip both of idir only libs and llibs only
                    if (dep.dep->IncludeDirectoriesOnly)
                        continue;
                    if (dep.dep->LinkLibrariesOnly)
                        continue;
                    calc_deps(*d, *dep.dep, dep.inhtype);
                }
            }
            else if (auto t = d->getTarget().as<const PredefinedTarget *>())
            {
                auto &ts = t->getInterfaceSettings();

                for (auto &[k, v] : ts["properties"].getMap())
                {
                    auto inh = (InheritanceType)std::stoi(k);

                    for (auto &v1 : v["dependencies"].getArray())
                    {
                        for (auto &[package_id, settings] : v1.getMap())
                        {
                            // find our resolved dependency and run
                            bool found = false;
                            for (auto &d3 : t->getDependencies())
                            {
                                if (d3->getTarget().getPackage() == package_id && d3->getSettings() == settings.getMap())
                                {
                                    String err = getPackage().toString() + ": ";
                                    err += "dependency: " + t->getPackage().toString() + ": ";
                                    err += "dependency: " + d3->getUnresolvedPackage().toString();

                                    // construct
                                    Dependency d2(d3->getUnresolvedPackage());
                                    d2.settings = d3->getSettings();
                                    d2.setTarget(d3->getTarget());
                                    //d2.IncludeDirectoriesOnly = d3->getSettings()["include_directories_only"] == "true";
                                    d2.IncludeDirectoriesOnly = settings["include_directories_only"] == "true";
                                    //SW_ASSERT(d3->getSettings()["include_directories_only"] == settings["include_directories_only"], err);
                                    d2.LinkLibrariesOnly = settings["link_libraries_only"] == "true";
                                    //SW_ASSERT(d3->getSettings()["link_libraries_only"] == settings["link_libraries_only"], err);

                                    // skip both of idir only libs and llibs only
                                    if (d2.IncludeDirectoriesOnly || d2.LinkLibrariesOnly)
                                    {
                                        // do not process here
                                        found = true;
                                        break;
                                    }

                                    calc_deps(*d, d2, inh);
                                    found = true;
                                    break;
                                }
                            }
                            if (!found)
                                throw SW_RUNTIME_ERROR("Cannot find predefined target: " + package_id);
                        }
                    }
                }
            }
            else
                throw SW_RUNTIME_ERROR("missing target code");
        }

        if (!new_dependency)
        {
            for (auto &d : deps_ordered)
            {
                if (&d->getTarget() != this)
                    all_deps_normal.insert(deps.find(d)->first);
            }
            break;
        }
    }
}

void NativeCompiledTarget::prepare_pass3_2()
{
    // idirs only deps

    // we have ptrs, so do custom sorting
    PreparePass3DepsType deps(0, H{});
    PreparePass3DepsOrderedType deps_ordered;

    // set our initial deps
    // take active idir only deps
    for (auto &d : getActiveDependencies())
    {
        if (!d.dep->IncludeDirectoriesOnly)
            continue;
        if (d.dep->LinkLibrariesOnly)
            continue;
        auto copy = std::make_shared<Dependency>(*d.dep);
        auto [i, inserted] = deps.emplace(copy, d.inhtype);
        if (inserted)
            deps_ordered.push_back(copy);
        else
            i->second |= d.inhtype;
    }
    // and processed normal deps also
    for (auto &d : all_deps_normal)
    {
        auto copy = std::make_shared<Dependency>(*d);
        auto [i, inserted] = deps.emplace(copy, InheritanceType::Public); // use public inh
        if (inserted)
            deps_ordered.push_back(copy);
        else
            i->second |= InheritanceType::Public;
    }

    //
    while (1)
    {
        bool new_dependency = false;
        auto deps2 = deps;
        for (auto &[d, _] : deps2)
        {
            // accepts this driver's Dependency class
            auto calc_deps = [this, &deps, &deps_ordered, &new_dependency](Dependency &d, Dependency &d2, InheritanceType Inheritance)
            {
                // nothing to do with private inheritance
                // before d2->getTarget()!
                if (Inheritance == InheritanceType::Private)
                    return;
                if (&d2.getTarget() == this)
                    return;
                // check same with d, not d2!
                if (Inheritance == InheritanceType::Protected && !hasSameProject(d.getTarget()))
                    return;

                auto copy = std::make_shared<Dependency>(d2);
                auto newinh = Inheritance == InheritanceType::Interface ? InheritanceType::Public : Inheritance;
                auto [i, inserted] = deps.emplace(copy, newinh);

                // include directories only handling
                auto &di = *i->first;
                di.IncludeDirectoriesOnly = true;

                if (inserted)
                {
                    deps_ordered.push_back(copy);
                    new_dependency = true;
                }
                else
                    i->second |= newinh;
            };

            if (auto t = d->getTarget().as<const NativeCompiledTarget *>())
            {
                // iterate over child deps
                for (auto &dep : t->getActiveDependencies())
                {
                    if (!d->IncludeDirectoriesOnly && !dep.dep->IncludeDirectoriesOnly)
                        continue;
                    if (dep.dep->LinkLibrariesOnly)
                        continue;
                    calc_deps(*d, *dep.dep, dep.inhtype);
                }
            }
            else if (auto t = d->getTarget().as<const PredefinedTarget *>())
            {
                auto &ts = t->getInterfaceSettings();

                for (auto &[k, v] : ts["properties"].getMap())
                {
                    auto inh = (InheritanceType)std::stoi(k);

                    for (auto &v1 : v["dependencies"].getArray())
                    {
                        for (auto &[package_id, settings] : v1.getMap())
                        {
                            // find our resolved dependency and run
                            bool found = false;
                            for (auto &d3 : t->getDependencies())
                            {
                                if (d3->getTarget().getPackage() == package_id && d3->getSettings() == settings.getMap())
                                {
                                    // construct
                                    Dependency d2(d3->getUnresolvedPackage());
                                    d2.settings = d3->getSettings();
                                    d2.setTarget(d3->getTarget());
                                    d2.IncludeDirectoriesOnly = settings["include_directories_only"] == "true";
                                    d2.LinkLibrariesOnly = settings["link_libraries_only"] == "true";

                                    //// exit early before llibs only
                                    if (!d->IncludeDirectoriesOnly && !d2.IncludeDirectoriesOnly)
                                    {
                                        // do not process here
                                        found = true;
                                        continue;
                                    }
                                    if (d2.LinkLibrariesOnly)
                                    {
                                        // do not process here
                                        found = true;
                                        continue;
                                    }

                                    calc_deps(*d, d2, inh);
                                    found = true;
                                    break;
                                }
                            }
                            if (!found)
                                throw SW_RUNTIME_ERROR("Cannot find predefined target: " + package_id);
                        }
                    }
                }
            }
            else
                throw SW_RUNTIME_ERROR("missing target code");
        }

        if (!new_dependency)
        {
            for (auto &d : deps_ordered)
            {
                if (&d->getTarget() != this && d->IncludeDirectoriesOnly)
                    all_deps_idir_only.insert(deps.find(d)->first);
            }
            break;
        }
    }
}

void NativeCompiledTarget::prepare_pass3_3()
{
    // llibs only

    if (isStaticLibrary())
        return;

    // we have ptrs, so do custom sorting
    PreparePass3DepsType deps(0, H{});
    PreparePass3DepsOrderedType deps_ordered;

    // set our initial deps
    for (auto &d : getActiveDependencies())
    {
        if (!d.dep->LinkLibrariesOnly)
            continue;
        auto copy = std::make_shared<Dependency>(*d.dep);
        auto [i, inserted] = deps.emplace(copy, InheritanceType::Public); // use public inh
        if (inserted)
            deps_ordered.push_back(copy);
        else
            i->second |= InheritanceType::Public;
    }
    for (auto &d : all_deps_normal)
    {
        if (auto t = d->getTarget().as<const NativeCompiledTarget *>())
        {
            if (!t->isStaticOrHeaderOnlyLibrary())
                continue;
        }
        else if (auto t = d->getTarget().as<const PredefinedTarget *>())
        {
            auto &ts = t->getInterfaceSettings();
            if (!::sw::isStaticOrHeaderOnlyLibrary(ts))
                continue;
        }
        else
            throw SW_RUNTIME_ERROR("missing target code");
        auto copy = std::make_shared<Dependency>(*d);
        copy->LinkLibrariesOnly = true; // force
        auto [i, inserted] = deps.emplace(copy, InheritanceType::Public); // use public inh
        if (inserted)
            deps_ordered.push_back(copy);
        else
            i->second |= InheritanceType::Public;
    }

    while (1)
    {
        bool new_dependency = false;
        auto deps2 = deps;
        for (auto &[d, _] : deps2)
        {
            // accepts this driver's Dependency class
            auto calc_deps = [this, &deps, &deps_ordered, &new_dependency](Dependency &d, Dependency &d2, InheritanceType Inheritance)
            {
                if (&d2.getTarget() == this)
                    return;

                auto copy = std::make_shared<Dependency>(d2);
                auto newinh = Inheritance == InheritanceType::Interface ? InheritanceType::Public : Inheritance;
                auto [i, inserted] = deps.emplace(copy, newinh);

                // include directories only handling
                auto &di = *i->first;
                di.LinkLibrariesOnly = true;

                if (inserted)
                {
                    deps_ordered.push_back(copy);
                    new_dependency = true;
                }
                else
                    i->second |= newinh;
            };

            if (auto t = d->getTarget().as<const NativeCompiledTarget *>())
            {
                if (!t->isStaticOrHeaderOnlyLibrary())
                    continue;
                // iterate over child deps
                for (auto &dep : t->getActiveDependencies())
                {
                    if (!dep.dep->LinkLibrariesOnly)
                        continue;
                    calc_deps(*d, *dep.dep, dep.inhtype);
                }
            }
            else if (auto t = d->getTarget().as<const PredefinedTarget *>())
            {
                auto &ts = t->getInterfaceSettings();

                if (!::sw::isStaticOrHeaderOnlyLibrary(ts))
                    continue;

                for (auto &[k, v] : ts["properties"].getMap())
                {
                    auto inh = (InheritanceType)std::stoi(k);

                    for (auto &v1 : v["dependencies"].getArray())
                    {
                        for (auto &[package_id, settings] : v1.getMap())
                        {
                            // find our resolved dependency and run
                            bool found = false;
                            for (auto &d3 : t->getDependencies())
                            {
                                if (d3->getTarget().getPackage() == package_id && d3->getSettings() == settings.getMap())
                                {
                                    // construct
                                    Dependency d2(d3->getUnresolvedPackage());
                                    d2.settings = d3->getSettings();
                                    d2.setTarget(d3->getTarget());
                                    d2.IncludeDirectoriesOnly = settings["include_directories_only"] == "true";
                                    d2.LinkLibrariesOnly = settings["link_libraries_only"] == "true";

                                    if (!d2.LinkLibrariesOnly)
                                    {
                                        // do not process here
                                        found = true;
                                        break;
                                    }

                                    calc_deps(*d, d2, inh);
                                    found = true;
                                    break;
                                }
                            }
                            if (!found)
                                throw SW_RUNTIME_ERROR("Cannot find predefined target: " + package_id);
                        }
                    }
                }
            }
            else
                throw SW_RUNTIME_ERROR("missing target code");
        }

        if (!new_dependency)
        {
            for (auto &d : deps_ordered)
            {
                if (&d->getTarget() != this && d->LinkLibrariesOnly)
                    all_deps_llibs_only.insert(deps.find(d)->first);
            }
            break;
        }
    }
}

void NativeCompiledTarget::prepare_pass4()
{
    // merge

    // merge files early
    for (auto &d : all_deps_normal)
    {
        if (auto t = d->getTarget().as<const NativeCompiledTarget *>())
        {
            GroupSettings s;
            s.has_same_parent = hasSameProject(*t);
            auto &g = *t;
            // merge from other group
            s.merge_to_self = false;
            if (s.has_same_parent)
                mergeFiles(g.Protected, s);
            mergeFiles(g.Public, s);
            // always with interface
            mergeFiles(g.Interface, s);
        }
        else if (auto t = d->getTarget().as<const PredefinedTarget *>())
        {
            const auto &is = d->getTarget().getInterfaceSettings();

            for (auto &[k,v] : is["properties"].getMap())
            {
                auto inh = (InheritanceType)std::stoi(k);
                if (inh == InheritanceType::Private)
                    continue;
                if (inh == InheritanceType::Protected && !hasSameProject(d->getTarget()))
                    continue;

                for (auto &v2 : v["source_files"].getArray())
                    *this += v2.getPathValue(getContext().getLocalStorage());
            }
        }
        else
            throw SW_RUNTIME_ERROR("missing target code");
    }

    // merge self
    TargetOptionsGroup::iterate_this([this](auto &v, auto i)
    {
        getMergeObject().merge(v);
    });

    // merge deps' stuff

    // normal deps
    // take everything
    for (auto &d : all_deps_normal)
    {
        if (auto t = d->getTarget().as<const NativeCompiledTarget *>())
        {
            GroupSettings s;
            s.has_same_parent = hasSameProject(*t);
            auto &g = *t;
            // merge from other group
            s.merge_to_self = false;
            if (s.has_same_parent)
                getMergeObject().merge(g.Protected, s);
            getMergeObject().merge(g.Public, s);
            // always with interface
            getMergeObject().merge(g.Interface, s);
        }
        else if (auto t = d->getTarget().as<const PredefinedTarget *>())
        {
            const auto &is = d->getTarget().getInterfaceSettings();

            for (auto &[k,v] : is["properties"].getMap())
            {
                auto inh = (InheritanceType)std::stoi(k);
                if (inh == InheritanceType::Private)
                    continue;
                if (inh == InheritanceType::Protected && !hasSameProject(d->getTarget()))
                    continue;

                for (auto &v2 : v["source_files"].getArray())
                    getMergeObject() += v2.getPathValue(getContext().getLocalStorage());

                for (auto &[k, v2] : v["definitions"].getMap())
                {
                    if (v2.getValue().empty())
                        getMergeObject().Definitions[k];
                    else
                        getMergeObject().Definitions[k] = v2.getValue();
                }

                for (auto &v2 : v["compile_options"].getArray())
                    getMergeObject().CompileOptions.insert(v2.getValue());

                // TODO: add custom options

                for (auto &v2 : v["include_directories"].getArray())
                    getMergeObject().IncludeDirectories.insert(v2.getPathValue(getContext().getLocalStorage()));

                for (auto &v2 : v["link_libraries"].getArray())
                    getMergeObject().LinkLibraries.insert(LinkLibrary{ v2.getPathValue(getContext().getLocalStorage()) });

                for (auto &v2 : v["system_include_directories"].getArray())
                    getMergeObject().NativeCompilerOptions::System.IncludeDirectories.push_back(v2.getAbsolutePathValue());
                for (auto &v2 : v["system_link_directories"].getArray())
                    getMergeObject().NativeLinkerOptions::System.LinkDirectories.push_back(v2.getAbsolutePathValue());
                for (auto &v2 : v["system_link_libraries"].getArray())
                    getMergeObject().NativeLinkerOptions::System.LinkLibraries.insert(LinkLibrary{ v2.getAbsolutePathValue() });

                for (auto &v2 : v["frameworks"].getArray())
                    getMergeObject().Frameworks.insert(v2.getValue());
            }
        }
        else
            throw SW_RUNTIME_ERROR("missing target code");
    }

    // idirs
    // take defs and idirs
    for (auto &d : all_deps_idir_only)
    {
        if (auto t = d->getTarget().as<const NativeCompiledTarget *>())
        {
            GroupSettings s;
            s.include_directories_only = true;
            s.has_same_parent = hasSameProject(*t);
            auto &g = *t;
            // merge from other group, always w/ interface
            s.merge_to_self = false;

            if (s.has_same_parent)
                getMergeObject().SourceFileStorage::merge(g.Protected, s);
            getMergeObject().SourceFileStorage::merge(g.Public, s);
            getMergeObject().SourceFileStorage::merge(g.Interface, s);

            if (s.has_same_parent)
                getMergeObject().NativeCompilerOptions::merge(g.Protected, s);
            getMergeObject().NativeCompilerOptions::merge(g.Public, s);
            getMergeObject().NativeCompilerOptions::merge(g.Interface, s);
        }
        else if (auto t = d->getTarget().as<const PredefinedTarget *>())
        {
            const auto &is = d->getTarget().getInterfaceSettings();

            for (auto &[k,v] : is["properties"].getMap())
            {
                auto inh = (InheritanceType)std::stoi(k);
                if (inh == InheritanceType::Private)
                    continue;
                if (inh == InheritanceType::Protected && !hasSameProject(d->getTarget()))
                    continue;

                // allow only header only files?
                for (auto &v2 : v["source_files"].getArray())
                    getMergeObject() += v2.getPathValue(getContext().getLocalStorage());

                for (auto &[k, v2] : v["definitions"].getMap())
                {
                    if (v2.getValue().empty())
                        getMergeObject().Definitions[k];
                    else
                        getMergeObject().Definitions[k] = v2.getValue();
                }

                for (auto &v2 : v["include_directories"].getArray())
                    getMergeObject().IncludeDirectories.insert(v2.getPathValue(getContext().getLocalStorage()));
                for (auto &v2 : v["system_include_directories"].getArray())
                    getMergeObject().NativeCompilerOptions::System.IncludeDirectories.push_back(v2.getAbsolutePathValue());
            }
        }
        else
            throw SW_RUNTIME_ERROR("missing target code");
    }

    if (isStaticOrHeaderOnlyLibrary())
        return;

    // llibs
    // merge from everything (every visibility class)
    for (auto &d : all_deps_llibs_only)
    {
        if (auto t = d->getTarget().as<const NativeCompiledTarget *>())
        {
            SwapAndRestore sr(getMergeObject().LinkOptions); // keep unchanged
            t->TargetOptionsGroup::iterate([this](auto &v, auto i)
            {
                getMergeObject().NativeLinkerOptions::merge(v);
            });
        }
        else if (auto t = d->getTarget().as<const PredefinedTarget *>())
        {
            const auto &is = d->getTarget().getInterfaceSettings();

            for (auto &[k,v] : is["properties"].getMap())
            {
                for (auto &v2 : v["link_libraries"].getArray())
                    getMergeObject().LinkLibraries.insert(LinkLibrary{ v2.getPathValue(getContext().getLocalStorage()) });

                for (auto &v2 : v["system_link_directories"].getArray())
                    getMergeObject().NativeLinkerOptions::System.LinkDirectories.push_back(v2.getAbsolutePathValue());
                for (auto &v2 : v["system_link_libraries"].getArray())
                    getMergeObject().NativeLinkerOptions::System.LinkLibraries.insert(LinkLibrary{ v2.getAbsolutePathValue() });

                for (auto &v2 : v["frameworks"].getArray())
                    getMergeObject().Frameworks.insert(v2.getValue());
            }
        }
        else
            throw SW_RUNTIME_ERROR("missing target code");
    }
}

void NativeCompiledTarget::prepare_pass5()
{
    // source files

    // set build as property
    /*for (auto &[p, f] : getMergeObject())
    {
        if (f->isActive() && !f->postponed)
        {
            auto f2 = f->as<NativeSourceFile*>();
            if (!f2)
                continue;
            auto ba = f2->BuildAs;
            switch (ba)
            {
            case NativeSourceFile::BasedOnExtension:
                break;
            case NativeSourceFile::C:
                if (auto p = findProgramByExtension(".c"))
                {
                    if (auto c = f2->compiler->as<VisualStudioCompiler*>())
                        c->CompileAsC = true;
                }
                else
                    throw std::logic_error("no C language found");
                break;
            case NativeSourceFile::CPP:
                if (auto p = findProgramByExtension(".cpp"))
                {
                    if (auto c = f2->compiler->as<VisualStudioCompiler*>())
                        c->CompileAsCPP = true;
                }
                else
                    throw std::logic_error("no CPP language found");
                break;
            case NativeSourceFile::ASM:
                SW_UNIMPLEMENTED; // actually remove this to make noop?
                                  //if (auto L = SourceFileStorage::findLanguageByExtension(".asm"); L)
                                  //L->clone()->createSourceFile(f.first, this);
                                  //else
                                  //throw std::logic_error("no ASM language found");
                break;
            default:
                SW_UNREACHABLE;
            }
        }
    }

    auto files = gatherSourceFiles();
    */

    // also fix rpath libname here
    /*if (getSelectedTool() && createWindowsRpath())
    {
        getSelectedTool()->setImportLibrary(getOutputFileName2("lib") += ".rp");
    }*/
}

void NativeCompiledTarget::prepare_pass6()
{
    // link libraries
    if (isStaticOrHeaderOnlyLibrary())
        return;

    // circular deps detection
    //SW_UNIMPLEMENTED;
    /*if (auto L = getLinker().as<VisualStudioLinker *>())
    {
        for (auto &d : all_deps_normal)
        {
            if (&d->getTarget() == this)
                continue;

            auto nt = d->getTarget().template as<NativeCompiledTarget *>();
            if (!nt)
                continue;

            for (auto &d2 : nt->all_deps_normal)
            {
                if (&d2->getTarget() != this)
                    continue;

                circular_dependency = true;
                break;
            }
        }
    }*/
}

void NativeCompiledTarget::prepare_pass7()
{
    // linker 1

    // gatherStaticLinkLibraries
    if (!isStaticOrHeaderOnlyLibrary())
    {
        bool setup_rpath =
            1
            && !getBuildSettings().TargetOS.is(OSType::Windows)
            && !getBuildSettings().TargetOS.is(OSType::Cygwin)
            && !getBuildSettings().TargetOS.is(OSType::Mingw)
            ;

        if (setup_rpath)
        {
            auto dirs = gatherRpathLinkDirectories();

            //
            // linux:
            //
            // -rpath-link
            //
            // When linking libA.so to libB.so and then libB.so to exeC,
            // ld requires to provide -rpath or -rpath-link to libA.so.
            //
            // Currently we do not set rpath, so ld cannot read automatically from libB.so
            // where libA.so is located.
            //
            // Hence, we must provide such paths ourselves.
            //
            if (getBuildSettings().TargetOS.is(OSType::Linux) && getType() == TargetType::NativeExecutable)
            {
                //for (auto &d : dirs)
                    //getMergeObject().LinkOptions.push_back("-Wl,-rpath-link," + normalize_path(d));
            }

            String rpath_var = "-Wl,";
            if (getBuildSettings().TargetOS.is(OSType::Linux))
                rpath_var += "--enable-new-dtags,";
            rpath_var += "-rpath,";

            for (auto &d : dirs)
                getMergeObject().LinkOptions.push_back(rpath_var + to_string(normalize_path(d)));

            // rpaths
            if (getType() == TargetType::NativeExecutable)
            {
                // rpath: currently we set non macos runpath to $ORIGIN
                String exe_path = "$ORIGIN";
                // rpath: currently we set macos rpath to @executable_path
                if (getBuildSettings().TargetOS.is(OSType::Macos))
                    exe_path = "@executable_path";
                getMergeObject().LinkOptions.push_back(rpath_var + exe_path);
            }
        }
    }
}

void NativeCompiledTarget::prepare_pass8()
{
    // linker 2

    // linker setup
    // circular and windows rpath processing
    //processCircular(obj);

    // setup target

    // now create pch
    createPrecompiledHeader();

    // before merge
    if (getBuildSettings().Native.ConfigurationType != ConfigurationType::Debug)
        getMergeObject() += Definition("NDEBUG");

    // emulate msvc defs for clang
    // https://docs.microsoft.com/en-us/cpp/build/reference/md-mt-ld-use-run-time-library?view=vs-2019
    if (getBuildSettings().TargetOS.is(OSType::Windows) && getCompilerType() == CompilerType::Clang)
    {
        // always (except /LD but we do not support it yet)
        getMergeObject() += Definition("_MT");
        if (!getBuildSettings().Native.MT)
            getMergeObject() += Definition("_DLL");
        if (getBuildSettings().Native.ConfigurationType == ConfigurationType::Debug)
            getMergeObject() += Definition("_DEBUG");
    }

    if (!isExecutable())
    {
        if (getBuildSettings().TargetOS.Type == OSType::Windows)
            getMergeObject() += "_WINDLL"_def;
    }

    // add rules
    auto add_rule2 = [this](const auto &n)
    {
        addRule(n, getRuleFromDependency(n));
        getRuleDependencies().erase(n);
    };
    if (!(ct == CompilerType::MSVC &&
        getBuildSettings().TargetOS.Arch != ArchType::x86 &&
        getBuildSettings().TargetOS.Arch != ArchType::x86_64
        ))
        add_rule2("asm");
    add_rule2("c");
    add_rule2("cpp");

    if (isHeaderOnly())
        ;
    else if (isStaticLibrary())
        add_rule2("lib");
    else
    {
        // generate rc
        if (GenerateWindowsResource
            && !isHeaderOnly()
            && ::sw::gatherSourceFiles<SourceFile>(*this, { ".rc" }).empty()
            && getBuildSettings().TargetOS.is(OSType::Windows)
            && Scope == TargetScope::Build
            )
        {
            auto p = generate_rc();
            // more info for generators
            // but we already wrote this file, no need to mark as generated
            //File(p, getFs()).setGenerated(true);
            getMergeObject() += p;
        }
        // add rc rule
        if (getBuildSettings().TargetOS.is(OSType::Windows) && getSettings()["rule"]["rc"])
            add_rule2("rc");

        if (isExecutable() || !isHeaderOnly())
            add_rule2("link");
    }
    //if (circular_dependency)
        //add_rule("link_circular", std::make_unique<NativeLinkerRule>(*prog_lib));

    // rules!
    RuleFiles rfs;
    for (auto &[p, f] : getMergeObject())
    {
        if (!f->isActive())
            continue;
        RuleFile rf(p);
        rf.getAdditionalArguments() = f->args;
        rfs.insert(rf);
    }
    DEBUG_BREAK_IF(getPackage().toString() == "qtproject.qt.base.core-5.15.0.1");
    runRules(rfs, *this);
}

void NativeCompiledTarget::prepare_pass9()
{
    clearGlobCache();
}

path NativeCompiledTarget::generate_rc()
{
    struct RcEmitter : primitives::Emitter
    {
        using Base = primitives::Emitter;

        RcEmitter(Version file_ver, Version product_ver)
        {
            if (file_ver.isBranch())
                file_ver = Version();
            if (product_ver.isBranch())
                product_ver = Version();

            file_ver = Version(file_ver.getMajor(), file_ver.getMinor(), file_ver.getPatch(), file_ver.getTweak());
            product_ver = Version(product_ver.getMajor(), product_ver.getMinor(), product_ver.getPatch(), product_ver.getTweak());

            addLine("1 VERSIONINFO");
            addLine("  FILEVERSION " + file_ver.toString(","s));
            addLine("  PRODUCTVERSION " + product_ver.toString(","s));
        }

        void beginBlock(const String &name)
        {
            addLine("BLOCK \"" + name + "\"");
            begin();
        }

        void endBlock()
        {
            end();
        }

        void addValue(const String &name, const Strings &vals)
        {
            addLine("VALUE \"" + name + "\", ");
            for (auto &v : vals)
                addText(v + ", ");
            trimEnd(2);
        }

        void addValueQuoted(const String &name, const Strings &vals)
        {
            Strings vals2;
            for (auto &v : vals)
                vals2.push_back("\"" + v + "\"");
            addValue(name, vals2);
        }

        void begin()
        {
            increaseIndent("BEGIN");
        }

        void end()
        {
            decreaseIndent("END");
        }
    };

    RcEmitter ctx(getPackage().getVersion(), getPackage().getVersion());
    ctx.begin();

    ctx.beginBlock("StringFileInfo");
    ctx.beginBlock("040904b0");
    //VALUE "CompanyName", "TODO: <Company name>"
    ctx.addValueQuoted("FileDescription", { getPackage().getPath().back()
        // + " - " + getConfig()
        }); // remove config for now
    ctx.addValueQuoted("FileVersion", { getPackage().getVersion().toString() });
    //VALUE "InternalName", "@PACKAGE@"
    ctx.addValueQuoted("LegalCopyright", { "Powered by Software Network" });
    ctx.addValueQuoted("OriginalFilename", { getPackage().toString() });
    ctx.addValueQuoted("ProductName", { getPackage().getPath().toString() });
    ctx.addValueQuoted("ProductVersion", { getPackage().getVersion().toString() });
    ctx.endBlock();
    ctx.endBlock();

    ctx.beginBlock("VarFileInfo");
    ctx.addValue("Translation", { "0x409","1200" });
    ctx.endBlock();

    ctx.end();

    path p = BinaryPrivateDir / "sw.rc";
    write_file_if_different(p, ctx.getText());

    return p;
}

void NativeCompiledTarget::processCircular(Files &obj)
{
    if (!hasCircularDependency() && !createWindowsRpath())
        return;
    if (isStaticOrHeaderOnlyLibrary())
        return;

    SW_UNIMPLEMENTED;
    /*auto lib_exe = Librarian->as<VisualStudioLibrarian*>();
    if (!lib_exe)
        throw SW_RUNTIME_ERROR("Unsupported librarian");

    auto link_exe = Linker->as<VisualStudioLinker*>();
    if (!link_exe)
        throw SW_RUNTIME_ERROR("Unsupported linker");

    // protect output file renaming
    static std::mutex m;

    auto name = to_string(Linker->getOutputFile().filename().u8string());
    if (createWindowsRpath())
    {
        Files dlls;
        SW_UNIMPLEMENTED;
        //for (auto &d : Dependencies)
        //{
        //    if (d->target == this)
        //        continue;
        //    if (d->isDisabledOrDummy())
        //        continue;
        //    if (d->IncludeDirectoriesOnly// || d->LinkLibrariesOnly
        //    )
        //        continue;

        //    auto nt = d->target->as<NativeCompiledTarget*>();

        //    if (!*nt->HeaderOnly)
        //    {
        //        if (nt->getSelectedTool() == nt->Linker.get())
        //        {
        //            dlls.push_back(nt->getPackage().toString() + ".dll"); // in

        //            // don't replace local targets' deps
        //            if (d->target->isLocal())
        //            {
        //                // same as in
        //                dlls.push_back(nt->getPackage().toString() + ".dll"); // out
        //                continue;
        //            }

        //            path out;
        //            String ext;
        //            {
        //                std::lock_guard lk(m);
        //                ext = nt->getOutputFile().extension().u8string();
        //                out = nt->getOutputFile().parent_path();
        //            }
        //            out = out.lexically_relative(getSolution().getContext().getLocalStorage().storage_dir);
        //            out /= nt->getPackage().toString() + ext + ".rp" + ext;
        //            dlls.push_back(out.u8string()); // out
        //        }
        //    }
        //}

        // even if dlls are empty we still need to do this!

        auto sz = name.size();
        if (sz > symbol_len_max)
        {
            throw SW_RUNTIME_ERROR("name size (" + std::to_string(sz) +
                ") is greater than max (" + std::to_string(symbol_len_max) + ")");
        }
        std::stringstream stream;
        stream << std::setfill('0') << std::setw(symbol_len_len) << std::hex << sz;
        name = stream.str() + name;
        name.resize(symbol_len_max, 's');

        path out;
        {
            std::lock_guard lk(m);
            out = Linker->getOutputFile();
            Linker->setOutputFile(path(out) += ".1");
        }
        out += ".rp" + to_string(out.extension().u8string());

        auto c = addCommand(SW_VISIBLE_BUILTIN_FUNCTION(replace_dll_import));
        c << cmd::in(Linker->getOutputFile());
        c << cmd::out(out);

        auto cmd = Linker->createCommand(getMainBuild());
        cmd->dependent_commands.insert(c.getCommand());
        std::dynamic_pointer_cast<builder::BuiltinCommand>(c.getCommand())->push_back(dlls);
        c->addInput(dlls);
        cmds.insert(c.getCommand());
        outputfile = out;
    }

    lib_exe->CreateImportLibrary = true; // set def option = create .exp(ort) file
    lib_exe->DllName = name;
    link_exe->ImportLibrary.clear(); // clear implib

    if (!link_exe->ModuleDefinitionFile)
    {
        FilesOrdered files(obj.begin(), obj.end());
        std::sort(files.begin(), files.end());
        Librarian->setObjectFiles(files);
    }
    else
    {
        lib_exe->ModuleDefinitionFile = link_exe->ModuleDefinitionFile;
        link_exe->ModuleDefinitionFile.clear(); // it will use .exp
    }
    // add rp only for winrpaths
    if (createWindowsRpath())
        Librarian->setOutputFile(getOutputFileName2("lib") += ".rp");
    else
        Librarian->setOutputFile(getOutputFileName2("lib"));

    //
    auto exp = Librarian->getImportLibrary();
    exp = exp.parent_path() / (exp.stem() += ".exp");
    Librarian->merge(getMergeObject());
    Librarian->prepareCommand(*this)->addOutput(exp);
    obj.insert(exp);*/
}

FilesOrdered NativeCompiledTarget::gatherRpathLinkDirectories() const
{
    FilesOrdered rpath;
    for (auto &d : all_deps_normal)
    {
        if (auto dt = d->getTarget().template as<const NativeCompiledTarget *>())
        {
            if (!dt->isStaticOrHeaderOnlyLibrary())
                rpath.push_back(dt->getOutputFile().parent_path());
        }
        else if (auto t = d->getTarget().as<const PredefinedTarget *>())
        {
            auto &ts = t->getInterfaceSettings();
            if (!::sw::isStaticOrHeaderOnlyLibrary(ts))
                rpath.push_back(ts["output_file"].getPathValue(getContext().getLocalStorage()).parent_path());
        }
        else
            throw SW_RUNTIME_ERROR("missing target code");
    }
    return rpath;
}

bool NativeCompiledTarget::prepareLibrary(LibraryType Type)
{
    switch (prepare_pass)
    {
    case 1:
    {
        auto set_api = [this, &Type](const String &api)
        {
            if (api.empty())
                return;

            if (0
                || getBuildSettings().TargetOS.Type == OSType::Windows
                || getBuildSettings().TargetOS.Type == OSType::Cygwin
                || getBuildSettings().TargetOS.Type == OSType::Mingw
                )
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
                Public.Definitions[api] = "SW_EXPORT";
            }

            // old
            //Definitions[api + "_EXTERN="];
            //Interface.Definitions[api + "_EXTERN"] = "extern";
        };

        if (SwDefinitions)
        {
            if (Type == LibraryType::Shared)
            {
                Definitions["SW_SHARED_BUILD"];
            }
            else if (Type == LibraryType::Static)
            {
                Definitions["SW_STATIC_BUILD"];
            }
        }

        set_api(ApiName);
        for (auto &a : ApiNames)
            set_api(a);
    }
    break;
    }

    return NativeCompiledTarget::prepare();
}

void NativeCompiledTarget::removeFile(const path &fn, bool binary_dir)
{
    remove_full(fn);
    Target::removeFile(fn, binary_dir);
}

void NativeCompiledTarget::addFileSilently(const path &from)
{
    // add to target if not already added
    if (DryRun)
        operator-=(from);
    else
    {
        auto fr = from;
        check_absolute(fr);
        if (!hasFile(fr))
            operator-=(from);
    }
}

void NativeCompiledTarget::configureFile(path from, path to, ConfigureFlags flags)
{
    addFileSilently(from);

    // before resolving
    if (!to.is_absolute())
        to = BinaryDir / to;
    File(to, getFs()).setGenerated();

    if (DryRun)
        return;

    if (!from.is_absolute())
    {
        if (fs::exists(SourceDir / from))
            from = SourceDir / from;
        else if (fs::exists(BinaryDir / from))
            from = BinaryDir / from;
        else
            throw SW_RUNTIME_ERROR("Package: " + getPackage().toString() + ", file not found: " + from.string());
    }

    // we really need ExecuteCommand here!!! or not?
    //auto c = std::make_shared<DummyCommand>();// ([this, from, to, flags]()
    {
        configureFile1(from, to, flags);
    }//);
    //c->addInput(from);
    //c->addOutput(to);

    if ((int)flags & (int)ConfigureFlags::AddToBuild)
        operator+=(to);
}

void NativeCompiledTarget::configureFile1(const path &from, const path &to, ConfigureFlags flags)
{
    static const std::regex cmDefineRegex(R"xxx(#\s*cmakedefine[ \t]+([A-Za-z_0-9]*)([^\r\n]*?)[\r\n])xxx");
    static const std::regex cmDefine01Regex(R"xxx(#\s*cmakedefine01[ \t]+([A-Za-z_0-9]*)[^\r\n]*?[\r\n])xxx");
    static const std::regex mesonDefine(R"xxx(#\s*mesondefine[ \t]+([A-Za-z_0-9]*)[^\r\n]*?[\r\n])xxx");
    static const std::regex undefDefine(R"xxx(#undef[ \t]+([A-Za-z_0-9]*)[^\r\n]*?[\r\n])xxx");
    static const std::regex cmAtVarRegex("@([A-Za-z_0-9/.+-]+)@");
    static const std::regex cmNamedCurly("\\$\\{([A-Za-z0-9/_.+-]+)\\}");

    static const StringSet offValues{
        "", "0", //"OFF", "NO", "FALSE", "N", "IGNORE",
    };

    configure_files.insert(from);

    auto s = read_file(from);

    if ((int)flags & (int)ConfigureFlags::CopyOnly)
    {
        writeFileOnce(to, s);
        return;
    }

    auto find_repl = [this, &from, flags](const auto &key) -> std::optional<std::string>
    {
        auto v = Variables.find(key);
        if (v != Variables.end())
            return v->second.toString();

        // dangerous! should we really check defs?
        /*auto d = Definitions.find(key);
        if (d != Definitions.end())
            return d->second.toString();
        */

        //if (isLocal()) // put under cl cond
            //LOG_WARN(logger, "Unset variable '" + key + "' in file: " + normalize_path(from));

        if ((int)flags & (int)ConfigureFlags::ReplaceUndefinedVariablesWithZeros)
            return "0";

        return {};
    };

    std::smatch m;

    // @vars@
    while (std::regex_search(s, m, cmAtVarRegex) ||
        std::regex_search(s, m, cmNamedCurly))
    {
        auto repl = find_repl(m[1].str());
        if (!repl)
        {
            s = m.prefix().str() + m.suffix().str();
            // make additional log level for this
            //LOG_TRACE(logger, "configure @@ or ${} " << m[1].str() << ": replacement not found");
            continue;
        }
        s = m.prefix().str() + *repl + m.suffix().str();
    }

    // #mesondefine
    while (std::regex_search(s, m, mesonDefine))
    {
        auto repl = find_repl(m[1].str());
        if (!repl)
        {
            s = m.prefix().str() + "/* #undef " + m[1].str() + " */\n" + m.suffix().str();
            // make additional log level for this
            //LOG_TRACE(logger, "configure #mesondefine " << m[1].str() << ": replacement not found");
            continue;
        }
        s = m.prefix().str() + "#define " + m[1].str() + " " + *repl + "\n" + m.suffix().str();
    }

    // #undef
    if ((int)flags & (int)ConfigureFlags::EnableUndefReplacements)
    {
        while (std::regex_search(s, m, undefDefine))
        {
            auto repl = find_repl(m[1].str());
            if (!repl)
            {
                // space to prevent loops
                s = m.prefix().str() + "/* # undef " + m[1].str() + " */\n" + m.suffix().str();
                // make additional log level for this
                //LOG_TRACE(logger, "configure #undef " << m[1].str() << ": replacement not found");
                continue;
            }
            if (offValues.find(boost::to_upper_copy(*repl)) != offValues.end())
                // space to prevent loops
                s = m.prefix().str() + "/* # undef " + m[1].str() + " */\n" + m.suffix().str();
            else
                s = m.prefix().str() + "#define " + m[1].str() + " " + *repl + "\n" + m.suffix().str();
        }
    }

    // #cmakedefine
    while (std::regex_search(s, m, cmDefineRegex))
    {
        auto repl = find_repl(m[1].str());
        if (!repl)
        {
            // make additional log level for this
            //LOG_TRACE(logger, "configure #cmakedefine " << m[1].str() << ": replacement not found");
            repl = {};
        }
        if (offValues.find(boost::to_upper_copy(*repl)) != offValues.end())
            s = m.prefix().str() + "/* #undef " + m[1].str() + m[2].str() + " */\n" + m.suffix().str();
        else
            s = m.prefix().str() + "#define " + m[1].str() + m[2].str() + "\n" + m.suffix().str();
    }

    // #cmakedefine01
    while (std::regex_search(s, m, cmDefine01Regex))
    {
        auto repl = find_repl(m[1].str());
        if (!repl)
        {
            // make additional log level for this
            //LOG_TRACE(logger, "configure #cmakedefine01 " << m[1].str() << ": replacement not found");
            repl = {};
        }
        if (offValues.find(boost::to_upper_copy(*repl)) != offValues.end())
            s = m.prefix().str() + "#define " + m[1].str() + " 0" + "\n" + m.suffix().str();
        else
            s = m.prefix().str() + "#define " + m[1].str() + " 1" + "\n" + m.suffix().str();
    }

    writeFileOnce(to, s);
}

CheckSet &NativeCompiledTarget::getChecks(const String &name)
{
    auto i = getSolution().checker->sets.find(name);
    if (i == getSolution().checker->sets.end())
        throw SW_RUNTIME_ERROR("No such check set: " + name);
    return *i->second;
}

void NativeCompiledTarget::setChecks(const String &name, bool check_definitions)
{
    if (DryRun)
        return;

    auto &checks_set = getChecks(name);
    checks_set.t = this;
    checks_set.performChecks(getMainBuild(), getSettings());

    // set results
    for (auto &&[k, c] : checks_set.getResults())
    {
        auto d = c->getDefinition(k);
        const auto v = c->Value.value();

        // make private?
        // remove completely?
        if (check_definitions && d)
            add(Definition{ d.value() });
        if (pystring::endswith(k, "_CODE"))
            Variables[k] = "#define " + k.substr(0, k.size() - 5) + " " + std::to_string(v);
        else
            Variables[k] = v;
    }
}

path NativeCompiledTarget::getPatchDir(bool binary_dir) const
{
    path base;
    if (auto d = getPackage().getOverriddenDir(); d)
        base = d.value() / SW_BINARY_DIR;
    else if (!isLocal())
        base = getPackage().getDirSrc();
    else
        base = getMainBuild().getBuildDirectory();
    return base / "patch";
}

void NativeCompiledTarget::writeFileOnce(const path &fn, const String &content)
{
    bool source_dir = false;
    path p = fn;
    if (!check_absolute(p, true, &source_dir))
    {
        // file does not exists
        if (!p.is_absolute())
        {
            p = BinaryDir / p;
            source_dir = false;
        }
    }

    // before resolving, we must set file as generated, to skip it on server
    // only in bdir case
    if (!source_dir)
    {
        File f(p, getFs());
        f.setGenerated();
    }

    if (DryRun)
        return;

    ::sw::writeFileOnce(p, content, getPatchDir(!source_dir));

    addFileSilently(p);

    //File f(p, getFs());
    //f.getFileRecord().load();
}

void NativeCompiledTarget::writeFileSafe(const path &fn, const String &content)
{
    if (DryRun)
        return;

    bool source_dir = false;
    path p = fn;
    if (!check_absolute(p, true, &source_dir))
        p = BinaryDir / p;
    ::sw::writeFileSafe(p, content, getPatchDir(!source_dir));

    addFileSilently(p);

    //File f(fn, getFs());
    //f.getFileRecord().load();
}

void NativeCompiledTarget::replaceInFileOnce(const path &fn, const String &from, const String &to)
{
    patch(fn, from, to);
}

void NativeCompiledTarget::patch(const path &fn, const String &from, const String &to)
{
    addFileSilently(fn);

    if (DryRun)
        return;

    bool source_dir = false;
    path p = fn;
    check_absolute(p, false, &source_dir);
    ::sw::replaceInFileOnce(p, from, to, getPatchDir(!source_dir));

    //File f(p, getFs());
    //f.getFileRecord().load();
}

void NativeCompiledTarget::patch(const path &fn, const String &patch_str)
{
    if (DryRun)
        return;

    bool source_dir = false;
    path p = fn;
    check_absolute(p, false, &source_dir);
    ::sw::patch(p, patch_str, getPatchDir(!source_dir));
}

void NativeCompiledTarget::deleteInFileOnce(const path &fn, const String &from)
{
    replaceInFileOnce(fn, from, "");
}

void NativeCompiledTarget::pushFrontToFileOnce(const path &fn, const String &text)
{
    addFileSilently(fn);

    if (DryRun)
        return;

    bool source_dir = false;
    path p = fn;
    check_absolute(p, false, &source_dir);
    ::sw::pushFrontToFileOnce(p, text, getPatchDir(!source_dir));

    //File f(p, getFs());
    //f.getFileRecord().load();
}

void NativeCompiledTarget::pushBackToFileOnce(const path &fn, const String &text)
{
    addFileSilently(fn);

    if (DryRun)
        return;

    bool source_dir = false;
    path p = fn;
    check_absolute(p, false, &source_dir);
    ::sw::pushBackToFileOnce(p, text, getPatchDir(!source_dir));

    //File f(p, getFs());
    //f.getFileRecord().load();
}

CompilerType NativeCompiledTarget::getCompilerType() const
{
    return ct;
}

LinkerType NativeCompiledTarget::getLinkerType() const
{
    return lt;
}

TargetType NativeCompiledTarget::getRealType() const
{
    if (isHeaderOnly())
        return TargetType::NativeHeaderOnlyLibrary;
    if (isStaticLibrary())
        return TargetType::NativeStaticLibrary;
    if (getType() == TargetType::NativeExecutable)
        return TargetType::NativeExecutable;
    return TargetType::NativeSharedLibrary;
}

#define STD(x)                                          \
    void NativeCompiledTarget::add(detail::__sw_##c##x) \
    {                                                   \
        CVersion = CLanguageStandard::c##x;             \
    }
#include "cstd.inl"
#undef STD

#define STD(x)                                            \
    void NativeCompiledTarget::add(detail::__sw_##gnu##x) \
    {                                                     \
        CVersion = CLanguageStandard::c##x;               \
        CExtensions = true;                               \
    }
#include "cstd.inl"
#undef STD

#define STD(x)                                            \
    void NativeCompiledTarget::add(detail::__sw_##cpp##x) \
    {                                                     \
        CPPVersion = CPPLanguageStandard::cpp##x;         \
    }
#include "cppstd.inl"
#undef STD

#define STD(x)                                              \
    void NativeCompiledTarget::add(detail::__sw_##gnupp##x) \
    {                                                       \
        CPPVersion = CPPLanguageStandard::cpp##x;           \
        CPPExtensions = true;                               \
    }
#include "cppstd.inl"
#undef STD

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
            if (0
                || getBuildSettings().TargetOS.Type == OSType::Windows
                || getBuildSettings().TargetOS.Type == OSType::Cygwin
                || getBuildSettings().TargetOS.Type == OSType::Mingw
                )
            {
                Private.Definitions[api] = "SW_EXPORT";
                Interface.Definitions[api] = "SW_IMPORT";
            }
            else
            {
                Public.Definitions[api] = "SW_EXPORT";
            }
        };

        if (SwDefinitions)
            Definitions["SW_EXECUTABLE"];

        set_api(ApiName);
        for (auto &a : ApiNames)
            set_api(a);
    }
    break;
    }

    return NativeCompiledTarget::prepare();
}

bool LibraryTarget::prepare()
{
    return prepareLibrary(getBuildSettings().Native.LibrariesType);
}

bool LibraryTarget::init()
{
    if (getBuildSettings().Native.LibrariesType == LibraryType::Shared)
        target_type = TargetType::NativeSharedLibrary;
    else
        target_type = TargetType::NativeStaticLibrary;

    auto r = NativeCompiledTarget::init();
    return r;
}

}
