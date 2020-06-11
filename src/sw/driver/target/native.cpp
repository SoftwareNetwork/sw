// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "native.h"

#include "../suffix.h"
#include "../bazel/bazel.h"
#include "../functions.h"
#include "../build.h"
#include "../command.h"
#include "../compiler/detect.h"

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

#define NATIVE_TARGET_DEF_SYMBOLS_FILE \
    (BinaryPrivateDir / ".sw.symbols.def")

#define RETURN_PREPARE_MULTIPASS_NEXT_PASS SW_RETURN_MULTIPASS_NEXT_PASS(prepare_pass)
#define RETURN_INIT_MULTIPASS_NEXT_PASS SW_RETURN_MULTIPASS_NEXT_PASS(init_pass)

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
SW_DEFINE_VISIBLE_FUNCTION_JUMPPAD(sw_create_def_file, create_def_file)

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

namespace sw
{

void NativeTarget::setOutputFile()
{
    /* || add a condition so user could change non build output dir*/
    //if (Scope == TargetScope::Build)
    {
        if (isStaticLibrary())
            getSelectedTool()->setOutputFile(getOutputFileName2("lib"));
        else
        {
            getSelectedTool()->setOutputFile(getOutputFileName2("bin"));
            getSelectedTool()->setImportLibrary(getOutputFileName2("lib"));
        }
    }
    /*else
    {
        SW_UNIMPLEMENTED;

        auto base = BinaryDir.parent_path() / "out" / ::sw::getOutputFileName(*this);
        getSelectedTool()->setOutputFile(base);
        if (getSelectedTool() != Librarian.get())
            getSelectedTool()->setImportLibrary(base);
    }*/

    // set generated early
    if (auto f = getOutputFile(); !f.empty())
        File(f, getFs()).setGenerated(true);
    if (auto f = getOutputFile(); !f.empty())
        File(f, getFs()).setGenerated(true);

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
    auto t = getSelectedTool();
    if (!t)
        throw SW_RUNTIME_ERROR("Empty selected tool: " + getPackage().toString());
    return t->getOutputFile();
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
    return getSelectedTool() && getSelectedTool() == Librarian.get();
}

bool NativeCompiledTarget::isStaticOrHeaderOnlyLibrary() const
{
    return isStaticLibrary() || *HeaderOnly;
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
    bool extended_desc = s.isObject();
    UnresolvedPackage id;
    if (extended_desc)
        id = s["package"].getValue();
    else
        id = s.getValue();
    return id;
}

void NativeCompiledTarget::activateCompiler(const TargetSetting &s, const StringSet &exts)
{
    bool extended_desc = s.isObject();
    auto id = get_settings_package_id(s);
    activateCompiler(s, id, exts, extended_desc);
}

void NativeCompiledTarget::activateCompiler(const TargetSetting &s, const UnresolvedPackage &id, const StringSet &exts, bool extended_desc)
{
    auto &cld = getMainBuild().getTargets();

    TargetSettings oss;
    oss["os"] = getSettings()["os"];
    auto i = cld.find(id, oss);
    if (!i)
    {
        i = getContext().getPredefinedTargets().find(id, oss);
        if (!i)
        {
            for (auto &e : exts)
                setExtensionProgram(e, id);
            return;
        }
    }
    auto t = i->as<PredefinedProgram *>();
    if (!t)
        throw SW_RUNTIME_ERROR("Target without PredefinedProgram: " + i->getPackage().toString());

    auto set_compiler_type = [this, &id, &exts](const auto &c)
    {
        for (auto &e : exts)
            setExtensionProgram(e, c->clone());

        if (0);
        else if (id.ppath == "com.Microsoft.VisualStudio.VC.cl")
            ct = CompilerType::MSVC;
        else if (id.ppath == "org.gnu.gcc" || id.ppath == "org.gnu.gpp")
            ct = CompilerType::GNU;
        else if (id.ppath == "org.LLVM.clang" || id.ppath == "org.LLVM.clangpp")
            ct = CompilerType::Clang;
        else if (id.ppath == "org.LLVM.clangcl")
            ct = CompilerType::ClangCl;
        else if (id.ppath == "com.intel.compiler.c" || id.ppath == "com.intel.compiler.cpp")
            ct = CompilerType::Intel;
        //else
            //throw SW_RUNTIME_ERROR("Unknown compiler type: " + id.toString());
    };

    auto c = std::dynamic_pointer_cast<CompilerBaseProgram>(t->getProgram().clone());
    if (c)
    {
        set_compiler_type(c);
        return;
    }

    bool created = false;
    auto create_command = [this, &created, &t, &c, &s, extended_desc]()
    {
        if (created)
            return;
        c->file = t->getProgram().file;
        auto C = c->createCommand(getMainBuild());
        static_cast<primitives::Command&>(*C) = *t->getProgram().getCommand();
        created = true;

        if (extended_desc && s["command"])
            targetSettings2Command(*C, s["command"]);
    };

    if (id.ppath == "com.Microsoft.VisualStudio.VC.cl")
    {
        c = std::make_shared<VisualStudioCompiler>();
        if (getSettings()["native"]["stdlib"]["cpp"].getValue() == "com.Microsoft.VisualStudio.VC.libcpp")
        {
            // take same ver as cl
            UnresolvedPackage up(getSettings()["native"]["stdlib"]["cpp"].getValue());
            up.range = id.range;
            *this += up;
            libstdcppset = true;
        }
    }
    else if (id.ppath == "com.Microsoft.VisualStudio.VC.ml")
        c = std::make_shared<VisualStudioASMCompiler>();
    else if (id.ppath == "com.Microsoft.Windows.rc")
        c = std::make_shared<RcTool>();
    else if (id.ppath == "org.gnu.gcc.as")
        c = std::make_shared<GNUASMCompiler>();
    else if (id.ppath == "org.gnu.gcc" || id.ppath == "org.gnu.gpp")
    {
        auto C = std::make_shared<GNUCompiler>();
        c = C;
        /*if (getBuildSettings().TargetOS.isApple())
        {
            C->VisibilityHidden = false;
            C->VisibilityInlinesHidden = false;
            C->PositionIndependentCode = false;
        }*/
    }
    else if (id.ppath == "org.LLVM.clang" || id.ppath == "org.LLVM.clangpp")
    {
        auto C = std::make_shared<ClangCompiler>();
        c = C;
        create_command();
        C->Target = getBuildSettings().getTargetTriplet();
        if (getBuildSettings().TargetOS.is(OSType::Windows))
        {
            auto c = C->createCommand(getMainBuild());
            // this one leaves default clang runtime library include path (from installed dir)
            c->push_back("-nostdlibinc");
            // this one cleans all default include dirs
            //c->push_back("-nostdinc");
            // clang gives error on reinterpret cast in offsetof macro in win ucrt
            *this += "_CRT_USE_BUILTIN_OFFSETOF"_def;
        }
        /*if (getBuildSettings().TargetOS.isApple())
        {
            C->VisibilityHidden = false;
            C->VisibilityInlinesHidden = false;
            C->PositionIndependentCode = false;
        }*/
    }
    else if (id.ppath == "org.LLVM.clangcl")
    {
        auto C = std::make_shared<ClangClCompiler>();
        c = C;
        create_command();

        {
            // we do everything ourselves
            // otherwise we get wrong order on clang includes and msvc includes (intrinsics and such)
            auto c = C->createCommand(getMainBuild());
            c->push_back("-nostdinc");
        }

        switch (getBuildSettings().TargetOS.Arch)
        {
        case ArchType::x86_64:
            C->CommandLineOptions<ClangClOptions>::Arch = clang::ArchType::m64;
            break;
        case ArchType::x86:
            C->CommandLineOptions<ClangClOptions>::Arch = clang::ArchType::m32;
            break;
        case ArchType::arm:
        {
            auto c = C->createCommand(getMainBuild());
            c->push_back("--target=arm-pc-windows-msvc");
            // set using target? check correctness then: improve getTargetTriplet()
        }
        break;
        case ArchType::aarch64:
        {
            auto c = C->createCommand(getMainBuild());
            c->push_back("--target=aarch64-pc-windows-msvc");
            // set using target? check correctness then: improve getTargetTriplet()
        }
        break;
        default:
            throw SW_RUNTIME_ERROR("Unknown arch");
        }

        auto c = C->createCommand(getMainBuild());
        // clang gives error on reinterpret cast in offsetof macro in win ucrt
        *this += "_CRT_USE_BUILTIN_OFFSETOF"_def;
    }
    else if (id.ppath == "com.intel.compiler.c" || id.ppath == "com.intel.compiler.cpp")
    {
        auto C = std::make_shared<VisualStudioCompiler>();
        c = C;
        C->ForceSynchronousPDBWrites = false;
        if (getSettings()["native"]["stdlib"]["cpp"].getValue() == "com.Microsoft.VisualStudio.VC.libcpp")
        {
            // take same ver as cl
            UnresolvedPackage up(getSettings()["native"]["stdlib"]["cpp"].getValue());
            up.range = id.range;
            *this += up;
            libstdcppset = true;
        }
    }
    else
        throw SW_RUNTIME_ERROR("Unknown compiler: " + id.toString());

    create_command();

    set_compiler_type(c);
}

std::shared_ptr<NativeLinker> NativeCompiledTarget::activateLinker(const TargetSetting &s)
{
    bool extended_desc = s.isObject();
    auto id = get_settings_package_id(s);
    return activateLinker(s, id, extended_desc);
}

std::shared_ptr<NativeLinker> NativeCompiledTarget::activateLinker(const TargetSetting &s, const UnresolvedPackage &id, bool extended_desc)
{
    auto &cld = getMainBuild().getTargets();

    TargetSettings oss;
    oss["os"] = getSettings()["os"];
    auto i = cld.find(id, oss);
    if (!i)
    {
        i = getContext().getPredefinedTargets().find(id, oss);
        if (!i)
            return {};
    }
    auto t = i->as<PredefinedProgram*>();
    if (!t)
        return {};

    std::shared_ptr<NativeLinker> c;

    bool created = false;
    auto create_command = [this, &created, &t, &c, &s, &extended_desc]()
    {
        if (created)
            return;
        c->file = t->getProgram().file;
        auto C = c->createCommand(getMainBuild());
        static_cast<primitives::Command&>(*C) = *t->getProgram().getCommand();
        created = true;

        if (extended_desc && s["command"])
            targetSettings2Command(*C, s["command"]);
    };

    if (0);
    else if (id.ppath == "com.Microsoft.VisualStudio.VC.lib")
    {
        c = std::make_shared<VisualStudioLibrarian>();
        c->Type = LinkerType::MSVC;
    }
    else if (id.ppath == "com.Microsoft.VisualStudio.VC.link" || id.ppath == "org.LLVM.lld.link")
    {
        c = std::make_shared<VisualStudioLinker>();
        c->Type = LinkerType::MSVC;
    }
    else if (id.ppath == "org.gnu.binutils.ar" || id.ppath == "org.LLVM.ar")
    {
        auto C = std::make_shared<GNULibrarian>();
        c = C;
        c->Type = LinkerType::GNU;
        C->Prefix = getBuildSettings().TargetOS.getLibraryPrefix();
    }
    else if (
        id.ppath == "org.gnu.gcc" ||
        id.ppath == "org.gnu.gpp" ||
        id.ppath == "org.LLVM.clang" ||
        id.ppath == "org.LLVM.clangpp"
        )
    {
        auto C = std::make_shared<GNULinker>();
        c = C;
        // actually it is depends on -fuse-ld option
        // do we need it at all?
        // probably yes, because user might provide different commands to ld and lld
        // is it true?
        c->Type = LinkerType::GNU;
        C->Prefix = getBuildSettings().TargetOS.getLibraryPrefix();
        if (getBuildSettings().TargetOS.isApple())
            C->use_start_end_groups = false;
        if (id.ppath == "org.LLVM.clang" ||
            id.ppath == "org.LLVM.clangpp")
        {
            create_command();
            auto cmd = c->createCommand(getMainBuild());
            cmd->push_back("-target");
            cmd->push_back(getBuildSettings().getTargetTriplet());
        }
        // TODO: find -fuse-ld option and set c->Type accordingly
    }
    else if (id.ppath == "org.gnu.gcc.ld")
    {
        SW_UNIMPLEMENTED;

        auto C = std::make_shared<GNULinker>();
        c = C;
        c->Type = LinkerType::GNU;
        C->Prefix = getBuildSettings().TargetOS.getLibraryPrefix();
    }
    else if (id.ppath == "org.LLVM.lld")
    {
        SW_UNIMPLEMENTED;

        auto C = std::make_shared<GNULinker>();
        c = C;
        c->Type = LinkerType::GNU;
        C->Prefix = getBuildSettings().TargetOS.getLibraryPrefix();

        create_command();

        auto cmd = c->createCommand(getMainBuild());
        //cmd->push_back("-fuse-ld=lld");
        cmd->push_back("-flavor");
        cmd->push_back("ld"); // for linux, TODO: add checks
        cmd->push_back("-eh-frame-hdr"); // needed
        if (getBuildSettings().TargetOS.is(OSType::Linux))
        {
            cmd->push_back("-dynamic-linker"); // needed
            cmd->push_back("/lib64/ld-linux-x86-64.so.2"); // needed
        }
        cmd->first_response_file_argument = 2;
        //cmd->push_back("-target");
        //cmd->push_back(getBuildSettings().getTargetTriplet());
    }
    else if (id.ppath == "com.intel.compiler.lib")
    {
        c = std::make_shared<VisualStudioLibrarian>();
        c->Type = LinkerType::MSVC;
    }
    else if (id.ppath == "com.intel.compiler.link")
    {
        c = std::make_shared<VisualStudioLinker>();
        c->Type = LinkerType::MSVC;
    }
    else
        throw SW_RUNTIME_ERROR("Unknown librarian/linker: " + id.toString());

    create_command();

    if (auto L = c->as<VisualStudioLibraryTool *>())
    {
        switch (getBuildSettings().TargetOS.Arch)
        {
        case ArchType::x86_64:
            L->Machine = vs::MachineType::X64;
            break;
        case ArchType::x86:
            L->Machine = vs::MachineType::X86;
            break;
        case ArchType::arm:
            L->Machine = vs::MachineType::ARM;
            break;
        case ArchType::aarch64:
            L->Machine = vs::MachineType::ARM64;
            break;
        default:
            SW_UNIMPLEMENTED;
        }
    }

    return c;
}

void NativeCompiledTarget::findCompiler()
{
    activateCompiler(getSettings()["native"]["program"]["cpp"], getCppSourceFileExtensions());
    activateCompiler(getSettings()["native"]["program"]["c"], { ".c" });

    if (ct == CompilerType::UnspecifiedCompiler)
        throw SW_RUNTIME_ERROR("Cannot find compiler " + get_settings_package_id(getSettings()["native"]["program"]["c"]).toString() + " for settings: " + getSettings().toString());

    if (getBuildSettings().TargetOS.is(OSType::Windows))
    {
        activateCompiler(getSettings()["native"]["program"]["asm"], { ".asm" });

        // actually a missing setting
        activateCompiler(getSettings()["native"]["program"]["rc"], "com.Microsoft.Windows.rc"s, { ".rc" }, false);
    }
    else
    {
        activateCompiler(getSettings()["native"]["program"]["asm"], { ".s", ".S", ".sx" });
    }

    if (!getBuildSettings().TargetOS.isApple())
    {
        removeExtension(".m");
        removeExtension(".mm");
    }

    Librarian = activateLinker(getSettings()["native"]["program"]["lib"]);
    if (!Librarian)
        throw SW_RUNTIME_ERROR("Librarian not found");

    Linker = activateLinker(getSettings()["native"]["program"]["link"]);
    if (!Linker)
        throw SW_RUNTIME_ERROR("Linker not found");

    Librarian->Extension = getBuildSettings().TargetOS.getStaticLibraryExtension();
    Linker->Extension = getBuildSettings().TargetOS.getSharedLibraryExtension();

    // c++ goes first for correct include order
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
}

bool NativeCompiledTarget::init()
{
    static std::once_flag f;
    std::call_once(f, [this] {detectNativeCompilers(DETECT_ARGS_PASS_FIRST_CALL_SIMPLE); });

    switch (init_pass)
    {
    case 1:
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

        // we set output file, but sometimes overridden call must set it later
        // (libraries etc.)
        // this one is used for executables
        setOutputFile();
    }
    RETURN_INIT_MULTIPASS_NEXT_PASS;
    case 2:
    {
        setOutputFile();
    }
    SW_RETURN_MULTIPASS_END(init_pass);
    }
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
                if (!*nt->HeaderOnly && nt->getSelectedTool() == nt->Linker.get())
                {
                    f(nt->getOutputFile());
                }
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

        a["PACKAGE_ROOT_DIR"] = q + normalize_path(getPackage().getPath().is_loc() ? RootDirectory : getPackage().getDirSrc()) + q;
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
    return getSelectedTool()->getImportLibrary();
}

std::unordered_set<NativeSourceFile*> NativeCompiledTarget::gatherSourceFiles() const
{
    return ::sw::gatherSourceFiles<NativeSourceFile>(getMergeObject());
}

FilesOrdered NativeCompiledTarget::gatherPrecompiledHeaders() const
{
    FilesOrdered hdrs;
    TargetOptionsGroup::iterate_this(
        [this, &hdrs](auto &v, auto i)
    {
        for (auto &i2 : v.PrecompiledHeaders)
            hdrs.push_back(i2);
    });
    return hdrs;
}

Files NativeCompiledTarget::gatherObjectFilesWithoutLibraries() const
{
    Files obj;
    for (auto &f : gatherSourceFiles())
    {
        if (f->skip_linking)
            continue;
        if (f->output.extension() != ".gch" &&
            f->output.extension() != ".pch"
            )
            obj.insert(f->output);
    }
    for (auto &[f, sf] : getMergeObject())
    {
        if (f.extension() == getBuildSettings().TargetOS.getObjectFileExtension())
            obj.insert(f);
    }
    return obj;
}

bool NativeCompiledTarget::hasSourceFiles() const
{
    bool r = false;

    auto check = [this, &r](auto &o)
    {
        if (!r)
        r |= std::any_of(o.begin(), o.end(), [this](const auto &f) {
            return f.second->isActive();
        });
        if (!r)
        r |= std::any_of(o.begin(), o.end(), [this](const auto &f) {
            return f.first.extension() == getBuildSettings().TargetOS.getObjectFileExtension();
            //|| f.first.extension() == ".def"
        });
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

void NativeCompiledTarget::resolvePostponedSourceFiles()
{
    // gather exts
    StringSet exts;
    for (auto &[f, sf] : getMergeObject())
    {
        if (!sf->isActive() || !sf->postponed)
            continue;
        //exts.insert(sf->file.extension().string());

        getMergeObject() += sf->file;
    }

    // activate langs
    for (auto &e : exts)
    {
    }

    // apply langs
    /*for (auto &[f, sf] : getMergeObject())
    {
        if (!sf->isActive() || !sf->postponed)
            continue;
        sf->file.extension();
        solution->getTarget();
    }*/
}

FilesOrdered NativeCompiledTarget::gatherLinkDirectories() const
{
    FilesOrdered dirs;
    auto get_ldir = [&dirs](const auto &a)
    {
        for (auto &d : a)
            dirs.push_back(d);
    };

    get_ldir(getMergeObject().NativeLinkerOptions::gatherLinkDirectories());
    get_ldir(getMergeObject().NativeLinkerOptions::System.gatherLinkDirectories());

    FilesOrdered dirs2;
    if (getSelectedTool())
        dirs2 = getSelectedTool()->gatherLinkDirectories();
    // tool dirs + lib dirs, not vice versa
    dirs2.insert(dirs2.end(), dirs.begin(), dirs.end());
    return dirs2;
}

LinkLibrariesType NativeCompiledTarget::gatherLinkLibraries() const
{
    LinkLibrariesType libs;
    const auto dirs = gatherLinkDirectories();
    for (auto &l : getMergeObject().LinkLibraries)
    {
        // reconsider
        // remove resolving?

        //if (l.is_absolute())
        {
            libs.push_back(LinkLibrary{ l });
            continue;
        }

        if (std::none_of(dirs.begin(), dirs.end(), [&l, &libs](auto &d)
        {
            if (fs::exists(d / l.l))
            {
                libs.push_back(LinkLibrary{ d / l.l });
                return true;
            }
            return false;
        }))
        {
            //LOG_TRACE(logger, "Cannot resolve library: " << l);
            throw SW_RUNTIME_ERROR(getPackage().toString() + ": Cannot resolve library: " + normalize_path(l.l));
        }

        //if (!getBuildSettings().TargetOS.is(OSType::Windows))
            //libs.push_back("-l" + l.u8string());
    }
    return libs;
}

NativeLinker *NativeCompiledTarget::getSelectedTool() const
{
    if (SelectedTool)
        return SelectedTool;
    if (Linker)
        return Linker.get();
    if (Librarian)
        return Librarian.get();
    if (isHeaderOnly())
        return {};
    throw SW_RUNTIME_ERROR("No tool selected");
}

void NativeCompiledTarget::createPrecompiledHeader()
{
    // disabled with PP
    if (PreprocessStep)
        return;

    auto files = gatherPrecompiledHeaders();
    if (files.empty())
        return;

    if (pch.name.empty())
        pch.name = "sw_pch";

    if (pch.dir.empty())
        pch.dir = BinaryDir.parent_path() / "pch";

    if (pch.files.empty())
        pch.files = files;

    String h;
    for (auto &f : pch.files)
    {
        if (f.string()[0] == '<' || f.string()[0] == '\"')
            h += "#include " + f.string() + "\n";
        else
            h += "#include \"" + normalize_path(f) + "\"\n";
    }
    pch.header = pch.get_base_pch_path() += ".h";
    {
        ScopedFileLock lk(pch.header);
        write_file_if_different(pch.header, h);
    }
    File(pch.header, getFs()).setGenerated(true); // prevents resolving issues

    pch.source = pch.get_base_pch_path() += ".cpp"; // msvc
    {
        ScopedFileLock lk(pch.source);
        write_file_if_different(pch.source, "#include \"" + normalize_path(pch.header) + "\"");
    }
    File(pch.source, getFs()).setGenerated(true); // prevents resolving issues

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
    auto sf = (getMergeObject()[pch.source]).as<NativeSourceFile *>();
    if (!sf)
        throw SW_RUNTIME_ERROR("Error creating pch");

    auto setup_create_vc = [this, &sf](auto &c)
    {
        if (getMainBuild().getSettings()["verbose"] == "true")
            getMergeObject()[pch.source].fancy_name += " (" + normalize_path(pch.source) + ")";

        sf->setOutputFile(pch.obj);

        c->PrecompiledHeaderFilename() = pch.pch;
        c->PrecompiledHeaderFilename.output_dependency = true;
        c->PrecompiledHeader().create = pch.header;
        c->PDBFilename = pch.pdb;
    };

    auto setup_create_gcc_clang = [this, &sf](auto &c)
    {
        sf->compiler->setSourceFile(pch.header, pch.pch);
        sf->output = sf->compiler->getOutputFile();

        if (getMainBuild().getSettings()["verbose"] == "true")
            getMergeObject()[pch.source].fancy_name += " (" + normalize_path(pch.header) + ")";

        c->Language = "c++-header"; // FIXME: also c-header sometimes
    };

    if (auto c = sf->compiler->as<VisualStudioCompiler*>())
    {
        setup_create_vc(c);
    }
    else if (auto c = sf->compiler->as<ClangClCompiler*>())
    {
        setup_create_vc(c);
    }
    else if (auto c = sf->compiler->as<ClangCompiler*>())
    {
        setup_create_gcc_clang(c);
    }
    else if (auto c = sf->compiler->as<GNUCompiler*>())
    {
        setup_create_gcc_clang(c);
    }
}

void NativeCompiledTarget::addPrecompiledHeader()
{
    if (pch.dir.empty())
        return;

    // on this step we setup compilers to USE our created pch
    for (auto &f : gatherSourceFiles())
    {
        auto sf = f->as<NativeSourceFile *>();
        if (!sf)
            continue;
        if (sf->skip_pch)
            continue;
        if (f->file == pch.source)
            continue;

        auto setup_use_vc = [this](auto &c)
        {
            c->ForcedIncludeFiles().insert(c->ForcedIncludeFiles().begin(), pch.header);
            c->PrecompiledHeaderFilename() = pch.pch;
            c->PrecompiledHeaderFilename.input_dependency = true;
            c->PrecompiledHeader().use = pch.header;
            c->PDBFilename = pch.pdb;
        };

        auto setup_use_gcc_clang = [this](auto &c)
        {
            c->ForcedIncludeFiles().insert(c->ForcedIncludeFiles().begin(), pch.header);
            // we must add this explicitly
            c->createCommand(getMainBuild())->addInput(pch.pch);
        };

        if (auto c = sf->compiler->as<VisualStudioCompiler*>())
        {
            setup_use_vc(c);
        }
        else if (auto c = sf->compiler->as<ClangClCompiler*>())
        {
            setup_use_vc(c);
        }
        else if (auto c = sf->compiler->as<ClangCompiler*>())
        {
            setup_use_gcc_clang(c);
        }
        else if (auto c = sf->compiler->as<GNUCompiler*>())
        {
            setup_use_gcc_clang(c);
        }
    }
}

std::shared_ptr<builder::Command> NativeCompiledTarget::getCommand() const
{
    if (HeaderOnly && *HeaderOnly)
        return nullptr;
    return getSelectedTool()->getCommand(*this);
}

Commands NativeCompiledTarget::getGeneratedCommands() const
{
    if (generated_commands)
        return generated_commands.value();
    generated_commands.emplace();

    Commands generated;

    const path def = NATIVE_TARGET_DEF_SYMBOLS_FILE;

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
        if (f == def)
            continue;
        auto c = p.getGenerator();
        if (c->strict_order > 0)
            order[c->strict_order].push_back(c);
        else
            generated.insert(c);
    }

    // respect ordering
    for (auto i = order.rbegin(); i != order.rend(); i++)
    {
        auto &cmds = i->second;
        for (auto &c : generated)
            c->dependencies.insert(cmds.begin(), cmds.end());
        generated.insert(cmds.begin(), cmds.end());
    }

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

    const path def = NATIVE_TARGET_DEF_SYMBOLS_FILE;

    // add generated files
    auto generated = getGeneratedCommands();

    Commands cmds;
    if (HeaderOnly && *HeaderOnly)
    {
        cmds.insert(generated.begin(), generated.end());
        return cmds;
    }

    // this source files
    {
        auto sd = normalize_path(SourceDir);
        auto bd = normalize_path(BinaryDir);
        auto bdp = normalize_path(BinaryPrivateDir);

        auto prepare_command = [this, &cmds, &sd, &bd, &bdp](auto f, auto c)
        {
            c->arguments.push_back(f->args);

            // set fancy name
            if (!IsSwConfig && !(getMainBuild().getSettings()["do_not_mangle_object_names"] == "true"))
            {
                auto p = normalize_path(f->file);
                if (bdp.size() < p.size() && p.find(bdp) == 0)
                {
                    auto n = p.substr(bdp.size());
                    c->name = "[" + getPackage().toString() + "]/[bdir_pvt]" + n;
                }
                else if (bd.size() < p.size() && p.find(bd) == 0)
                {
                    auto n = p.substr(bd.size());
                    c->name = "[" + getPackage().toString() + "]/[bdir]" + n;
                }
                if (sd.size() < p.size() && p.find(sd) == 0)
                {
                    String prefix;
                    /*if (f->compiler == getBuildSettings().Native.CCompiler)
                        prefix = "Building C object ";
                    else if (f->compiler == getBuildSettings().Native.CPPCompiler)
                        prefix = "Building CXX object ";*/
                    auto n = p.substr(sd.size());
                    if (!n.empty() && n[0] != '/')
                        n = "/" + n;
                    c->name = prefix + "[" + getPackage().toString() + "]" + n;
                }
            }
            if (!(getMainBuild().getSettings()["do_not_mangle_object_names"] == "true") && !f->fancy_name.empty())
                c->name = f->fancy_name;
            cmds.insert(c);
        };

        for (auto &f : gatherSourceFiles())
        {
            auto c = f->getCommand(*this);
            prepare_command(f, c);
        }

        for (auto &f : ::sw::gatherSourceFiles<RcToolSourceFile>(*this))
        {
            auto c = f->getCommand(*this);
            prepare_command(f, c);
        }
    }

    // add generated files
    for (auto &cmd : cmds)
    {
        cmd->dependencies.insert(generated.begin(), generated.end());

        for (auto &[k, v] : break_gch_deps)
        {
            auto input_pch = std::find_if(cmd->inputs.begin(), cmd->inputs.end(),
                [k = std::ref(k)](const auto &p)
            {
                return p == k;
            });
            if (input_pch == cmd->inputs.end())
                continue;

            for (auto &c : generated)
            {
                auto output_gch = std::find_if(c->outputs.begin(), c->outputs.end(),
                    [v = std::ref(v)](const auto &p)
                {
                    return p == v;
                });
                if (output_gch == c->outputs.end())
                    continue;

                cmd->dependencies.erase(c);
            }
        }
    }
    cmds.insert(generated.begin(), generated.end());

    // add install commands
    /*for (auto &[p, f] : getMergeObject())
    {
        if (f->install_dir.empty())
            continue;

        auto o = getOutputDir1();
        o /= f->install_dir / p.filename();

        auto c = addCommand(SW_VISIBLE_BUILTIN_FUNCTION(copy_file));
        c << cmd::in(p);
        c << cmd::out(o);
        c.c->name = "copy: " + normalize_path(o);
        cmds.insert(c.c);
    }*/

    // this library, check if nothing to link
    if (auto c = getCommand())
    {
        c->dependencies.insert(cmds.begin(), cmds.end());

        File d(def, getFs());
        if (d.isGenerated())
        {
            auto g = d.getGenerator();
            c->dependencies.insert(g);
            for (auto &c1 : cmds)
                g->dependencies.insert(c1);
            cmds.insert(g);
        }

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
                    if (auto c2 = c->as<driver::Command*>(); c2 && c2->ignore_deps_generated_commands)
                        continue;
                    c->dependencies.insert(cmds2.begin(), cmds2.end());
                }
            }
        }

        // link deps
        if (hasCircularDependency() || createWindowsRpath())
            cmds.insert(Librarian->getCommand(*this));

        cmds.insert(c);

        // set fancy name
        if (!IsSwConfig && !(getMainBuild().getSettings()["do_not_mangle_object_names"] == "true"))
        {
            c->name.clear();

            // not implemented
            /*if (getSolution().solutions.size() > 1)
            {
                auto i = std::find_if(getSolution().build->solutions.begin(), getSolution().build->solutions.end(), [this](auto &s)
                {
                    return &s == getSolution();
                });
                if (i == getSolution().build->solutions.end())
                    // add trace message?
                    ;// throw SW_RUNTIME_ERROR("Wrong sln");
                else
                    c->name += "sln [" + std::to_string(i - getSolution().build->solutions.begin() + 1) +
                        "/" + std::to_string(getSolution().build->solutions.size()) + "] ";
            }*/
            c->name += "[" + getPackage().toString() + "]" + getSelectedTool()->Extension;
        }

        // copy deps
        /*auto cdb = std::make_shared<ExecuteCommand>(true, [p = getPackage()(), c = getConfig()]
        {
            auto &sdb = getServiceDatabase();
            auto f = sdb.getInstalledPackageFlags(p, c);
            f.set(pfBuilt, true);
            sdb.setInstalledPackageFlags(p, c, f);
        });
        cdb->dependencies.insert(c);
        cmds.insert(cdb);*/
    }

    /*if (auto evs = Events.getCommands(); !evs.empty())
    {
        for (auto &c : cmds)
            c->dependencies.insert(evs.begin(), evs.end());
        cmds.insert(evs.begin(), evs.end());
    }*/

    /*if (!IsSwConfig && !Local)
    {
        if (!File(getOutputFile(), getFs()).isChanged())
            return {};
    }*/

    cmds.insert(this->cmds.begin(), this->cmds.end());

    return cmds;
}

bool NativeCompiledTarget::hasCircularDependency() const
{
    return
        1
        && getSelectedTool() == Linker.get()
        && circular_dependency
        ;
}

bool NativeCompiledTarget::createWindowsRpath() const
{
    // http://nibblestew.blogspot.com/2019/05/emulating-rpath-on-windows-via-binary.html
    return
        1
        && !IsSwConfig
        && getBuildSettings().TargetOS.is(OSType::Windows)
        && getSelectedTool() == Linker.get()
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
            throw SW_RUNTIME_ERROR("No bazel file found in SourceDir: " + normalize_path(SourceDir));

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

void NativeCompiledTarget::autoDetectOptions()
{
    // TODO: add dirs with first capital letter:
    // Include, Source etc.

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

    if (*HeaderOnly)
        s["header_only"] = "true";
    else
    {
        if (getType() != TargetType::NativeExecutable) // skip for exe atm
            s["import_library"].setPathValue(getContext().getLocalStorage(), getImportLibrary());
        s["output_file"].setPathValue(getContext().getLocalStorage(), getOutputFile());
        if (!OutputDir.empty())
            s["output_dir"] = normalize_path(OutputDir);
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

    if (getType() == TargetType::NativeExecutable)
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

bool NativeCompiledTarget::prepare()
{
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
        // link libraries
        prepare_pass6_1();
        RETURN_PREPARE_MULTIPASS_NEXT_PASS;
    case 8:
        // linker 1
        prepare_pass7();
    RETURN_PREPARE_MULTIPASS_NEXT_PASS;
    case 9:
        // linker 2
        prepare_pass8();
    RETURN_PREPARE_MULTIPASS_NEXT_PASS;
    case 10:
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
                + normalize_path(getContext().getLocalStorage().storage_dir)
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
            if (getCompilerType() == CompilerType::Clang)
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

    resolvePostponedSourceFiles();
    if (!HeaderOnly || !*HeaderOnly)
        HeaderOnly = !hasSourceFiles();
    if (*HeaderOnly)
    {
        Linker.reset();
        Librarian.reset();
        SelectedTool = nullptr;
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
            else if (getBuildSettings().TargetOS.isApple() && getCompilerType() == CompilerType::Clang)
                l.style = l.AppleLD;
            else
                l.style = l.GNU;
        }
        if (isStaticLibrary())
            l.static_ = true;
        Interface += l;
    }

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
            t = getContext().getPredefinedTargets().find(d.dep->getPackage(), d.dep->settings);
            if (!t)
                throw SW_RUNTIME_ERROR("No such target: " + d.dep->getPackage().toString());
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

    if (*HeaderOnly)
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

    // check postponed files first
    auto create_more_source_files = [this]()
    {
        std::vector<std::shared_ptr<NativeSourceFile>> new_files;
        for (auto &[p, f] : getMergeObject())
        {
            if (!f->postponed || f->skip)
                continue;

            auto ext = p.extension().string();
            auto prog = findProgramByExtension(ext);
            if (!prog)
                throw std::logic_error("User defined program not registered for " + ext);

            auto p2 = dynamic_cast<FileToFileTransformProgram *>(prog);
            if (!p2)
                throw SW_RUNTIME_ERROR("Bad program type");
            f = p2->createSourceFile(*this, p);
            addFile(p, f);
            if (auto f2 = std::dynamic_pointer_cast<NativeSourceFile>(f))
                new_files.push_back(f2);
        }
        for (auto &f : new_files)
        {
            File(f->output, getFs()).setGenerated();
            getMergeObject() += f->output;
        }
        return !new_files.empty();
    };

    while (create_more_source_files())
        ;

    // set build as property
    for (auto &[p, f] : getMergeObject())
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
                                  /*if (auto L = SourceFileStorage::findLanguageByExtension(".asm"); L)
                                  L->clone()->createSourceFile(f.first, this);
                                  else
                                  throw std::logic_error("no ASM language found");*/
                break;
            default:
                SW_UNREACHABLE;
            }
        }
    }

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

    auto files = gatherSourceFiles();

    // unity build
    if (UnityBuild)
    {
        std::vector<NativeSourceFile *> files2(files.begin(), files.end());
        std::sort(files2.begin(), files2.end(), [](const auto f1, const auto f2)
        {
            return f1->index < f2->index;
        });

        if (UnityBuildBatchSize < 0)
            UnityBuildBatchSize = 0;

        struct data
        {
            String s;
            int idx = 0;
            String ext;
        };

        data c, cpp;
        c.ext = ".c";
        cpp.ext = ".cpp";
        int fidx = 1; // for humans
        auto writef = [this, &fidx](auto &d)
        {
            if (d.s.empty())
                return;
            auto fns = "Module." + std::to_string(fidx++) + d.ext;
            auto fn = BinaryPrivateDir / "unity" / fns;
            write_file_if_different(fn, d.s); // do not trigger rebuilds
            getMergeObject() += fn; // after write
            getMergeObject()[fn].fancy_name = "[" + getPackage().toString() + "]/[unity]/" + fns;
            d.s.clear();
        };

        for (auto f : files2)
        {
            // skip when args are populated
            if (!f->args.empty())
                continue;

            auto ext = f->file.extension().string();
            auto cext = ext == ".c";
            auto cppext = getCppSourceFileExtensions().find(ext) != getCppSourceFileExtensions().end();
            // skip asm etc.
            if (!cext && !cppext)
                continue;

            // asm won't work here right now
            data &d = cext ? c : cpp;
            d.s += "#include \"" + normalize_path(f->file) + "\"\n";
            *this -= f->file;
            if (++d.idx % UnityBuildBatchSize == 0)
                writef(d);
        }
        writef(c);
        writef(cpp);

        // again
        files = gatherSourceFiles();
    }

    // merge file compiler options with target compiler options
    for (auto &f : files)
    {
        // set everything before merge!
        f->compiler->merge(*this);

        auto vs_setup = [this](auto *f, auto *c)
        {
            if (getBuildSettings().Native.MT)
                c->RuntimeLibrary = vs::RuntimeLibraryType::MultiThreaded;

            switch (getBuildSettings().Native.ConfigurationType)
            {
            case ConfigurationType::Debug:
                c->RuntimeLibrary =
                    getBuildSettings().Native.MT ?
                    vs::RuntimeLibraryType::MultiThreadedDebug :
                    vs::RuntimeLibraryType::MultiThreadedDLLDebug;
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
            if (f->file.extension() != ".c")
                c->CPPStandard = CPPVersion;

            // for static libs, we gather and put pdb near output file
            // btw, VS is clever enough to take this info from .lib
            /*if (getSelectedTool() == Librarian.get())
            {
            if ((getBuildSettings().Native.ConfigurationType == ConfigurationType::Debug ||
            getBuildSettings().Native.ConfigurationType == ConfigurationType::ReleaseWithDebugInformation) &&
            c->PDBFilename.empty())
            {
            auto f = getOutputFile();
            f = f.parent_path() / f.filename().stem();
            f += ".pdb";
            c->PDBFilename = f;// BinaryDir.parent_path() / "obj" / (getPackage().getPath().toString() + ".pdb");
            }
            }*/
        };

        auto gnu_setup = [this](auto *f, auto *c)
        {
            switch (getBuildSettings().Native.ConfigurationType)
            {
            case ConfigurationType::Debug:
                c->GenerateDebugInformation = true;
                //c->Optimizations().Level = 0; this is the default
                break;
            case ConfigurationType::Release:
                c->Optimizations().Level = 3;
                break;
            case ConfigurationType::ReleaseWithDebugInformation:
                c->GenerateDebugInformation = true;
                c->Optimizations().Level = 2;
                break;
            case ConfigurationType::MinimalSizeRelease:
                c->Optimizations().SmallCode = true;
                c->Optimizations().Level = 2;
                break;
            }
            if (f->file.extension() != ".c")
                c->CPPStandard = CPPVersion;
            else
                c->CStandard = CVersion;

            if (ExportAllSymbols && getSelectedTool() == Linker.get())
                c->VisibilityHidden = false;
        };

        if (auto c = f->compiler->as<VisualStudioCompiler*>())
        {
            if (UseModules)
            {
                c->UseModules = UseModules;
                //c->stdIfcDir = c->System.IncludeDirectories.begin()->parent_path() / "ifc" / (getBuildSettings().TargetOS.Arch == ArchType::x86_64 ? "x64" : "x86");
                c->stdIfcDir = c->System.IncludeDirectories.begin()->parent_path() / "ifc" / c->file.parent_path().filename();
                c->UTF8 = false; // utf8 is not used in std modules and produce a warning

                auto s = read_file(f->file);
                std::smatch m;
                static std::regex r("export module (\\w+)");
                if (std::regex_search(s, m, r))
                {
                    c->ExportModule = true;
                }
            }

            vs_setup(f, c);
        }
        else if (auto c = f->compiler->as<ClangClCompiler*>())
        {
            vs_setup(f, c);
        }
        // clang compiler is not working atm, gnu is created instead
        else if (auto c = f->compiler->as<ClangCompiler*>())
        {
            gnu_setup(f, c);
        }
        else if (auto c = f->compiler->as<GNUCompiler*>())
        {
            gnu_setup(f, c);
        }
    }

    // after merge
    if (PreprocessStep)
    {
        for (auto &f : files)
        {
            auto set_fancy_name = [this](auto &t, auto &cmd, auto *f)
            {
                if (getMainBuild().getSettings()["do_not_mangle_object_names"] == "true")
                    return;

                auto sd = normalize_path(t.SourceDir);
                auto bd = normalize_path(t.BinaryDir);
                auto bdp = normalize_path(t.BinaryPrivateDir);

                auto p = normalize_path(f->file);
                if (bdp.size() < p.size() && p.find(bdp) == 0)
                {
                    auto n = p.substr(bdp.size());
                    cmd->name = "[bdir_pvt]" + n;
                }
                else if (bd.size() < p.size() && p.find(bd) == 0)
                {
                    auto n = p.substr(bd.size());
                    cmd->name = "[bdir]" + n;
                }
                if (sd.size() < p.size() && p.find(sd) == 0)
                {
                    auto n = p.substr(sd.size());
                    if (!n.empty() && n[0] == '/')
                        n = n.substr(1);
                    cmd->name = n;
                }
                cmd->name = "[" + t.getPackage().toString() + "]/[preprocess]/" + cmd->name;
            };

            auto vs_setup = [&set_fancy_name](auto &t, auto *f, auto *c, auto &pp_command)
            {
                // create new cmd
                t.Storage.push_back(pp_command);

                // set pp
                pp_command->PreprocessToFile() = true;
                // prepare & register
                auto cmd = pp_command->getCommand(t);
                t.registerCommand(*cmd);

                // set input file for old command
                c->setSourceFile(pp_command->PreprocessFileName(), c->getOutputFile());

                set_fancy_name(t, cmd, f);
            };

            auto gnu_setup = [&set_fancy_name](auto &t, auto *f, auto *c, auto &pp_command)
            {
                // create new cmd
                t.Storage.push_back(pp_command);

                // set pp
                pp_command->CompileWithoutLinking = false;
                pp_command->Preprocess = true;
                auto o = pp_command->getOutputFile();
                o = o.parent_path() / o.stem() += ".i";
                pp_command->setOutputFile(o);
                // prepare & register
                auto cmd = pp_command->getCommand(t);
                t.registerCommand(*cmd);

                // set input file for old command
                c->setSourceFile(pp_command->getOutputFile(), c->getOutputFile());

                set_fancy_name(t, cmd, f);
            };

            //
            if (auto c = f->compiler->as<VisualStudioCompiler *>())
            {
                auto pp_command = f->compiler->clone();
                auto pp_command2 = std::static_pointer_cast<VisualStudioCompiler>(pp_command);
                vs_setup(*this, f, c, pp_command2);
            }
            else if (auto c = f->compiler->as<ClangClCompiler *>())
            {
                auto pp_command = f->compiler->clone();
                auto pp_command2 = std::static_pointer_cast<ClangClCompiler>(pp_command);
                vs_setup(*this, f, c, pp_command2);
            }
            else if (auto c = f->compiler->as<ClangCompiler *>())
            {
                auto pp_command = f->compiler->clone();
                auto pp_command2 = std::static_pointer_cast<ClangCompiler>(pp_command);
                gnu_setup(*this, f, c, pp_command2);
            }
            else if (auto c = f->compiler->as<GNUCompiler *>())
            {
                auto pp_command = f->compiler->clone();
                auto pp_command2 = std::static_pointer_cast<GNUCompiler>(pp_command);
                gnu_setup(*this, f, c, pp_command2);
            }
            else
                SW_UNIMPLEMENTED;
        }
    }

    // also merge rc files
    for (auto &f : ::sw::gatherSourceFiles<RcToolSourceFile>(*this))
    {
        // add casual idirs?
        f->compiler->idirs = NativeCompilerOptions::System.IncludeDirectories;
    }

    // generate rc, this one does not need idirs above
    if (GenerateWindowsResource
        && !*HeaderOnly
        && ::sw::gatherSourceFiles<RcToolSourceFile>(*this).empty()
        && getSelectedTool() == Linker.get()
        && getBuildSettings().TargetOS.is(OSType::Windows)
        && Scope == TargetScope::Build
        )
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
        ctx.addValueQuoted("FileDescription", { getPackage().getPath().back()/* + " - " + getConfig()*/ }); // remove config for now
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

        // more info for generators
        File(p, getFs()).setGenerated(true);

        operator+=(p);
    }

    addPrecompiledHeader();

    // pdb
    if (getSelectedTool())
    {
        if (auto c = getSelectedTool()->as<VisualStudioLinker *>())
        {
            if (!c->GenerateDebugInformation)
            {
                if (getBuildSettings().Native.ConfigurationType == ConfigurationType::Debug ||
                    getBuildSettings().Native.ConfigurationType == ConfigurationType::ReleaseWithDebugInformation)
                {
                    /*if (auto g = getSolution().getGenerator(); g && g->type == GeneratorType::VisualStudio)
                    c->GenerateDebugInformation = vs::link::Debug::FastLink;
                    else*/
                    c->GenerateDebugInformation = vs::link::Debug::Full;
                }
                else
                    c->GenerateDebugInformation = vs::link::Debug::None;
            }

            //if ((!c->GenerateDebugInformation || c->GenerateDebugInformation() != vs::link::Debug::None) &&
            if ((c->GenerateDebugInformation && c->GenerateDebugInformation() != vs::link::Debug::None) &&
                c->PDBFilename.empty())
            {
                auto f = getOutputFile();
                f = f.parent_path() / f.filename().stem();
                f += ".pdb";
                c->PDBFilename = f;// BinaryDir.parent_path() / "obj" / (getPackage().getPath().toString() + ".pdb");
            }
            else
                c->PDBFilename.output_dependency = false;

            if (Linker->Type == LinkerType::LLD)
            {
                if (c->GenerateDebugInformation)
                    c->InputFiles().push_back("msvcrtd.lib");
                else
                    c->InputFiles().push_back("msvcrt.lib");
            }
        }
    }

    // export all symbols
    if (ExportAllSymbols && getBuildSettings().TargetOS.Type == OSType::Windows && getSelectedTool() == Linker.get())
    {
        const path def = NATIVE_TARGET_DEF_SYMBOLS_FILE;
        Files objs;
        for (auto &f : files)
            objs.insert(f->output);
        auto c = addCommand(SW_VISIBLE_BUILTIN_FUNCTION(create_def_file));
        c << cmd::out(def);
        std::dynamic_pointer_cast<builder::BuiltinCommand>(c.getCommand())->push_back(objs);
        c->addInput(objs);
    }

    // add def file to linker
    if (getSelectedTool() && getSelectedTool() == Linker.get())
    {
        if (auto VSL = getSelectedTool()->as<VisualStudioLibraryTool*>())
        {
            for (auto &[p, f] : getMergeObject())
            {
                if (!f->skip && p.extension() == ".def")
                {
                    VSL->ModuleDefinitionFile = p;
                    HeaderOnly = false;
                }
            }
        }
    }

    // also fix rpath libname here
    if (getSelectedTool() && createWindowsRpath())
    {
        getSelectedTool()->setImportLibrary(getOutputFileName2("lib") += ".rp");
    }
}

void NativeCompiledTarget::prepare_pass6()
{
    // link libraries

    // link libs
    if (!getBuildSettings().TargetOS.is(OSType::Windows))
        return;

    auto rt = vs::RuntimeLibraryType::MultiThreadedDLL;
    if (getBuildSettings().Native.MT)
        rt = vs::RuntimeLibraryType::MultiThreaded;
    if (getBuildSettings().Native.ConfigurationType == ConfigurationType::Debug)
    {
        rt = vs::RuntimeLibraryType::MultiThreadedDLLDebug;
        if (getBuildSettings().Native.MT)
            rt = vs::RuntimeLibraryType::MultiThreadedDebug;
    }

    // TODO: move vs _slib to detect.cpp from native.cpp

    // https://docs.microsoft.com/en-us/cpp/c-runtime-library/crt-library-features?view=vs-2019

    // sometimes link.exe fails to add libs (SDL-2.0.10)
    // so we take full control here

    // we add main 5 libs and its variations for /MD /MDd /MT /MTd flags
    // (listed in reverse order):
    // 1. kernel (windows) library - kernel32.lib
    // 2. libc - ucrt.lib
    // 3. ms crt - msvcrt.lib
    // 4. compiler (cl.exe) library - vcruntime.lib
    // 5. ms std c++ library - msvcprt.lib
    // 6. concurrency crt (concrt.lib)
    //
    // we also add some other libs needed by msvc
    // 1. oldnames.lib - for backward compat - https://docs.microsoft.com/en-us/cpp/c-runtime-library/backward-compatibility?view=vs-2019
    // 2. concrt.lib - concurrency crt

    // TODO: push these libs from properties!

    // TODO: libs may have further versions like
    // libcpmt.lib
    // libcpmt1.lib
    //
    // libcpmtd.lib
    // libcpmtd0.lib
    // libcpmtd1.lib
    //
    // libconcrt.lib
    // libconcrt1.lib
    //
    // libconcrtd.lib
    // libconcrtd0.lib
    // libconcrtd1.lib

    auto &t = getMergeObject();

    switch (rt)
    {
    case vs::RuntimeLibraryType::MultiThreadedDLL:
        t += "concrt.lib"_slib;
        t += "vcruntime.lib"_slib;
        t += "msvcprt.lib"_slib;
        t += "msvcrt.lib"_slib;
        t += "ucrt.lib"_slib;
        break;
    case vs::RuntimeLibraryType::MultiThreadedDLLDebug:
        t += "concrtd.lib"_slib;
        t += "vcruntimed.lib"_slib;
        t += "msvcprtd.lib"_slib;
        t += "msvcrtd.lib"_slib;
        t += "ucrtd.lib"_slib;
        break;
    case vs::RuntimeLibraryType::MultiThreaded:
        t += "libconcrt.lib"_slib;
        t += "libvcruntime.lib"_slib;
        t += "libcpmt.lib"_slib;
        t += "libcmt.lib"_slib;
        t += "libucrt.lib"_slib;
        break;
    case vs::RuntimeLibraryType::MultiThreadedDebug:
        t += "libconcrtd.lib"_slib;
        t += "libvcruntimed.lib"_slib;
        t += "libcpmtd.lib"_slib;
        t += "libcmtd.lib"_slib;
        t += "libucrtd.lib"_slib;
        break;
    }
    if (auto L = getSelectedTool()->as<VisualStudioLinker *>())
    {
        auto cmd = L->createCommand(getMainBuild());
        cmd->push_back("-NODEFAULTLIB");
    }
}

void NativeCompiledTarget::prepare_pass6_1()
{
    if (isStaticOrHeaderOnlyLibrary())
        return;

    // circular deps detection
    if (auto L = Linker->as<VisualStudioLinker *>())
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
    }
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
                getMergeObject().LinkOptions.push_back(rpath_var + normalize_path(d));

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

    // after gatherStaticLinkLibraries()!
    if (getSelectedTool())
        getSelectedTool()->merge(getMergeObject());
}

void NativeCompiledTarget::prepare_pass8()
{
    // linker 2

    // linker setup
    auto obj = gatherObjectFilesWithoutLibraries();

    if (!isStaticOrHeaderOnlyLibrary())
    {
        for (auto &f : ::sw::gatherSourceFiles<RcToolSourceFile>(*this))
            obj.insert(f->output);
    }

    // circular and windows rpath processing
    processCircular(obj);

    if (getSelectedTool())
    {
        FilesOrdered files(obj.begin(), obj.end());
        std::sort(files.begin(), files.end());
        getSelectedTool()->setObjectFiles(files);
        getSelectedTool()->setInputLibraryDependencies(gatherLinkLibraries());
    }
}

void NativeCompiledTarget::prepare_pass9()
{
    clearGlobCache();
}

void NativeCompiledTarget::processCircular(Files &obj)
{
    if (!hasCircularDependency() && !createWindowsRpath())
        return;
    if (isStaticOrHeaderOnlyLibrary())
        return;

    auto lib_exe = Librarian->as<VisualStudioLibrarian*>();
    if (!lib_exe)
        throw SW_RUNTIME_ERROR("Unsupported librarian");

    auto link_exe = Linker->as<VisualStudioLinker*>();
    if (!link_exe)
        throw SW_RUNTIME_ERROR("Unsupported linker");

    // protect output file renaming
    static std::mutex m;

    auto name = Linker->getOutputFile().filename().u8string();
    if (createWindowsRpath())
    {
        Files dlls;
        SW_UNIMPLEMENTED;
        /*for (auto &d : Dependencies)
        {
            if (d->target == this)
                continue;
            if (d->isDisabledOrDummy())
                continue;
            if (d->IncludeDirectoriesOnly// || d->LinkLibrariesOnly
            )
                continue;

            auto nt = d->target->as<NativeCompiledTarget*>();

            if (!*nt->HeaderOnly)
            {
                if (nt->getSelectedTool() == nt->Linker.get())
                {
                    dlls.push_back(nt->getPackage().toString() + ".dll"); // in

                    // don't replace local targets' deps
                    if (d->target->isLocal())
                    {
                        // same as in
                        dlls.push_back(nt->getPackage().toString() + ".dll"); // out
                        continue;
                    }

                    path out;
                    String ext;
                    {
                        std::lock_guard lk(m);
                        ext = nt->getOutputFile().extension().u8string();
                        out = nt->getOutputFile().parent_path();
                    }
                    out = out.lexically_relative(getSolution().getContext().getLocalStorage().storage_dir);
                    out /= nt->getPackage().toString() + ext + ".rp" + ext;
                    dlls.push_back(out.u8string()); // out
                }
            }
        }*/

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
        out += ".rp" + out.extension().u8string();

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
    exp = exp.parent_path() / (exp.stem().u8string() + ".exp");
    Librarian->merge(getMergeObject());
    Librarian->prepareCommand(*this)->addOutput(exp);
    obj.insert(exp);
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

void NativeCompiledTarget::initLibrary(LibraryType Type)
{
    if (isHeaderOnly())
        return;
    if (Type == LibraryType::Shared)
    {
        if (Linker->Type == LinkerType::MSVC)
        {
            // set machine to target os arch
            auto L = Linker->as<VisualStudioLinker*>();
            L->Dll = true;
        }
        else if (Linker->Type == LinkerType::GNU)
        {
            auto L = Linker->as<GNULinker*>();
            L->SharedObject = true;
            if (getBuildSettings().TargetOS.Type == OSType::Linux)
                L->AsNeeded = true;
        }
        if (getBuildSettings().TargetOS.Type == OSType::Windows)
            Definitions["_WINDLL"];
    }
    else
    {
        SelectedTool = Librarian.get();
    }
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
    auto i = getSolution().checker.sets.find(name);
    if (i == getSolution().checker.sets.end())
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
    for (auto &[k, c] : checks_set.check_values)
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

bool ExecutableTarget::init()
{
    auto r = NativeCompiledTarget::init();

    switch (init_pass)
    {
    case 2:
    {
        Linker->Prefix.clear();
        Linker->Extension = getBuildSettings().TargetOS.getExecutableExtension();

        if (getSelectedTool())
        {
            if (auto c = getSelectedTool()->as<VisualStudioLinker*>())
            {
                c->ImportLibrary.output_dependency = false; // become optional
                c->ImportLibrary.create_directory = true; // but create always
            }
            else if (auto L = Linker->as<GNULinker*>())
            {
                L->PositionIndependentCode = false;
                L->SharedObject = false;
            }
        }
    }
    break;
    }

    return r;
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
    auto r = NativeCompiledTarget::init();
    initLibrary(getBuildSettings().Native.LibrariesType);
    return r;
}

path LibraryTarget::getImportLibrary() const
{
    if (isStaticLibrary())
        return getOutputFile();
    return getSelectedTool()->getImportLibrary();
}

bool StaticLibraryTarget::init()
{
    auto r = NativeCompiledTarget::init();
    initLibrary(LibraryType::Static);
    return r;
}

bool SharedLibraryTarget::init()
{
    auto r = NativeCompiledTarget::init();
    initLibrary(LibraryType::Shared);
    return r;
}

}
