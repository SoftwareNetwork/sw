// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "native.h"

#include "sw/driver/bazel/bazel.h"
#include "sw/driver/frontend/cppan/project.h"
#include "sw/driver/functions.h"
#include "sw/driver/build.h"

#include <sw/builder/jumppad.h>
#include <sw/core/sw_context.h>
#include <sw/manager/storage.h>
#include <sw/manager/yaml.h>

#include <boost/algorithm/string.hpp>
#include <nlohmann/json.hpp>
#include <primitives/constants.h>
#include <primitives/emitter.h>
#include <primitives/debug.h>
#include <primitives/sw/cl.h>

#include <charconv>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "target.native");

#define NATIVE_TARGET_DEF_SYMBOLS_FILE \
    (BinaryPrivateDir / ".sw.symbols.def")

#define RETURN_PREPARE_MULTIPASS_NEXT_PASS SW_RETURN_MULTIPASS_NEXT_PASS(prepare_pass)
#define RETURN_INIT_MULTIPASS_NEXT_PASS SW_RETURN_MULTIPASS_NEXT_PASS(init_pass)

extern bool gVerbose;

static cl::opt<bool> do_not_mangle_object_names("do-not-mangle-object-names");
//static cl::opt<bool> full_build("full", cl::desc("Full build (check all conditions)"));

static cl::opt<bool> standalone("standalone", cl::desc("Build standalone binaries"), cl::init(true));
static cl::alias standalone2("sa", cl::aliasopt(standalone));

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
    return 0;
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

NativeCompiledTarget::~NativeCompiledTarget()
{
    // incomplete type cannot be in default dtor
    // in our case it is nlohmann::json member
}

void NativeCompiledTarget::setOutputDir(const path &dir)
{
    //SwapAndRestore sr(OutputDir, dir);
    OutputDir = dir;
    setOutputFile();
}

void NativeCompiledTarget::findCompiler()
{
    // libc
    auto add_libc = [this](const PackagePath &pp)
    {
        auto &cld = getSolution().getChildren();
        auto i = cld.find(pp, getTargetSettings());
        if (!i)
            return false;
        if (!(*i).second)
            return false;
        (*this + *((*i).second))->sw_pushed = true;
        return true;
    };

    if (!add_libc(ts["native"]["stdlib"]["c"].getValue()))
        throw SW_RUNTIME_ERROR("no libc");
    if (!add_libc(ts["native"]["stdlib"]["c++"].getValue()))
        throw SW_RUNTIME_ERROR("no libc++");

    struct CompilerDesc
    {
        PackagePath id;
        StringSet exts;
    };

    using CompilerVector = std::vector<CompilerDesc>;

    auto activate_one = [this](const CompilerDesc &v)
    {
        auto &cld = getSolution().getChildren();

        auto i = cld.find(v.id, getTargetSettings());
        if (!i)
            return false;
        if (!(*i).second)
        {
            for (auto &e : v.exts)
                setExtensionProgram(e, PackageId{ v.id, (*i).first });
            return true;
        }
        if (auto t = (*i).second->as<PredefinedProgram*>())
        {
            auto c = std::dynamic_pointer_cast<CompilerBaseProgram>(t->getProgram().clone());
            if (!c)
            {
                if (v.id == "com.Microsoft.VisualStudio.VC.cl")
                    c = std::make_shared<VisualStudioCompiler>(getSolution().swctx);
                else if (v.id == "com.Microsoft.VisualStudio.VC.ml")
                    c = std::make_shared<VisualStudioASMCompiler>(getSolution().swctx);
                else if (v.id == "com.Microsoft.Windows.rc")
                    c = std::make_shared<RcTool>(getSolution().swctx);
                else
                    throw SW_RUNTIME_ERROR("Unknown compiler");
                (Program&)*c = t->getProgram();
            }
            for (auto &e : v.exts)
                setExtensionProgram(e, c->clone());
        }
        else
        {
            throw SW_RUNTIME_ERROR("Target without PredefinedProgram: " + (*i).first.toString());
        }
        return true;
    };

    auto activate_all = [this, &activate_one](const CompilerVector &a)
    {
        return std::all_of(a.begin(), a.end(), [this, &activate_one](const auto &v)
        {
            auto r = activate_one(v);
            if (r)
            {
                if (v.id == "com.Microsoft.VisualStudio.VC.cl")
                    ct = CompilerType::MSVC;
                else
                    throw SW_RUNTIME_ERROR("Unknown compiler");
            }
            return r;
        });
    };

    static const CompilerVector c_cpp =
    {
        {{ts["native"]["program"]["cpp"].getValue()}, getCppSourceFileExtensions()},
        {{ts["native"]["program"]["c"].getValue()}, { ".c" }},
    };

    if (!activate_all(c_cpp))
        throw SW_RUNTIME_ERROR("Missing native toolchain");

    // .asm
    if (ts["native"]["program"]["asm"])
    {
        static const CompilerDesc rc
        {
            ts["native"]["program"]["asm"].getValue(), { ".asm" },
        };
        if (!activate_one(rc))
            throw SW_RUNTIME_ERROR("Resource compiler was not found in Windows SDK");
    }

    // .rc
    if (getSettings().TargetOS.is(OSType::Windows))
    {
        static const CompilerDesc rc
        {
            "com.Microsoft.Windows.rc", { ".rc" },
        };
        if (!activate_one(rc))
            throw SW_RUNTIME_ERROR("Resource compiler was not found in Windows SDK");
    }

    // before linkers
    /*if (isClangFamily(getSettings().Native.CompilerType))
    {
        if (auto p = getProgram("org.LLVM.ld"))
        {
            if (auto l = p->template as<GNULinker>())
            {
                auto cmd = l->createCommand(getSolution().swctx);
                cmd->args.push_back("-fuse-ld=lld");
                cmd->args.push_back("-target");
                cmd->args.push_back(getSettings().getTargetTriplet());
            }
        }
    }*/

    if (getSettings().TargetOS.Type != OSType::Macos)
    {
        removeExtension(".m");
        removeExtension(".mm");
    }

    // lib/link
    auto activate_lib_link_or_throw = [this](const PackagePath &p, bool link = false)
    {
        auto &cld = getSolution().getChildren();

        auto i = cld.find(p, getTargetSettings());
        if (!i)
            return false;
        if (!(*i).second)
            return false;
        auto t = (*i).second->as<PredefinedProgram*>();
        if (t)
        {
            auto set_arch = [this](auto in)
            {
                if (auto L = in->as<VisualStudioLibraryTool *>())
                {
                    switch (getSettings().TargetOS.Arch)
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
            };

            auto nl = std::dynamic_pointer_cast<NativeLinker>(t->getProgram().clone());
            if (!nl)
            {
                if (link)
                    nl = std::make_shared<VisualStudioLinker>(getSolution().swctx);
                else
                    nl = std::make_shared<VisualStudioLibrarian>(getSolution().swctx);
                nl->Type = LinkerType::MSVC;
                (Program&)*nl = t->getProgram();
            }
            if (link)
                this->Linker = nl;
            else
                this->Librarian = nl;
            set_arch(nl);
            return true;
        }

        return false;
    };

    if (!activate_lib_link_or_throw(ts["native"]["program"]["lib"].getValue()))
        throw SW_RUNTIME_ERROR("Try to add more librarians");
    if (!activate_lib_link_or_throw(ts["native"]["program"]["link"].getValue(), true))
        throw SW_RUNTIME_ERROR("Try to add more linkers");
}

bool NativeCompiledTarget::init()
{
    switch (init_pass)
    {
    case 1:
    {
        // propagate this pointer to all
        TargetOptionsGroup::iterate([this](auto &v, auto i)
        {
            v.target = this;
        });

        if (!isHeaderOnly())
            findCompiler();

        // early setup compilers after libc, libcpp
        merge();
        if (!isHeaderOnly())
        {
            if (auto c = findProgramByExtension(".c")->as<NativeCompiler *>())
                c->merge(*this);
            if (auto c = findProgramByExtension(".cpp")->as<NativeCompiler *>())
                c->merge(*this);
        }

        // after compilers
        Target::init();

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
    SW_RETURN_MULTIPASS_END;
    }
    SW_RETURN_MULTIPASS_END;
}

void NativeCompiledTarget::setupCommand(builder::Command &c) const
{
    NativeTarget::setupCommand(c);

    // perform this after prepare?
    auto for_deps = [this](auto &a)
    {
        for (auto &d : getAllDependencies())
        {
            if (&d->getTarget() == this)
                continue;
            //if (!d->isRuntime())
            {
                if (d->IncludeDirectoriesOnly)
                    continue;
            }

            auto nt = d->getTarget().as<NativeCompiledTarget*>();
            if (!nt)
                continue;
            if (!*nt->HeaderOnly && nt->getSelectedTool() == nt->Linker.get())
            {
                a(nt);
            }
        }
    };

    if (standalone)
    {
        for_deps([&c](auto nt)
        {
            c.addPathDirectory(nt->getOutputFile().parent_path());
        });
        return;
    }

    // more under if (createWindowsRpath())?
    c.addPathDirectory(getSolution().swctx.getLocalStorage().storage_dir);

    if (createWindowsRpath())
    {
        for_deps([&c](auto nt)
        {
            // dlls, when emulating rpath, are created after executables and commands running them
            // so we put explicit dependency on them
            c.addInput(nt->getOutputFile());
        });
    }
}

driver::CommandBuilder NativeCompiledTarget::addCommand() const
{
    driver::CommandBuilder cb(getSolution().swctx);
    // set as default
    // source dir contains more files than bdir?
    // sdir or bdir?
    cb.c->working_directory = SourceDir;
    //setupCommand(*cb.c);
    cb << *this; // this adds to storage
    return cb;
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
        a["PACKAGE"] = q + getPackage().ppath.toString() + q;
        a["PACKAGE_NAME"] = q + getPackage().ppath.toString() + q;
        a["PACKAGE_NAME_LAST"] = q + getPackage().ppath.back() + q;
        a["PACKAGE_VERSION"] = q + getPackage().version.toString() + q;
        a["PACKAGE_STRING"] = q + getPackage().toString() + q;
        a["PACKAGE_BUILD_CONFIG"] = q + getConfig() + q;
        a["PACKAGE_BUGREPORT"] = q + q;
        a["PACKAGE_URL"] = q + q;
        a["PACKAGE_TARNAME"] = q + getPackage().ppath.toString() + q; // must be lowercase version of PACKAGE_NAME
        a["PACKAGE_VENDOR"] = q + getPackage().ppath.getOwner() + q;
        a["PACKAGE_YEAR"] = std::to_string(1900 + t.tm_year); // custom
        a["PACKAGE_COPYRIGHT_YEAR"] = std::to_string(1900 + t.tm_year);

        a["PACKAGE_ROOT_DIR"] = q + normalize_path(getPackage().ppath.is_loc() ? RootDirectory : getPackage().getDirSrc()) + q;
        a["PACKAGE_NAME_WITHOUT_OWNER"] = q/* + getPackage().ppath.slice(2).toString()*/ + q;
        a["PACKAGE_NAME_CLEAN"] = q + (getPackage().ppath.is_loc() ? getPackage().ppath.slice(2).toString() : getPackage().ppath.toString()) + q;

        //"@PACKAGE_CHANGE_DATE@"
            //"@PACKAGE_RELEASE_DATE@"

        a["PACKAGE_VERSION_MAJOR"] = std::to_string(getPackage().version.getMajor());
        a["PACKAGE_VERSION_MINOR"] = std::to_string(getPackage().version.getMinor());
        a["PACKAGE_VERSION_PATCH"] = std::to_string(getPackage().version.getPatch());
        a["PACKAGE_VERSION_TWEAK"] = std::to_string(getPackage().version.getTweak());
        a["PACKAGE_VERSION_NUM"] = "0x" + ver2hex(getPackage().version, 2) + "LL";
        a["PACKAGE_VERSION_MAJOR_NUM"] = n2hex(getPackage().version.getMajor(), 2);
        a["PACKAGE_VERSION_MINOR_NUM"] = n2hex(getPackage().version.getMinor(), 2);
        a["PACKAGE_VERSION_PATCH_NUM"] = n2hex(getPackage().version.getPatch(), 2);
        a["PACKAGE_VERSION_TWEAK_NUM"] = n2hex(getPackage().version.getTweak(), 2);
        a["PACKAGE_VERSION_NUM2"] = "0x" + ver2hex(getPackage().version, 4) + "LL";
        a["PACKAGE_VERSION_MAJOR_NUM2"] = n2hex(getPackage().version.getMajor(), 4);
        a["PACKAGE_VERSION_MINOR_NUM2"] = n2hex(getPackage().version.getMinor(), 4);
        a["PACKAGE_VERSION_PATCH_NUM2"] = n2hex(getPackage().version.getPatch(), 4);
        a["PACKAGE_VERSION_TWEAK_NUM2"] = n2hex(getPackage().version.getTweak(), 4);
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

path NativeCompiledTarget::getOutputDir() const
{
    if (OutputDir.empty())
        return getOutputFile().parent_path();
    return getTargetsDir() / OutputDir;
}

void NativeCompiledTarget::setOutputFile()
{
    if (isHeaderOnly())
        return;

    /* || add a considiton so user could change nont build output dir*/
    if (Scope == TargetScope::Build)
    {
        if (getSelectedTool() == Librarian.get())
            getSelectedTool()->setOutputFile(getOutputFileName2("lib"));
        else
        {
            if (getType() == TargetType::NativeExecutable)
                getSelectedTool()->setOutputFile(getOutputFileName2("bin"));
            else
                getSelectedTool()->setOutputFile(getOutputFileName2("bin"));
            getSelectedTool()->setImportLibrary(getOutputFileName2("lib"));
        }
    }
    else
    {
        auto base = BinaryDir.parent_path() / "out" / getOutputFileName();
        getSelectedTool()->setOutputFile(base);
        if (getSelectedTool() != Librarian.get())
            getSelectedTool()->setImportLibrary(base);
    }

    // set generated early
    if (auto f = getOutputFile(); !f.empty())
        File(f, getFs()).setGenerated(true);
    if (auto f = getOutputFile(); !f.empty())
        File(f, getFs()).setGenerated(true);
}

path Target::getOutputFileName() const
{
    return getPackage().toString();
}

/*String NativeCompiledTarget::getConfigRaw() const
{
    auto c = Target::getConfigRaw();
    addConfigElement(c, toString(getCompilerType()));
    auto p = findProgramByExtension(".cpp");
    if (!p)
        throw std::logic_error("no cpp compiler");
    addConfigElement(c, p->getVersion().toString(2));
    return c;
}*/

path NativeCompiledTarget::getOutputFileName(const path &root) const
{
    path p;
    if (IsConfig)
    {
        p = getSolution().BinaryDir / "cfg" / getConfig() / getOutputFileName();
    }
    else if (isLocal())
    {
        p = getTargetsDir() / OutputDir / getOutputFileName();
    }
    else
    {
        p = root / getConfig() / OutputDir / getOutputFileName();
    }
    return p;
}

path NativeCompiledTarget::getOutputFileName2(const path &subdir) const
{
    if (isLocal())
    {
        return getOutputFileName("");
    }
    else
    {
        if (IsConfig)
            return getOutputFileName("");
        else
            return BinaryDir.parent_path() / subdir / getOutputFileName();
    }
}

path NativeCompiledTarget::getOutputFile() const
{
    if (!outputfile.empty())
        return outputfile;
    return getSelectedTool()->getOutputFile();
}

path NativeCompiledTarget::getImportLibrary() const
{
    return getSelectedTool()->getImportLibrary();
}

NativeCompiledTarget::TargetsSet NativeCompiledTarget::gatherDependenciesTargets() const
{
    TargetsSet deps;
    for (auto &d : getAllDependencies())
    {
        if (&d->getTarget() == this)
            continue;
        if (d->IncludeDirectoriesOnly)
            continue;
        deps.insert(&d->getTarget());
    }
    return deps;
}

NativeCompiledTarget::TargetsSet NativeCompiledTarget::gatherAllRelatedDependencies() const
{
    auto libs = gatherDependenciesTargets();
    while (1)
    {
        auto sz = libs.size();
        for (auto &d : libs)
        {
            auto dt = d->as<NativeCompiledTarget*>();
            if (!dt)
                continue;
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

std::unordered_set<NativeSourceFile*> NativeCompiledTarget::gatherSourceFiles() const
{
    return ::sw::gatherSourceFiles<NativeSourceFile>(*this);
}

Files NativeCompiledTarget::gatherIncludeDirectories() const
{
    Files idirs;
    TargetOptionsGroup::iterate(
        [this, &idirs](auto &v, auto i)
    {
        auto idirs2 = v.gatherIncludeDirectories();
        for (auto &i2 : idirs2)
            idirs.insert(i2);
    });
    return idirs;
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

bool NativeCompiledTarget::hasSourceFiles() const
{
    return std::any_of(this->begin(), this->end(), [](const auto &f) {
               return f.second->isActive();
           }) ||
           std::any_of(this->begin(), this->end(), [](const auto &f) {
               return f.first.extension() == ".obj"
                   //|| f.first.extension() == ".def"
                   ;
           });
}

void NativeCompiledTarget::resolvePostponedSourceFiles()
{
    // gather exts
    StringSet exts;
    for (auto &[f, sf] : *this)
    {
        if (!sf->isActive() || !sf->postponed)
            continue;
        //exts.insert(sf->file.extension().string());

        *this += sf->file;
    }

    // activate langs
    for (auto &e : exts)
    {
    }

    // apply langs
    /*for (auto &[f, sf] : *this)
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

    get_ldir(NativeLinkerOptions::gatherLinkDirectories());
    get_ldir(NativeLinkerOptions::System.gatherLinkDirectories());

    FilesOrdered dirs2;
    if (getSelectedTool())
        dirs2 = getSelectedTool()->gatherLinkDirectories();
    // tool dirs + lib dirs, not vice versa
    dirs2.insert(dirs2.end(), dirs.begin(), dirs.end());
    return dirs2;
}

FilesOrdered NativeCompiledTarget::gatherLinkLibraries() const
{
    FilesOrdered libs;
    const auto dirs = gatherLinkDirectories();
    for (auto &l : LinkLibraries)
    {
        // reconsider
        // remove resolving?

        if (l.is_absolute())
        {
            libs.push_back(l);
            continue;
        }

        if (std::none_of(dirs.begin(), dirs.end(), [&l, &libs](auto &d)
        {
            if (fs::exists(d / l))
            {
                libs.push_back(d / l);
                return true;
            }
            return false;
        }))
        {
            //LOG_TRACE(logger, "Cannot resolve library: " << l);
            throw SW_RUNTIME_ERROR(getPackage().toString() + ": Cannot resolve library: " + normalize_path(l));
        }

        //if (!getSettings().TargetOS.is(OSType::Windows))
            //libs.push_back("-l" + l.u8string());
    }
    return libs;
}

Files NativeCompiledTarget::gatherObjectFiles() const
{
    auto obj = gatherObjectFilesWithoutLibraries();
    auto ll = gatherLinkLibraries();
    obj.insert(ll.begin(), ll.end());
    return obj;
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

void NativeCompiledTarget::addPrecompiledHeader(const path &h, const path &cpp)
{
    PrecompiledHeader pch;
    pch.header = h;
    pch.source = cpp;
    addPrecompiledHeader(pch);
}

void NativeCompiledTarget::addPrecompiledHeader(PrecompiledHeader &p)
{
    /*check_absolute(p.header);
    if (!p.source.empty())
        check_absolute(p.source);*/

    bool force_include_pch_header_to_pch_source = true;
    bool force_include_pch_header_to_target_source_files = p.force_include_pch;
    auto &pch = p.source;
    path pch_dir = BinaryDir.parent_path() / "pch";
    if (!pch.empty())
    {
        if (!fs::exists(pch))
            write_file_if_different(pch, "");
        pch_dir = pch.parent_path();
        force_include_pch_header_to_pch_source = p.force_include_pch_to_source;
    }
    else
    {
        pch = pch_dir / (p.header.stem().string() + ".cpp");
        write_file_if_different(pch, "");
    }

    auto pch_fn = pch.parent_path() / (pch.stem().string() + ".pch");
    auto obj_fn = pch.parent_path() / (pch.stem().string() + ".obj");
    auto pdb_fn = pch.parent_path() / (pch.stem().string() + ".pdb");

    // gch always uses header filename + .gch
    auto gch_fn = pch.parent_path() / (p.header.filename().string() + ".gch");
    auto gch_fn_clang = pch.parent_path() / (p.header.filename().string() + ".pch");
#ifndef _WIN32
    pch_dir = getSolution().swctx.getLocalStorage().storage_dir_tmp;
    gch_fn = getSolution().swctx.getLocalStorage().storage_dir_tmp / "sw/driver/sw.h.gch";
#endif

    auto setup_use_vc = [&force_include_pch_header_to_target_source_files, &p, &pch_fn, &pdb_fn](auto &c)
    {
        if (force_include_pch_header_to_target_source_files)
            c->ForcedIncludeFiles().push_back(p.header);
        c->PrecompiledHeaderFilename() = pch_fn;
        c->PrecompiledHeaderFilename.input_dependency = true;
        c->PrecompiledHeader().use = p.header;
        c->PDBFilename = pdb_fn;
        //c->PDBFilename.intermediate_file = false;
    };

    // before adding pch source file to target
    // on this step we setup compilers to USE our created pch
    // MSVC does it explicitly, gnu does implicitly; check what about clang
    CompilerType cc = CompilerType::UnspecifiedCompiler;
    for (auto &f : gatherSourceFiles())
    {
        if (auto sf = f->as<NativeSourceFile*>())
        {
            if (auto c = sf->compiler->as<VisualStudioCompiler*>())
            {
                cc = c->Type;
                setup_use_vc(c);
            }
            else if (auto c = sf->compiler->as<ClangClCompiler*>())
            {
                cc = c->Type;
                setup_use_vc(c);
            }
            else if (auto c = sf->compiler->as<ClangCompiler*>())
            {
                cc = c->Type;

                if (force_include_pch_header_to_target_source_files)
                    c->ForcedIncludeFiles().push_back(p.header);

                c->PrecompiledHeader = gch_fn_clang;
                c->createCommand(getSolution().swctx)->addInput(gch_fn_clang);
            }
            else if (auto c = sf->compiler->as<GNUCompiler*>())
            {
                cc = c->Type;

                if (force_include_pch_header_to_target_source_files)
                    c->ForcedIncludeFiles().push_back(p.header);

                c->createCommand(getSolution().swctx)->addInput(gch_fn);
            }
        }
    }

    // on this step we setup compilers to CREATE our pch
    if (!p.created)
    {
        *this += pch;
        (*this)[pch].fancy_name = "[config pch]";
        if (auto sf = ((*this)[pch]).as<NativeSourceFile*>())
        {
            auto setup_create_vc = [this, &pch, &sf, &force_include_pch_header_to_pch_source, &p, &pch_fn, &pdb_fn, &obj_fn](auto &c)
            {
                if (gVerbose)
                    (*this)[pch].fancy_name += " (" + normalize_path(pch) + ")";

                sf->setOutputFile(obj_fn);

                if (force_include_pch_header_to_pch_source)
                    c->ForcedIncludeFiles().push_back(p.header);
                c->PrecompiledHeaderFilename() = pch_fn;
                c->PrecompiledHeaderFilename.output_dependency = true;
                c->PrecompiledHeader().create = p.header;
                c->PDBFilename = pdb_fn;
                //c->PDBFilename.intermediate_file = false;
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
                if (gVerbose)
                    (*this)[pch].fancy_name += " (" + normalize_path(gch_fn_clang) + ")";

                sf->setOutputFile(gch_fn_clang);
                c->Language = "c++-header";
                if (force_include_pch_header_to_pch_source)
                    c->ForcedIncludeFiles().push_back(p.header);
                c->EmitPCH = true;
            }
            else if (auto c = sf->compiler->as<GNUCompiler*>())
            {
                if (gVerbose)
                    (*this)[pch].fancy_name += " (" + normalize_path(gch_fn) + ")";

                sf->setOutputFile(gch_fn);
                c->Language = "c++-header";
                if (force_include_pch_header_to_pch_source)
                    c->ForcedIncludeFiles().push_back(p.header);

                IncludeDirectories.insert(pch_dir);
            }
            p.created = true;
        }
    }
    else
    {
        switch (cc)
        {
        case CompilerType::MSVC:
        case CompilerType::ClangCl:
            *this += obj_fn;
            break;
        case CompilerType::Clang:
            break;
        case CompilerType::GNU:
            break;
        default:
            throw SW_RUNTIME_ERROR("unknown compiler for pch");
        }
    }
}

NativeCompiledTarget &NativeCompiledTarget::operator=(PrecompiledHeader &pch)
{
    addPrecompiledHeader(pch);
    return *this;
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
    for (auto &[f, _] : *this)
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

    // also add deps to all deps' generated commands
    Commands deps_commands;
    /*for (auto &f : FileDependencies)
    {
        File p(f, getFs());
        if (!p.isGenerated())
            continue;
        auto c = p.getFileRecord().getGenerator();
        deps_commands.insert(c); // gather deps' commands
    }*/

    // make our commands to depend on gathered
    //for (auto &c : generated)
        //c->dependencies.insert(deps_commands.begin(), deps_commands.end());

    // and now also insert deps' commands to list
    // this is useful when our generated list is empty
    //if (generated.empty())
    generated.insert(deps_commands.begin(), deps_commands.end());

    generated_commands = generated;
    return generated;
}

Commands NativeCompiledTarget::getCommands1() const
{
    if (getSolution().skipTarget(Scope))
        return {};

    if (already_built)
        return {};

    const path def = NATIVE_TARGET_DEF_SYMBOLS_FILE;

    // add generated files
    auto generated = getGeneratedCommands();

    Commands cmds;
    if (HeaderOnly && *HeaderOnly)
    {
        //LOG_TRACE(logger, "target " << getPackage().toString() << " is header only");
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
            if (!IsConfig && !do_not_mangle_object_names)
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
                    /*if (f->compiler == getSettings().Native.CCompiler)
                        prefix = "Building C object ";
                    else if (f->compiler == getSettings().Native.CPPCompiler)
                        prefix = "Building CXX object ";*/
                    auto n = p.substr(sd.size());
                    if (!n.empty() && n[0] != '/')
                        n = "/" + n;
                    c->name = prefix + "[" + getPackage().toString() + "]" + n;
                }
            }
            if (!do_not_mangle_object_names && !f->fancy_name.empty())
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
    for (auto &[p, f] : *this)
    {
        if (f->install_dir.empty())
            continue;

        auto o = getOutputDir();
        o /= f->install_dir / p.filename();

        SW_MAKE_EXECUTE_BUILTIN_COMMAND(copy_cmd, *this, "sw_copy_file", nullptr);
        copy_cmd->arguments.push_back(p.u8string());
        copy_cmd->arguments.push_back(o.u8string());
        copy_cmd->addInput(p);
        copy_cmd->addOutput(o);
        copy_cmd->name = "copy: " + normalize_path(o);
        copy_cmd->maybe_unused = builder::Command::MU_ALWAYS;
        cmds.insert(copy_cmd);
    }

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
            for (auto &d : getAllDependencies())
            {
                if (d->IncludeDirectoriesOnly && !d->GenerateCommandsBefore)
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
                auto cmds2 = nt->getGeneratedCommands();
                for (auto &c : cmds)
                {
                    if (auto c2 = c->as<driver::detail::Command*>(); c2 && c2->ignore_deps_generated_commands)
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
        if (!IsConfig && !do_not_mangle_object_names)
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

    /*if (!IsConfig && !Local)
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
        //|| (getSolution().getGenerator())
        ;
}

bool NativeCompiledTarget::createWindowsRpath() const
{
    // http://nibblestew.blogspot.com/2019/05/emulating-rpath-on-windows-via-binary.html
    return
        1
        && !IsConfig
        && getSettings().TargetOS.is(OSType::Windows)
        && getSelectedTool() == Linker.get()
        //&& !getSolution().getGenerator()
        && !standalone
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
            throw SW_RUNTIME_ERROR("");

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
        if (!getPackage().ppath.empty())
            project_name = getPackage().ppath.back();
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

    autoDetectIncludeDirectories();
    autoDetectSources();
}

void NativeCompiledTarget::autoDetectSources()
{
    // gather things to check
    //bool sources_empty = gatherSourceFiles().empty();
    bool sources_empty = sizeKnown() == 0;

    if (!(sources_empty && !already_built))
        return;

    LOG_TRACE(logger, getPackage().toString() + ": Autodetecting sources");

    // all files except starting from point
    static const auto files_regex = "[^\\.].*";

    bool added = false;
    for (auto &d : include_dir_names)
    {
        if (fs::exists(SourceDir / d))
        {
            add(FileRegex(d, std::regex(files_regex), true));
            added = true;
            break; // break here!
        }
    }
    for (auto &d : source_dir_names)
    {
        if (fs::exists(SourceDir / d))
        {
            add(FileRegex(d, std::regex(files_regex), true));
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

    LOG_TRACE(logger, getPackage().toString() + ": Autodetecting include dirs");

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

DependenciesType NativeCompiledTarget::gatherDependencies() const
{
    DependenciesType deps;
    for (auto &d : ((NativeCompiledTarget*)this)->getActiveDependencies())
        deps.insert(d.dep);
    return deps;
}

NativeCompiledTarget::ActiveDeps &NativeCompiledTarget::getActiveDependencies()
{
    if (!active_deps)
    {
        ActiveDeps deps;
        TargetOptionsGroup::iterate([this, &deps](auto &v, auto i)
        {
            for (auto &d : v.getRawDependencies())
            {
                if (d->isDisabled())
                    continue;

                TargetDependency td;
                td.dep = d;
                td.inhtype = i;
                td.dep->settings = ts;
                deps.push_back(td);
            }
        });
        active_deps = deps;
    }
    return *active_deps;
}

const NativeCompiledTarget::ActiveDeps &NativeCompiledTarget::getActiveDependencies() const
{
    if (!active_deps)
        throw SW_RUNTIME_ERROR("no active deps calculated");
    return *active_deps;
}

void NativeCompiledTarget::merge1()
{
    // merge self
    merge();

    // merge deps' stuff
    for (auto &d : getAllDependencies())
    {
        GroupSettings s;
        s.include_directories_only = d->IncludeDirectoriesOnly;
        //s.merge_to_self = false;
        auto t = d->getTarget().as<const NativeCompiledTarget*>();
        if (t)
        {
            merge(*t, s);
            continue;
        }

        const auto &is = d->getTarget().getInterfaceSettings();
        if (is["system-include-directories"])
        {
            for (auto &v : is["system-include-directories"].getArray())
                NativeCompilerOptions::System.IncludeDirectories.push_back(v);
        }
        if (is["system-link-directories"])
        {
            for (auto &v : is["system-link-directories"].getArray())
                NativeLinkerOptions::System.LinkDirectories.push_back(v);
        }
    }
}

bool NativeCompiledTarget::prepare()
{
    if (skip || DryRun)
        return false;
    if (getSolution().skipTarget(Scope))
        return false;

    //DEBUG_BREAK_IF_STRING_HAS(getPackage().ppath.toString(), "GDCM.gdcm");

    switch (prepare_pass)
    {
    case 1:
    {
        LOG_TRACE(logger, "Preparing target: " + getPackage().ppath.toString());

        getSolution().call_event(*this, CallbackType::BeginPrepare);

        if (UseModules)
        {
            if (getCompilerType() != CompilerType::MSVC)
                throw SW_RUNTIME_ERROR("Currently modules are implemented for MSVC only");
            CPPVersion = CPPLanguageStandard::CPP2a;
        }

        findSources();

        if (!sw_provided)
        {
            // add pvt binary dir
            IncludeDirectories.insert(BinaryPrivateDir);

            // always add bdir to include dirs
            Public.IncludeDirectories.insert(BinaryDir);
        }

        resolvePostponedSourceFiles();
        HeaderOnly = !hasSourceFiles();

        if (PackageDefinitions)
            addPackageDefinitions(true);

        for (auto &[p, f] : *this)
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
                    throw std::logic_error("not implemented");
                }
            }
        }

        // default macros
        // public to make sure integrations also take these
        if (getSettings().TargetOS.Type == OSType::Windows)
        {
            Public.Definitions["SW_EXPORT"] = "__declspec(dllexport)";
            Public.Definitions["SW_IMPORT"] = "__declspec(dllimport)";
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
    RETURN_PREPARE_MULTIPASS_NEXT_PASS;
    case 2:
        // resolve
    {
        for (auto &d : getActiveDependencies())
        {
            auto t = getSolution().getChildren().find(d.dep->getPackage(), d.dep->settings);
            if (!t)
                throw SW_RUNTIME_ERROR("No such target: " + d.dep->getPackage().toString());
            d.dep->setTarget(*t);
        }
    }
    RETURN_PREPARE_MULTIPASS_NEXT_PASS;
    case 3:
        // inheritance
    {
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

        // we have ptrs, so do custom sorting
        std::unordered_map<DependencyPtr, InheritanceType, H, EQ> deps(0, H{});
        std::vector<DependencyPtr> deps_ordered;

        // set our initial deps
        for (auto &d : getActiveDependencies())
        {
            deps.emplace(d.dep, d.inhtype);
            deps_ordered.push_back(d.dep);
        }

        while (1)
        {
            bool new_dependency = false;
            auto deps2 = deps;
            for (auto &[d, _] : deps2)
            {
                // iterate over child deps
                auto t = d->getTarget().as<const NativeCompiledTarget*>();
                if (!t)
                    continue;
                for (auto &dep : t->getActiveDependencies())
                {
                    auto Inheritance = dep.inhtype;
                    auto d2 = dep.dep;

                    // nothing to do with private inheritance
                    // before d2->getTarget()!
                    if (Inheritance == InheritanceType::Private)
                        continue;

                    if (&d2->getTarget() == this)
                        continue;

                    if (Inheritance == InheritanceType::Protected && !hasSameProject(d2->getTarget()))
                        continue;

                    auto copy = std::make_shared<Dependency>(*d2);
                    auto[i, inserted] = deps.emplace(copy,
                        Inheritance == InheritanceType::Interface ?
                        InheritanceType::Public : Inheritance
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
            }

            if (!new_dependency)
            {
                for (auto &d : deps_ordered)
                {
                    if (&d->getTarget() != this)
                        all_deps.insert(deps.find(d)->first);
                }
                break;
            }
        }

        // Here we check if some deps are not included in solution target set (children).
        // They could be in dummy children, because of different target scope, not listed on software network,
        // but still in use.
        // We add them back to children.
        // Example: helpers, small tools, code generators.
        // TODO: maybe reconsider
        {
            // not implemented
            /*auto &c = getSolution().children;
            auto &dc = getSolution().dummy_children;
            for (auto &d2 : Dependencies)
            {
                // only for tools?
                if (d2->target &&
                    //d2->target->Scope != TargetScope::Build &&
                    d2->target->Scope == TargetScope::Tool &&
                    c.find(d2->target->getPackage()) == c.end(d2->target->getPackage()) &&
                    dc.find(d2->target->getPackage()) != dc.end(d2->target->getPackage()))
                {
                    c[d2->target->getPackage()] = dc[d2->target->getPackage()];

                    // such packages are not completely independent
                    // they share same source dir (but not binary?) with parent etc.
                    d2->target->setSourceDir(SourceDir);
                }
            }*/
        }
    }
    RETURN_PREPARE_MULTIPASS_NEXT_PASS;
    case 4:
        // merge
    {
        merge1();
    }
    RETURN_PREPARE_MULTIPASS_NEXT_PASS;
    case 5:
        // source files
    {
        // check postponed files first
        for (auto &[p, f] : *this)
        {
            if (!f->postponed || f->skip)
                continue;

            auto ext = p.extension().string();
            auto prog = findProgramByExtension(ext);
            if (!prog)
                throw std::logic_error("User defined program not registered");

            auto p2 = dynamic_cast<FileToFileTransformProgram*>(prog);
            if (!p2)
                throw SW_RUNTIME_ERROR("Bad program type");
            f = this->SourceFileMapThis::operator[](p) = p2->createSourceFile(*this, p);
        }

        auto files = gatherSourceFiles();

        // copy headers to install dir
        if (!InstallDirectory.empty() && !fs::exists(SourceDir / InstallDirectory))
        {
            auto d = SourceDir / InstallDirectory;
            fs::create_directories(d);
            for (auto &[p, fp] : *this)
            {
                File f(p, getFs());
                if (f.isGenerated())
                    continue;
                // is_header_ext()
                const auto e = f.file.extension();
                if (getCppHeaderFileExtensions().find(e.string()) != getCppHeaderFileExtensions().end())
                    fs::copy_file(f.file, d / f.file.filename());
            }
        }

        // before merge
        if (getSettings().Native.ConfigurationType != ConfigurationType::Debug)
            *this += Definition("NDEBUG");
        // allow to other compilers?
        else if (getCompilerType() == CompilerType::MSVC)
            *this += Definition("_DEBUG");

        auto remove_bdirs = [this](auto *c)
        {
            // if we won't remove this, bdirs will differ between different config compilations
            // so our own config pch will be outdated on every call
            c->IncludeDirectories.erase(BinaryDir);
            c->IncludeDirectories.erase(BinaryPrivateDir);
        };

        auto vs_setup = [this, &remove_bdirs](auto *f, auto *c)
        {
            if (getSettings().Native.MT)
                c->RuntimeLibrary = vs::RuntimeLibraryType::MultiThreaded;

            switch (getSettings().Native.ConfigurationType)
            {
            case ConfigurationType::Debug:
                c->RuntimeLibrary =
                    getSettings().Native.MT ?
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

            if (IsConfig/* || c->PrecompiledHeader && c->PrecompiledHeader().create*/)
            {
                remove_bdirs(c);
            }

			// for static libs, we gather and put pdb near output file
            // btw, VS is clever enough to take this info from .lib
			/*if (getSelectedTool() == Librarian.get())
			{
				if ((getSettings().Native.ConfigurationType == ConfigurationType::Debug ||
					getSettings().Native.ConfigurationType == ConfigurationType::ReleaseWithDebugInformation) &&
					c->PDBFilename.empty())
				{
					auto f = getOutputFile();
					f = f.parent_path() / f.filename().stem();
					f += ".pdb";
					c->PDBFilename = f;// BinaryDir.parent_path() / "obj" / (getPackage().ppath.toString() + ".pdb");
				}
			}*/
        };

        auto gnu_setup = [this](auto *f, auto *c)
        {
            switch (getSettings().Native.ConfigurationType)
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

        // merge file compiler options with target compiler options
        for (auto &f : files)
        {
            // set everything before merge!
            f->compiler->merge(*this);

            if (auto c = f->compiler->as<VisualStudioCompiler*>())
            {
                if (UseModules)
                {
                    c->UseModules = UseModules;
                    //c->stdIfcDir = c->System.IncludeDirectories.begin()->parent_path() / "ifc" / (getSettings().TargetOS.Arch == ArchType::x86_64 ? "x64" : "x86");
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

                if (IsConfig/* || c->EmitPCH*/)
                {
                    remove_bdirs(c);
                }
            }
            else if (auto c = f->compiler->as<GNUCompiler*>())
            {
                gnu_setup(f, c);

                if (IsConfig/* || c->Language && c->Language() == "c++-header"s*/)
                {
                    remove_bdirs(c);
                }
            }
        }

        // also merge rc files
        for (auto &f : ::sw::gatherSourceFiles<RcToolSourceFile>(*this))
        {
            // add casual idirs?
            f->compiler->idirs = NativeCompilerOptions::System.IncludeDirectories;
        }

        //
        if (GenerateWindowsResource
            && !*HeaderOnly
            && ::sw::gatherSourceFiles<RcToolSourceFile>(*this).empty()
            && getSelectedTool() == Linker.get()
            && !IsConfig
            && getSettings().TargetOS.is(OSType::Windows)
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

            RcEmitter ctx(getPackage().version, getPackage().version);
            ctx.begin();

            ctx.beginBlock("StringFileInfo");
            ctx.beginBlock("040904b0");
            //VALUE "CompanyName", "TODO: <Company name>"
            ctx.addValueQuoted("FileDescription", { getPackage().ppath.back() + " - " + getConfig() });
            ctx.addValueQuoted("FileVersion", { getPackage().version.toString() });
            //VALUE "InternalName", "@PACKAGE@"
            ctx.addValueQuoted("LegalCopyright", { "Powered by Software Network" });
            ctx.addValueQuoted("OriginalFilename", { getPackage().toString() });
            ctx.addValueQuoted("ProductName", { getPackage().ppath.toString() });
            ctx.addValueQuoted("ProductVersion", { getPackage().version.toString() });
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
                if (auto c = f->compiler->as<VisualStudioCompiler*>())
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

        // pdb
        if (getSelectedTool())
        {
            if (auto c = getSelectedTool()->as<VisualStudioLinker *>())
            {
                if (!c->GenerateDebugInformation)
                {
                    if (getSettings().Native.ConfigurationType == ConfigurationType::Debug ||
                        getSettings().Native.ConfigurationType == ConfigurationType::ReleaseWithDebugInformation)
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
                    c->PDBFilename = f;// BinaryDir.parent_path() / "obj" / (getPackage().ppath.toString() + ".pdb");
                }
                else
                    c->PDBFilename.output_dependency = false;

                if (Linker->Type == LinkerType::LLD)
                {
                    if (c->GenerateDebugInformation)
                        c->InputFiles().insert("msvcrtd.lib");
                    else
                        c->InputFiles().insert("msvcrt.lib");
                }
            }
        }

        // export all symbols
        if (ExportAllSymbols && getSettings().TargetOS.Type == OSType::Windows && getSelectedTool() == Linker.get())
        {
            const path def = NATIVE_TARGET_DEF_SYMBOLS_FILE;
            Files objs;
            for (auto &f : files)
                objs.insert(f->output);
            SW_MAKE_EXECUTE_BUILTIN_COMMAND_AND_ADD(c, *this, "sw_create_def_file", nullptr);
            //c->record_inputs_mtime = true;
            c->arguments.push_back(def.u8string());
            c->push_back(objs);
            c->addInput(objs);
            c->addOutput(def);
            add(def);
        }

        // add def file to linker
        if (getSelectedTool() && getSelectedTool() == Linker.get())
        {
            if (auto VSL = getSelectedTool()->as<VisualStudioLibraryTool*>())
            {
                for (auto &[p, f] : *this)
                {
                    if (!f->skip && p.extension() == ".def")
                    {
                        VSL->ModuleDefinitionFile = p;
                        HeaderOnly = false;
                    }
                }
            }
        }

        // on macos we explicitly say that dylib should resolve symbols on dlopen
        if (IsConfig && getSolution().getHostOs().is(OSType::Macos) && getSelectedTool())
        {
            if (auto c = getSelectedTool()->as<GNULinker*>())
                c->Undefined = "dynamic_lookup";
        }

        // also fix rpath libname here
        if (createWindowsRpath())
        {
            getSelectedTool()->setImportLibrary(getOutputFileName2("lib") += ".rp");
        }
    }
    RETURN_PREPARE_MULTIPASS_NEXT_PASS;
    case 6:
        // link libraries
    {
        // add link libraries from deps
        if (!*HeaderOnly && getSelectedTool() != Librarian.get())
        {
            auto L = Linker->as<VisualStudioLinker*>();
            for (auto &d : getAllDependencies())
            {
                if (&d->getTarget() == this)
                    continue;
                if (d->IncludeDirectoriesOnly)
                    continue;

                auto nt = d->getTarget().template as<NativeCompiledTarget*>();
                if (!nt)
                    continue;

                // circular deps detection
                if (L)
                {
                    for (auto &d2 : nt->getAllDependencies())
                    {
                        if (&d2->getTarget() != this)
                            continue;
                        if (d2->IncludeDirectoriesOnly)
                            continue;

                        circular_dependency = true;
                        L->ImportLibrary.clear();
                        break;
                    }
                }

                if (!*nt->HeaderOnly)
                {
                    LinkLibraries.push_back(nt->getImportLibrary());
                }
            }
        }
    }
    RETURN_PREPARE_MULTIPASS_NEXT_PASS;
    case 7:
        // linker 1
    {
        // add more link libraries from deps
        if (!*HeaderOnly && getSelectedTool() != Librarian.get())
        {
            auto ll = [this](auto &l, bool system)
            {
                std::unordered_set<const NativeCompiledTarget*> targets;
                Files added;
                added.insert(l.begin(), l.end());
                gatherStaticLinkLibraries(l, added, targets, system);
            };

            ll(LinkLibraries, false);
            ll(NativeLinkerOptions::System.LinkLibraries, true);
        }

        // right after gatherStaticLinkLibraries()!
        if (getSelectedTool())
            getSelectedTool()->merge(*this);
    }
    RETURN_PREPARE_MULTIPASS_NEXT_PASS;
    case 8:
        // linker 2
    {
        // linker setup
        auto obj = gatherObjectFilesWithoutLibraries();
        auto O1 = gatherLinkLibraries();

        if (!*HeaderOnly && getSelectedTool() != Librarian.get())
        {
            for (auto &f : ::sw::gatherSourceFiles<RcToolSourceFile>(*this))
                obj.insert(f->output);
        }

        // circular and windows rpath processing
        processCircular(obj);

        if (getSelectedTool())
        {
            getSelectedTool()->setObjectFiles(obj);
            getSelectedTool()->setInputLibraryDependencies(O1);
        }

        getSolution().call_event(*this, CallbackType::EndPrepare);
    }
    RETURN_PREPARE_MULTIPASS_NEXT_PASS;
    case 9:
        clearGlobCache();
    SW_RETURN_MULTIPASS_END;
    }

    SW_RETURN_MULTIPASS_END;
}

void NativeCompiledTarget::processCircular(Files &obj)
{
    if (!hasCircularDependency() && !createWindowsRpath())
        return;
    if (*HeaderOnly || getSelectedTool() == Librarian.get())
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
        Strings dlls;
        SW_UNIMPLEMENTED;
        /*for (auto &d : Dependencies)
        {
            if (d->target == this)
                continue;
            if (d->isDisabledOrDummy())
                continue;
            if (d->IncludeDirectoriesOnly)
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
                    out = out.lexically_relative(getSolution().swctx.getLocalStorage().storage_dir);
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
        link_exe->ImportLibrary.clear();

        path out;
        {
            std::lock_guard lk(m);
            out = Linker->getOutputFile();
            Linker->setOutputFile(path(out) += ".1");
        }
        out += ".rp" + out.extension().u8string();

        SW_MAKE_EXECUTE_BUILTIN_COMMAND_AND_ADD(c, *this, "sw_replace_dll_import", nullptr);
        c->arguments.push_back(Linker->getOutputFile().u8string());
        c->arguments.push_back(out.u8string());
        c->addInput(Linker->getOutputFile());
        c->addOutput(out);
        auto cmd = Linker->createCommand(getSolution().swctx);
        cmd->dependent_commands.insert(c);
        c->push_back(dlls);
        cmds.insert(c);
        outputfile = out;
    }

    lib_exe->CreateImportLibrary = true;
    lib_exe->DllName = name;

    if (!link_exe->ModuleDefinitionFile)
    {
        Librarian->setObjectFiles(obj);
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
    Librarian->merge(*this);
    Librarian->prepareCommand(*this)->addOutput(exp);
    obj.insert(exp);
}

void NativeCompiledTarget::gatherStaticLinkLibraries(
    LinkLibrariesType &ll, Files &added, std::unordered_set<const NativeCompiledTarget*> &targets, bool system) const
{
    if (!targets.insert(this).second)
        return;
    for (auto &d : getAllDependencies())
    {
        if (d->IncludeDirectoriesOnly)
            continue;

        auto dt = d->getTarget().template as<const NativeCompiledTarget*>();
        if (!dt)
            continue;

        // here we must gather all static (and header only?) lib deps in recursive manner
        if (dt->getSelectedTool() == dt->Librarian.get() || *dt->HeaderOnly)
        {
            auto add = [&added, &ll](auto &dt, const path &base, bool system)
            {
                auto &a = system ? dt->NativeLinkerOptions::System.LinkLibraries : dt->LinkLibraries;
                if (added.find(base) == added.end() && !system)
                {
                    ll.push_back(base);
                    ll.insert(ll.end(), a.begin(), a.end()); // also link libs
                }
                else
                {
                    // we added output file but not its system libs
                    for (auto &l : a)
                    {
                        if (std::find(ll.begin(), ll.end(), l) == ll.end())
                            ll.push_back(l);
                    }
                }
            };

            if (!*dt->HeaderOnly)
                add(dt, dt->getOutputFile(), system);

            // if dep is a static library, we take all its deps link libraries too
            for (auto &d2 : dt->getAllDependencies())
            {
                if (&d2->getTarget() == this)
                    continue;
                if (&d2->getTarget() == &d->getTarget())
                    continue;
                if (d2->IncludeDirectoriesOnly)
                    continue;

                auto dt2 = d2->getTarget().template as<const NativeCompiledTarget*>();
                if (!dt2)
                    continue;
                if (!*dt2->HeaderOnly)
                    add(dt2, dt2->getImportLibrary(), system);
                dt2->gatherStaticLinkLibraries(ll, added, targets, system);
            }
        }
    }
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

            if (getSettings().TargetOS.Type == OSType::Windows)
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

            Definitions[api + "_EXTERN="];
            Interface.Definitions[api + "_EXTERN"] = "extern";
        };

        if (SwDefinitions)
        {
            if (Type == LibraryType::Shared)
            {
                //Definitions["CPPAN_SHARED_BUILD"];
                Definitions["SW_SHARED_BUILD"];
            }
            else if (Type == LibraryType::Static)
            {
                //Definitions["CPPAN_STATIC_BUILD"];
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
        // probably setting dll must affect .dll extension automatically
        Linker->Extension = getSettings().TargetOS.getSharedLibraryExtension();
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
        }
        if (getSettings().TargetOS.Type == OSType::Windows)
            Definitions["_WINDLL"];
    }
    else
    {
        SelectedTool = Librarian.get();
        Librarian->Extension = getSettings().TargetOS.getStaticLibraryExtension();
    }
}

void NativeCompiledTarget::removeFile(const path &fn, bool binary_dir)
{
    remove_full(fn);
    Target::removeFile(fn, binary_dir);
}

void NativeCompiledTarget::configureFile(path from, path to, ConfigureFlags flags)
{
    // add to target if not already added
    if (DryRun)
        operator-=(from);
    else
    {
        auto fr = from;
        check_absolute(fr);
        if (find(fr) == end())
            operator-=(from);
    }

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
    static const std::regex cmDefineRegex(R"xxx(#cmakedefine[ \t]+([A-Za-z_0-9]*)([^\r\n]*?)[\r\n])xxx");
    static const std::regex cmDefine01Regex(R"xxx(#cmakedefine01[ \t]+([A-Za-z_0-9]*)[^\r\n]*?[\r\n])xxx");
    static const std::regex mesonDefine(R"xxx(#mesondefine[ \t]+([A-Za-z_0-9]*)[^\r\n]*?[\r\n])xxx");
    static const std::regex undefDefine(R"xxx(#undef[ \t]+([A-Za-z_0-9]*)[^\r\n]*?[\r\n])xxx");
    static const std::regex cmAtVarRegex("@([A-Za-z_0-9/.+-]+)@");
    static const std::regex cmNamedCurly("\\$\\{([A-Za-z0-9/_.+-]+)\\}");

    static const StringSet offValues{
        "", "0", //"OFF", "NO", "FALSE", "N", "IGNORE",
    };

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
            LOG_TRACE(logger, "configure @@ or ${} " << m[1].str() << ": replacement not found");
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
            LOG_TRACE(logger, "configure #mesondefine " << m[1].str() << ": replacement not found");
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
                s = m.prefix().str() + m.suffix().str();
                LOG_TRACE(logger, "configure #undef " << m[1].str() << ": replacement not found");
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
            LOG_TRACE(logger, "configure #cmakedefine " << m[1].str() << ": replacement not found");
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
            LOG_TRACE(logger, "configure #cmakedefine01 " << m[1].str() << ": replacement not found");
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
    auto i0 = getSolution().checker.sets.find(getSolution().getCurrentGroupNumber());
    if (i0 == getSolution().checker.sets.end())
        throw SW_RUNTIME_ERROR("No such group number: " + std::to_string(getSolution().getCurrentGroupNumber()));
    auto i = i0->second.find(name);
    if (i == i0->second.end())
        throw SW_RUNTIME_ERROR("No such set: " + name);
    return i->second;
}

void NativeCompiledTarget::setChecks(const String &name, bool check_definitions)
{
    auto &checks_set = getChecks(name);
    checks_set.t = this;
    checks_set.performChecks(getConfig());

    // set results
    for (auto &[k, c] : checks_set.check_values)
    {
        auto d = c->getDefinition(k);
        const auto v = c->Value.value();
        // make private?
        // remove completely?
        if (check_definitions && d)
        {
            add(Definition{ d.value() });
        }
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
        base = getSolution().BinaryDir;
    return base / "patch";
}

void NativeCompiledTarget::writeFileOnce(const path &fn, const String &content) const
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

    //File f(p, getFs());
    //f.getFileRecord().load();
}

void NativeCompiledTarget::writeFileSafe(const path &fn, const String &content) const
{
    if (DryRun)
        return;

    bool source_dir = false;
    path p = fn;
    if (!check_absolute(p, true, &source_dir))
        p = BinaryDir / p;
    ::sw::writeFileSafe(p, content, getPatchDir(!source_dir));

    //File f(fn, getFs());
    //f.getFileRecord().load();
}

void NativeCompiledTarget::replaceInFileOnce(const path &fn, const String &from, const String &to) const
{
    patch(fn, from, to);
}

void NativeCompiledTarget::patch(const path &fn, const String &from, const String &to) const
{
    if (DryRun)
        return;

    bool source_dir = false;
    path p = fn;
    check_absolute(p, false, &source_dir);
    ::sw::replaceInFileOnce(p, from, to, getPatchDir(!source_dir));

    //File f(p, getFs());
    //f.getFileRecord().load();
}

void NativeCompiledTarget::patch(const path &fn, const String &patch_str) const
{
    if (DryRun)
        return;

    bool source_dir = false;
    path p = fn;
    check_absolute(p, false, &source_dir);
    ::sw::patch(p, patch_str, getPatchDir(!source_dir));
}

void NativeCompiledTarget::deleteInFileOnce(const path &fn, const String &from) const
{
    replaceInFileOnce(fn, from, "");
}

void NativeCompiledTarget::pushFrontToFileOnce(const path &fn, const String &text) const
{
    if (DryRun)
        return;

    bool source_dir = false;
    path p = fn;
    check_absolute(p, false, &source_dir);
    ::sw::pushFrontToFileOnce(p, text, getPatchDir(!source_dir));

    //File f(p, getFs());
    //f.getFileRecord().load();
}

void NativeCompiledTarget::pushBackToFileOnce(const path &fn, const String &text) const
{
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

static std::unique_ptr<Source> load_source_and_version(const yaml &root, Version &version)
{
    String ver;
    YAML_EXTRACT_VAR(root, ver, "version", String);
    if (!ver.empty())
        version = Version(ver);
    if (root["source"].IsDefined())
        return Source::load(root["source"]);
    return nullptr;
}

void NativeCompiledTarget::cppan_load_project(const yaml &root)
{
    *this += load_source_and_version(root, getPackageMutable().version);

    YAML_EXTRACT_AUTO2(Empty, "empty");
    YAML_EXTRACT_VAR(root, HeaderOnly, "header_only", bool);

    YAML_EXTRACT_AUTO2(ImportFromBazel, "import_from_bazel");
    YAML_EXTRACT_AUTO2(BazelTargetName, "bazel_target_name");
    YAML_EXTRACT_AUTO2(BazelTargetFunction, "bazel_target_function");

    YAML_EXTRACT_AUTO2(ExportAllSymbols, "export_all_symbols");
    YAML_EXTRACT_AUTO2(ExportIfStatic, "export_if_static");

    ApiNames = get_sequence_set<String>(root, "api_name");

    auto read_dir = [&root](auto &p, const String &s)
    {
        get_scalar_f(root, s, [&p, &s](const auto &n)
        {
            auto cp = current_thread_path();
            p = n.template as<String>();
            if (!is_under_root(cp / p, cp))
                throw std::runtime_error("'" + s + "' must not point outside the current dir: " + p.string() + ", " + cp.string());
        });
    };

    read_dir(RootDirectory, "root_directory");
    if (RootDirectory.empty())
        read_dir(RootDirectory, "root_dir");

    // sources
    {
        auto read_sources = [&root](auto &a, const String &key, bool required = true)
        {
            a.clear();
            auto files = root[key];
            if (!files.IsDefined())
                return;
            if (files.IsScalar())
            {
                a.insert(files.as<String>());
            }
            else if (files.IsSequence())
            {
                for (const auto &v : files)
                    a.insert(v.as<String>());
            }
            else if (files.IsMap())
            {
                for (const auto &group : files)
                {
                    if (group.second.IsScalar())
                        a.insert(group.second.as<String>());
                    else if (group.second.IsSequence())
                    {
                        for (const auto &v : group.second)
                            a.insert(v.as<String>());
                    }
                    else if (group.second.IsMap())
                    {
                        String root = get_scalar<String>(group.second, "root");
                        auto v = get_sequence<String>(group.second, "files");
                        for (auto &e : v)
                            a.insert(root + "/" + e);
                    }
                }
            }
        };

        StringSet sources;
        read_sources(sources, "files");
        for (auto &s : sources)
            operator+=(FileRegex(SourceDir, std::regex(s), true));

        StringSet exclude_from_build;
        read_sources(exclude_from_build, "exclude_from_build");
        for (auto &s : exclude_from_build)
            operator-=(FileRegex(SourceDir, std::regex(s), true));

        StringSet exclude_from_package;
        read_sources(exclude_from_package, "exclude_from_package");
        for (auto &s : exclude_from_package)
            operator^=(FileRegex(SourceDir, std::regex(s), true));
    }

    // include_directories
    {
        get_variety(root, "include_directories",
            [this](const auto &d)
        {
            Public.IncludeDirectories.insert(d.template as<String>());
        },
            [this](const auto &dall)
        {
            for (auto d : dall)
                Public.IncludeDirectories.insert(d.template as<String>());
        },
            [this, &root](const auto &)
        {
            get_map_and_iterate(root, "include_directories", [this](const auto &n)
            {
                auto f = n.first.template as<String>();
                auto s = get_sequence<String>(n.second);
                if (f == "public")
                    Public.IncludeDirectories.insert(s.begin(), s.end());
                else if (f == "private")
                    Private.IncludeDirectories.insert(s.begin(), s.end());
                else if (f == "interface")
                    Interface.IncludeDirectories.insert(s.begin(), s.end());
                else if (f == "protected")
                    Protected.IncludeDirectories.insert(s.begin(), s.end());
                else
                    throw std::runtime_error("include key must be only 'public' or 'private' or 'interface'");
            });
        });
    }

    // deps
    {
        auto read_version = [](auto &dependency, const String &v)
        {
            // some code was removed here
            // check out original version (v1) if you encounter some errors

            //auto nppath = dependency.ppath / v;
            //dependency.ppath = nppath;

            dependency.range = v;
        };

        auto relative_name_to_absolute = [](const String &in)
        {
            // TODO
            PackagePath p(in);
            //if (p.isAbsolute())
            return p;
            //throw SW_RUNTIME_ERROR("not implemented");
            //return in;
        };

        auto read_single_dep = [this, &read_version, &relative_name_to_absolute](const auto &d, UnresolvedPackage dependency = {})
        {
            bool local_ok = false;
            if (d.IsScalar())
            {
                auto p = extractFromString(d.template as<String>());
                dependency.ppath = relative_name_to_absolute(p.ppath.toString());
                dependency.range = p.range;
            }
            else if (d.IsMap())
            {
                // read only field related to ppath - name, local
                if (d["name"].IsDefined())
                    dependency.ppath = relative_name_to_absolute(d["name"].template as<String>());
                if (d["package"].IsDefined())
                    dependency.ppath = relative_name_to_absolute(d["package"].template as<String>());
                if (dependency.ppath.empty() && d.size() == 1)
                {
                    dependency.ppath = relative_name_to_absolute(d.begin()->first.template as<String>());
                    //if (dependency.ppath.is_loc())
                        //dependency.flags.set(pfLocalProject);
                    read_version(dependency, d.begin()->second.template as<String>());
                }
                if (d["local"].IsDefined()/* && allow_local_dependencies*/)
                {
                    auto p = d["local"].template as<String>();
                    UnresolvedPackage pkg;
                    pkg.ppath = p;
                    //if (rd.known_local_packages.find(pkg) != rd.known_local_packages.end())
                        //local_ok = true;
                    if (local_ok)
                        dependency.ppath = p;
                }
            }

            if (dependency.ppath.is_loc())
            {
                //dependency.flags.set(pfLocalProject);

                // version will be read for local project
                // even 2nd arg is not valid
                String v;
                if (d.IsMap() && d["version"].IsDefined())
                    v = d["version"].template as<String>();
                read_version(dependency, v);
            }

            if (d.IsMap())
            {
                // read other map fields
                if (d["version"].IsDefined())
                {
                    read_version(dependency, d["version"].template as<String>());
                    if (local_ok)
                        dependency.range = "*";
                }
                //if (d["ref"].IsDefined())
                    //dependency.reference = d["ref"].template as<String>();
                //if (d["reference"].IsDefined())
                    //dependency.reference = d["reference"].template as<String>();
                //if (d["include_directories_only"].IsDefined())
                    //dependency.flags.set(pfIncludeDirectoriesOnly, d["include_directories_only"].template as<bool>());

                // conditions
                //dependency.conditions = get_sequence_set<String>(d, "condition");
                //auto conds = get_sequence_set<String>(d, "conditions");
                //dependency.conditions.insert(conds.begin(), conds.end());
            }

            //if (dependency.flags[pfLocalProject])
                //dependency.createNames();

            return dependency;
        };

        auto get_deps = [&](const auto &node)
        {
            get_variety(root, node,
                [this, &read_single_dep](const auto &d)
            {
                auto dep = read_single_dep(d);
                Public += dep;
                //throw SW_RUNTIME_ERROR("not implemented");
                //dependencies[dep.ppath.toString()] = dep;
            },
                [this, &read_single_dep](const auto &dall)
            {
                for (auto d : dall)
                {
                    auto dep = read_single_dep(d);
                    Public += dep;
                    //throw SW_RUNTIME_ERROR("not implemented");
                    //dependencies[dep.ppath.toString()] = dep;
                }
            },
                [this, &read_single_dep, &read_version, &relative_name_to_absolute](const auto &dall)
            {
                auto get_dep = [this, &read_version, &read_single_dep, &relative_name_to_absolute](const auto &d)
                {
                    UnresolvedPackage dependency;

                    dependency.ppath = relative_name_to_absolute(d.first.template as<String>());
                    //if (dependency.ppath.is_loc())
                        //dependency.flags.set(pfLocalProject);

                    if (d.second.IsScalar())
                        read_version(dependency, d.second.template as<String>());
                    else if (d.second.IsMap())
                        return read_single_dep(d.second, dependency);
                    else
                        throw std::runtime_error("Dependency should be a scalar or a map");

                    //if (dependency.flags[pfLocalProject])
                        //dependency.createNames();

                    return dependency;
                };

                auto extract_deps = [&get_dep, &read_single_dep](const auto &dall, const auto &str)
                {
                    UnresolvedPackages deps;
                    auto priv = dall[str];
                    if (!priv.IsDefined())
                        return deps;
                    if (priv.IsMap())
                    {
                        get_map_and_iterate(dall, str,
                            [&get_dep, &deps](const auto &d)
                        {
                            auto dep = get_dep(d);
                            deps.insert(dep);
                            //throw SW_RUNTIME_ERROR("not implemented");
                            //deps[dep.ppath.toString()] = dep;
                        });
                    }
                    else if (priv.IsSequence())
                    {
                        for (auto d : priv)
                        {
                            auto dep = read_single_dep(d);
                            deps.insert(dep);
                            //throw SW_RUNTIME_ERROR("not implemented");
                            //deps[dep.ppath.toString()] = dep;
                        }
                    }
                    return deps;
                };

                auto extract_deps_from_node = [this, &extract_deps, &get_dep](const auto &node)
                {
                    auto deps_private = extract_deps(node, "private");
                    auto deps = extract_deps(node, "public");

                    operator+=(deps_private);
                    for (auto &d : deps_private)
                    {
                        //operator+=(d);
                        //throw SW_RUNTIME_ERROR("not implemented");
                        //d.second.flags.set(pfPrivateDependency);
                        //deps.insert(d);
                    }

                    Public += deps;
                    for (auto &d : deps)
                    {
                        //Public += d;
                        //throw SW_RUNTIME_ERROR("not implemented");
                        //d.second.flags.set(pfPrivateDependency);
                        //deps.insert(d);
                    }

                    if (deps.empty() && deps_private.empty())
                    {
                        for (auto d : node)
                        {
                            auto dep = get_dep(d);
                            Public += dep;
                            //throw SW_RUNTIME_ERROR("not implemented");
                            //deps[dep.ppath.toString()] = dep;
                        }
                    }

                    return deps;
                };

                auto ed = extract_deps_from_node(dall);
                //throw SW_RUNTIME_ERROR("not implemented");
                //dependencies.insert(ed.begin(), ed.end());

                // conditional deps
                /*for (auto n : dall)
                {
                    auto spec = n.first.as<String>();
                    if (spec == "private" || spec == "public")
                        continue;
                    if (n.second.IsSequence())
                    {
                        for (auto d : n.second)
                        {
                            auto dep = read_single_dep(d);
                            dep.condition = spec;
                            dependencies[dep.ppath.toString()] = dep;
                        }
                    }
                    else if (n.second.IsMap())
                    {
                        ed = extract_deps_from_node(n.second, spec);
                        dependencies.insert(ed.begin(), ed.end());
                    }
                }

                if (deps.empty() && deps_private.empty())
                {
                    for (auto d : node)
                    {
                        auto dep = get_dep(d);
                        deps[dep.ppath.toString()] = dep;
                    }
                }*/
            });
        };

        get_deps("dependencies");
        get_deps("deps");
    }

    // standards
    {
        int c_standard = 89;
        bool c_extensions = false;
        YAML_EXTRACT_AUTO(c_standard);
        if (c_standard == 0)
        {
            YAML_EXTRACT_VAR(root, c_standard, "c", int);
        }
        YAML_EXTRACT_AUTO(c_extensions);

        int cxx_standard = 14;
        bool cxx_extensions = false;
        String cxx;
        YAML_EXTRACT_VAR(root, cxx, "cxx_standard", String);
        if (cxx.empty())
            YAML_EXTRACT_VAR(root, cxx, "c++", String);
        YAML_EXTRACT_AUTO(cxx_extensions);

        if (!cxx.empty())
        {
            try
            {
                cxx_standard = std::stoi(cxx);
            }
            catch (const std::exception&)
            {
                if (cxx == "1z")
                    cxx_standard = 17;
                else if (cxx == "2x")
                    cxx_standard = 20;
            }
        }

        switch (cxx_standard)
        {
        case 98:
            CPPVersion = CPPLanguageStandard::CPP98;
            break;
        case 11:
            CPPVersion = CPPLanguageStandard::CPP11;
            break;
        case 14:
            CPPVersion = CPPLanguageStandard::CPP14;
            break;
        case 17:
            CPPVersion = CPPLanguageStandard::CPP17;
            break;
        case 20:
            CPPVersion = CPPLanguageStandard::CPP20;
            break;
        }
    }

    /*YAML_EXTRACT_AUTO(output_name);
    YAML_EXTRACT_AUTO(condition);
    YAML_EXTRACT_AUTO(include_script);
    license = get_scalar<String>(root, "license");

    read_dir(unpack_directory, "unpack_directory");
    if (unpack_directory.empty())
        read_dir(unpack_directory, "unpack_dir");

    YAML_EXTRACT_AUTO(output_directory);
    if (output_directory.empty())
        YAML_EXTRACT_VAR(root, output_directory, "output_dir", String);

    bs_insertions.load(root);*/
    auto options = cppan::loadOptionsMap(root);
    for (auto &[k, v] : options["any"].system_definitions["win32"])
        add(Definition(v));

    /*read_sources(public_headers, "public_headers");
    include_hints = get_sequence_set<String>(root, "include_hints");

    aliases = get_sequence_set<String>(root, "aliases");

    checks.load(root);
    checks_prefixes = get_sequence_set<String>(root, "checks_prefixes");
    if (checks_prefixes.empty())
        checks_prefixes = get_sequence_set<String>(root, "checks_prefix");

    const auto &patch_node = root["patch"];
    if (patch_node.IsDefined())
        patch.load(patch_node);*/
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
        Linker->Extension = getSettings().TargetOS.getExecutableExtension();

        if (getSelectedTool())
        {
            if (auto c = getSelectedTool()->as<VisualStudioLinker *>())
            {
                c->ImportLibrary.output_dependency = false; // become optional
                c->ImportLibrary.create_directory = true; // but create always
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
            if (getSettings().TargetOS.Type == OSType::Windows)
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

void ExecutableTarget::cppan_load_project(const yaml &root)
{
    /*String et;
    YAML_EXTRACT_VAR(root, et, "executable_type", String);
    if (et == "win32")
        executable_type = ExecutableType::Win32;*/

    NativeCompiledTarget::cppan_load_project(root);
}

bool LibraryTarget::prepare()
{
    return prepareLibrary(getSettings().Native.LibrariesType);
}

bool LibraryTarget::init()
{
    auto r = NativeCompiledTarget::init();
    initLibrary(getSettings().Native.LibrariesType);
    return r;
}

path LibraryTarget::getImportLibrary() const
{
    if (getSelectedTool() == Librarian.get())
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
