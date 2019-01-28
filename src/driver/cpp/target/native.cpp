#include "native.h"

#include "bazel/bazel.h"
#include <directories.h>
#include <functions.h>
#include <solution.h>
#include <suffix.h>

#include <boost/algorithm/string.hpp>
#include <nlohmann/json.hpp>
#include <primitives/constants.h>
#include <primitives/context.h>
#include <primitives/sw/settings.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "target.native");

#define NATIVE_TARGET_DEF_SYMBOLS_FILE \
    (BinaryDir / ".sw.symbols.def")

#define RETURN_PREPARE_PASS \
    do {prepare_pass++; return true;} while (0)

static cl::opt<bool> do_not_mangle_object_names("do-not-mangle-object-names");
//static cl::opt<bool> full_build("full", cl::desc("Full build (check all conditions)"));

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

namespace sw
{

DependencyPtr NativeTarget::getDependency() const
{
    auto d = std::make_shared<Dependency>(this);
    return d;
}

void NativeTarget::setOutputDir(const path &dir)
{
    //SwapAndRestore sr(OutputDir, dir);
    OutputDir = dir;
    setOutputFile();
}

NativeExecutedTarget::~NativeExecutedTarget()
{
    // incomplete type cannot be in default dtor
    // in our case it is nlohmann::json member
}

void NativeExecutedTarget::init()
{
    Target::init();

    // propagate this pointer to all
    TargetOptionsGroup::iterate<WithSourceFileStorage, WithoutNativeOptions>([this](auto &v, auto &gs)
    {
        v.target = this;
    });
    //LanguageStorage::target = this;

    Librarian = std::dynamic_pointer_cast<NativeLinker>(getSolution()->Settings.Native.Librarian->clone());
    Linker = std::dynamic_pointer_cast<NativeLinker>(getSolution()->Settings.Native.Linker->clone());

    addPackageDefinitions();

    // we set output file, but sometimes overridden call must set it later
    // (libraries etc.)
    // this one is used for executables
    setOutputFile();
}

/*void NativeExecutedTarget::init2()
{
    if (!Local)
    {
        // activate later?
        /*auto &sdb = getServiceDatabase();
        auto o = getOutputFile();
        auto f = sdb.getInstalledPackageFlags(pkg, getConfig());
        if (f[pfHeaderOnly] || fs::exists(o) && f[pfBuilt])
        {
            already_built = true;
        }
    }
}*/

driver::cpp::CommandBuilder NativeExecutedTarget::addCommand() const
{
    driver::cpp::CommandBuilder cb(*getSolution()->fs);
    cb.c->addPathDirectory(getOutputBaseDir() / getConfig());
    cb << *this;
    return cb;
}

void NativeExecutedTarget::addPackageDefinitions(bool defs)
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
        a["PACKAGE_STRING"] = q + pkg.toString() + q;
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
    if (defs)
    {
        set_pkg_info(Definitions, true); // false?
        PackageDefinitions = false;
    }
    else
        set_pkg_info(Variables, false); // false?
}

path NativeExecutedTarget::getOutputBaseDir() const
{
    if (getSolution()->Settings.TargetOS.Type == OSType::Windows)
        return getUserDirectories().storage_dir_bin;
    else
        return getUserDirectories().storage_dir_lib;
}

path NativeExecutedTarget::getOutputDir() const
{
    if (OutputDir.empty())
        return getOutputFile().parent_path();
    return getTargetsDir().parent_path() / OutputDir;
}

/*void NativeExecutedTarget::setOutputFilename(const path &fn)
{
    //OutputFilename = fn;
}*/

void NativeExecutedTarget::setOutputFile()
{
    /* || add a considiton so user could change nont build output dir*/
    if (Scope == TargetScope::Build)
    {
        if (getSelectedTool() == Librarian.get())
            getSelectedTool()->setOutputFile(getOutputFileName(getUserDirectories().storage_dir_lib));
        else
        {
            getSelectedTool()->setOutputFile(getOutputFileName(getOutputBaseDir()));
            getSelectedTool()->setImportLibrary(getOutputFileName(getUserDirectories().storage_dir_lib));
        }
    }
    else
    {
        auto base = BinaryDir.parent_path() / "out" / getOutputFileName();
        getSelectedTool()->setOutputFile(base);
        if (getSelectedTool() != Librarian.get())
            getSelectedTool()->setImportLibrary(base);
    }
}

path NativeExecutedTarget::makeOutputFile() const
{
    if (getSelectedTool() == Librarian.get())
        return getOutputFileName(getUserDirectories().storage_dir_lib);
    else
        return getOutputFileName(getOutputBaseDir());
}

path Target::getOutputFileName() const
{
    return pkg.toString();
}

path NativeExecutedTarget::getOutputFileName(const path &root) const
{
    path p;
    if (SW_IS_LOCAL_BINARY_DIR)
    {
        if (IsConfig)
            p = getSolution()->BinaryDir / "cfg" / pkg.ppath.toString() / getConfig() / "out" / getOutputFileName();
        else
            p = getTargetsDir().parent_path() / OutputDir / getOutputFileName();
    }
    else
    {
        if (IsConfig)
            p = pkg.getDir() / "out" / getConfig() / getOutputFileName();
        //p = BinaryDir / "out";
        else
            p = root / getConfig() / OutputDir / getOutputFileName();
    }
    return p;
}

path NativeExecutedTarget::getOutputFile() const
{
    return getSelectedTool()->getOutputFile();
}

path NativeExecutedTarget::getImportLibrary() const
{
    return getSelectedTool()->getImportLibrary();
}

NativeExecutedTarget::TargetsSet NativeExecutedTarget::gatherDependenciesTargets() const
{
    TargetsSet deps;
    for (auto &d : Dependencies)
    {
        if (d->target.lock().get() == this)
            continue;
        if (d->isDummy())
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

FilesOrdered NativeExecutedTarget::gatherLinkLibraries() const
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

        if (!getSolution()->Settings.TargetOS.is(OSType::Windows))
            libs.push_back("-l" + l.u8string());
    }
    return libs;
}

std::unordered_set<NativeSourceFile*> NativeExecutedTarget::gatherSourceFiles() const
{
    return ::sw::gatherSourceFiles<NativeSourceFile>(*this);
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

Files NativeExecutedTarget::gatherObjectFilesWithoutLibraries() const
{
    Files obj;
    for (auto &f : gatherSourceFiles())
    {
        if (f->output.file.extension() != ".gch")
            obj.insert(f->output.file);
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

bool NativeExecutedTarget::hasSourceFiles() const
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

void NativeExecutedTarget::resolvePostponedSourceFiles()
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
    throw SW_RUNTIME_ERROR("No tool selected");
}

void NativeExecutedTarget::addPrecompiledHeader(const path &h, const path &cpp)
{
    PrecompiledHeader pch;
    pch.header = h;
    pch.source = cpp;
    addPrecompiledHeader(pch);
}

void NativeExecutedTarget::addPrecompiledHeader(PrecompiledHeader &p)
{
    check_absolute(p.header);
    if (!p.source.empty())
        check_absolute(p.source);

    bool force_include_pch_header_to_pch_source = true;
    bool force_include_pch_header_to_target_source_files = p.force_include_pch;
    auto &pch = p.source;
    path pch_dir = BinaryDir.parent_path() / "pch";
    if (!pch.empty())
    {
        if (!fs::exists(pch))
            write_file_if_different(pch, "");
        pch_dir = pch.parent_path();
        force_include_pch_header_to_pch_source = false;
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
    pch_dir = getUserDirectories().storage_dir_tmp;
    gch_fn = getUserDirectories().storage_dir_tmp / "sw/driver/cpp/sw.h.gch";
#endif

    auto setup_use_vc = [&force_include_pch_header_to_target_source_files, &p, &pch_fn, &pdb_fn](auto &c)
    {
        if (force_include_pch_header_to_target_source_files)
            c->ForcedIncludeFiles().push_back(p.header);
        c->PrecompiledHeaderFilename() = pch_fn;
        c->PrecompiledHeaderFilename.input_dependency = true;
        c->PrecompiledHeader().use = p.header;
        c->PDBFilename = pdb_fn;
        c->PDBFilename.intermediate_file = false;
    };

    // before adding pch source file to target
    // on this step we setup compilers to USE our created pch
    // MSVC does it explicitly, gnu does implicitly; check what about clang
    CompilerType cc = CompilerType::UnspecifiedCompiler;
    for (auto &f : gatherSourceFiles())
    {
        if (auto sf = f->as<NativeSourceFile>())
        {
            if (auto c = sf->compiler->as<VisualStudioCompiler>())
            {
                cc = c->Type;
                setup_use_vc(c);
            }
            else if (auto c = sf->compiler->as<ClangClCompiler>())
            {
                cc = c->Type;
                setup_use_vc(c);
            }
            else if (auto c = sf->compiler->as<ClangCompiler>())
            {
                cc = c->Type;
                break_gch_deps[pch] = gch_fn_clang;

                // !
                // add generated file, so it will be executed before
                File(gch_fn_clang, *getSolution()->fs).getFileRecord().setGenerated(true);
                *this += gch_fn_clang;

                if (force_include_pch_header_to_target_source_files)
                    c->ForcedIncludeFiles().push_back(p.header);

                c->PrecompiledHeader = gch_fn_clang;

                //c->PrecompiledHeaderFilename() = pch_fn;
                //c->PrecompiledHeaderFilename.input_dependency = true;
                //c->PrecompiledHeader().use = p.header;
            }
            else if (auto c = sf->compiler->as<GNUCompiler>())
            {
                cc = c->Type;
                break_gch_deps[pch] = gch_fn;

                // !
                // add generated file, so it will be executed before
                File(gch_fn, *getSolution()->fs).getFileRecord().setGenerated(true);
                *this += gch_fn;

                if (force_include_pch_header_to_target_source_files)
                    c->ForcedIncludeFiles().push_back(p.header);

                //c->PrecompiledHeaderFilename() = pch_fn;
                //c->PrecompiledHeaderFilename.input_dependency = true;
                //c->PrecompiledHeader().use = p.header;
            }
        }
    }

    // on this step we setup compilers to CREATE our pch
    if (!p.created)
    {
        *this += pch;
        if (auto sf = ((*this)[pch]).as<NativeSourceFile>(); sf)
        {
            auto setup_create_vc = [&sf, &force_include_pch_header_to_pch_source, &p, &pch_fn, &pdb_fn, &obj_fn](auto &c)
            {
                sf->setOutputFile(obj_fn);

                if (force_include_pch_header_to_pch_source)
                    c->ForcedIncludeFiles().push_back(p.header);
                c->PrecompiledHeaderFilename() = pch_fn;
                c->PrecompiledHeaderFilename.output_dependency = true;
                c->PrecompiledHeader().create = p.header;
                c->PDBFilename = pdb_fn;
                c->PDBFilename.intermediate_file = false;
                //c->PDBFilename.output_dependency = true;
            };

            if (auto c = sf->compiler->as<VisualStudioCompiler>())
            {
                setup_create_vc(c);
            }
            else if (auto c = sf->compiler->as<ClangClCompiler>())
            {
                setup_create_vc(c);
            }
            else if (auto c = sf->compiler->as<ClangCompiler>())
            {
                sf->setOutputFile(gch_fn_clang);
                c->Language = "c++-header";
                if (force_include_pch_header_to_pch_source)
                    c->ForcedIncludeFiles().push_back(p.header);
                c->EmitPCH = true;
            }
            else if (auto c = sf->compiler->as<GNUCompiler>())
            {
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

NativeExecutedTarget &NativeExecutedTarget::operator=(PrecompiledHeader &pch)
{
    addPrecompiledHeader(pch);
    return *this;
}

std::shared_ptr<builder::Command> NativeExecutedTarget::getCommand() const
{
    if (HeaderOnly && HeaderOnly.value())
        return nullptr;
    return getSelectedTool()->getCommand(*this);
}

Commands NativeExecutedTarget::getGeneratedCommands() const
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
        File p(f, *getSolution()->fs);
        if (!p.isGenerated())
            continue;
        if (f == def)
            continue;
        auto c = p.getFileRecord().getGenerator();
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
        File p(f, *getSolution()->fs);
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

Commands NativeExecutedTarget::getCommands() const
{
    if (getSolution()->skipTarget(Scope))
        return {};

    if (already_built)
        return {};

    const path def = NATIVE_TARGET_DEF_SYMBOLS_FILE;

    //DEBUG_BREAK_IF_STRING_HAS(pkg.toString(), "version-master");

    // add generated files
    auto generated = getGeneratedCommands();

    // also from deps???
    // remove?
    // who is responsible for this? users? program?
    /*for (auto &d : Dependencies)
    {
        if (d->target == this)
            continue;
        if (d->isDummy())
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

    Commands cmds;
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

        auto prepare_command = [this, &cmds, &sd, &bd, &bdp](auto f, auto c)
        {
            c->args.insert(c->args.end(), f->args.begin(), f->args.end());

            // set fancy name
            if (/*!Local && */!IsConfig && !do_not_mangle_object_names)
            {
                auto p = normalize_path(f->file);
                if (bdp.size() < p.size() && p.find(bdp) == 0)
                {
                    auto n = p.substr(bdp.size());
                    c->name = "[" + pkg.toString() + "]/[bdir_pvt]" + n;
                }
                else if (bd.size() < p.size() && p.find(bd) == 0)
                {
                    auto n = p.substr(bd.size());
                    c->name = "[" + pkg.toString() + "]/[bdir]" + n;
                }
                if (sd.size() < p.size() && p.find(sd) == 0)
                {
                    String prefix;
                    /*if (f->compiler == getSolution()->Settings.Native.CCompiler)
                        prefix = "Building C object ";
                    else if (f->compiler == getSolution()->Settings.Native.CPPCompiler)
                        prefix = "Building CXX object ";*/
                    auto n = p.substr(sd.size());
                    if (!n.empty() && n[0] != '/')
                        n = "/" + n;
                    c->name = prefix + "[" + pkg.toString() + "]" + n;
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

    //LOG_DEBUG(logger, "Building target: " + pkg.ppath.toString());
    //move this somewhere

    //DEBUG_BREAK_IF_STRING_HAS(pkg.ppath.toString(), "self_builder");

    // add install commands
    for (auto &[p, f] : *this)
    {
        if (f->install_dir.empty())
            continue;

        auto o = getOutputDir();
        o /= f->install_dir / p.filename();

        SW_MAKE_EXECUTE_BUILTIN_COMMAND(copy_cmd, *this, "sw_copy_file");
        copy_cmd->args.push_back(p.u8string());
        copy_cmd->args.push_back(o.u8string());
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
                if (d->isDummy())
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
            if (isLocal() && getSolution()->Settings.Native.CopySharedLibraries &&
                Scope == TargetScope::Build && OutputDir.empty())
            {
                for (auto &l : gatherAllRelatedDependencies())
                {
                    auto dt = ((NativeExecutedTarget*)l);
                    if (dt->isLocal())
                        continue;
                    if (dt->HeaderOnly.value())
                        continue;
                    if (getSolution()->Settings.Native.LibrariesType != LibraryType::Shared && !dt->isSharedOnly())
                        continue;
                    if (dt->getSelectedTool() == dt->Librarian.get())
                        continue;
                    auto in = dt->getOutputFile();
                    auto o = getOutputDir() / dt->OutputDir;
                    //if (OutputFilename.empty())
                    o /= in.filename();
                    //else
                    {
                        //o /= OutputFilename;
                        //if (add_d_on_debug && getSolution()->Settings.Native.ConfigurationType == ConfigurationType::Debug)
                            //o += "d";
                        //o += in.extension().u8string();
                    }
                    if (in == o)
                        continue;
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
                auto non_circ_cmd = dt->getSelectedTool()->getCommand(*this);

                // one command must be executed after the second to free implib files from any compiler locks
                // we choose it based on ptr address
                //if (c < non_circ_cmd)
                c->dependencies.erase(non_circ_cmd);

                if (dt->CircularLinker)
                {
                    auto cd = dt->CircularLinker->getCommand(*this);
                    c->dependencies.insert(cd);
                }
                //cmds.insert(cd);
            }

            if (CircularLinker)
            {
                // execute this command after unresolved (circular) cmd
                c->dependencies.insert(CircularLinker->getCommand(*this));

                // we reset generator of implib from usual build command (c = getCommand()) to circular linker generator to overcome
                // automatic circular dependency generation in command.cpp
                //File(getImportLibrary()).getFileRecord().generator = CircularLinker->getCommand();
            }
        }

        cmds.insert(c);

        // set fancy name
        if (/*!Local && */!IsConfig && !do_not_mangle_object_names)
            c->name = "[" + pkg.toString() + "]" + getSelectedTool()->Extension;

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

    /*if (auto evs = Events.getCommands(); !evs.empty())
    {
        for (auto &c : cmds)
            c->dependencies.insert(evs.begin(), evs.end());
        cmds.insert(evs.begin(), evs.end());
    }*/

    /*if (!IsConfig && !Local)
    {
        if (!File(getOutputFile(), *getSolution()->fs).isChanged())
            return {};
    }*/

    return cmds;
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

static const Strings source_dir_names = { "src", "source", "sources", "lib", "library" };

void NativeExecutedTarget::autoDetectOptions()
{
    // TODO: add dirs with first capital letter:
    // Include, Source etc.

    autodetect = true;

    autoDetectIncludeDirectories();
    autoDetectSources();
}

void NativeExecutedTarget::autoDetectSources()
{
    // gather things to check
    //bool sources_empty = gatherSourceFiles().empty();
    bool sources_empty = sizeKnown() == 0;

    // files
    if (sources_empty && !already_built)
    {
        LOG_TRACE(logger, getPackage().toString() + ": Autodetecting sources");

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
        path f = "sw.cpp";
        check_absolute(f, true);
        operator^=(f);
    }
}

void NativeExecutedTarget::autoDetectIncludeDirectories()
{
    bool idirs_empty = true;

    // idirs
    if (idirs_empty)
    {
        LOG_TRACE(logger, getPackage().toString() + ": Autodetecting include dirs");

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

    if (!Local)
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

path NativeExecutedTarget::getPrecomputedDataFilename()
{
    // binary dir!
    return BinaryDir.parent_path() / "info" / "precomputed.5.json";
}

void NativeExecutedTarget::tryLoadPrecomputedData()
{
    if (isLocalOrOverridden())
        return;

    auto fn = getPrecomputedDataFilename();
    if (!fs::exists(fn))
        return;

    // TODO: detect frontend
    if (!File(pkg.getDirSrc2() / "sw.cpp", *getSolution()->fs).isChanged())
        return;

    //precomputed_data = nlohmann::json::parse(read_file(fn));
}

void NativeExecutedTarget::applyPrecomputedData()
{
}

void NativeExecutedTarget::savePrecomputedData()
{
    if (isLocalOrOverridden())
        return;

    nlohmann::json j;

    for (int i = toIndex(InheritanceType::Min); i < toIndex(InheritanceType::Max); i++)
    {
        auto s = getInheritanceStorage().raw()[i];
        if (!s)
            continue;
        auto si = std::to_string(i);
        for (auto &[p, _] : *s)
        {
            j[si]["source_files"].push_back(normalize_path(p)); // add flags: skip, postpone(?) etc.
        }
        for (auto &d : s->Dependencies)
        {
            auto &jd = j[si]["dependencies"][d->getResolvedPackage().toString()];
            jd["idir"] = d->IncludeDirectoriesOnly;
            jd["dummy"] = d->Dummy;
        }
    }

    write_file(getPrecomputedDataFilename(), j.dump(2));
}

bool NativeExecutedTarget::prepare()
{
    if (getSolution()->skipTarget(Scope))
        return false;

    //DEBUG_BREAK_IF_STRING_HAS(pkg.ppath.toString(), "aspia.codec");

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
        //if (!IsConfig/* && PackageDefinitions*/)

        //restoreSourceDir();
        RETURN_PREPARE_PASS;
    case 1:
    {
        LOG_TRACE(logger, "Preparing target: " + pkg.ppath.toString());

        //tryLoadPrecomputedData();

        getSolution()->call_event(*this, CallbackType::BeginPrepare);

        if (UseModules)
        {
            if (getSolution()->Settings.Native.CompilerType != CompilerType::MSVC)
                throw SW_RUNTIME_ERROR("Currently modules are implemented for MSVC only");
            CPPVersion = CPPLanguageStandard::CPP2a;
        }

        findSources();

        // add pvt binary dir
        IncludeDirectories.insert(BinaryPrivateDir);

        // always add bdir to include dirs
        Public.IncludeDirectories.insert(BinaryDir);

        resolvePostponedSourceFiles();
        HeaderOnly = !hasSourceFiles();

        if (PackageDefinitions)
            addPackageDefinitions(true);

        for (auto &[p, f] : *this)
        {
            if (f->isActive() && !f->postponed)
            {
                auto f2 = f->as<NativeSourceFile>();
                if (!f2)
                    continue;
                auto ba = f2->BuildAs;
                switch (ba)
                {
                case NativeSourceFile::BasedOnExtension:
                    break;
                case NativeSourceFile::C:
                    if (auto L = SourceFileStorage::findLanguageByExtension(".c"); L)
                    {
                        if (auto c = f2->compiler->as<VisualStudioCompiler>(); c)
                            c->CompileAsC = true;
                    }
                    else
                        throw std::logic_error("no C language found");
                    break;
                case NativeSourceFile::CPP:
                    if (auto L = SourceFileStorage::findLanguageByExtension(".cpp"); L)
                    {
                        if (auto c = f2->compiler->as<VisualStudioCompiler>(); c)
                            c->CompileAsCPP = true;
                    }
                    else
                        throw std::logic_error("no CPP language found");
                    break;
                case NativeSourceFile::ASM:
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
        if (getSolution()->Settings.TargetOS.Type == OSType::Windows)
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
    }
    RETURN_PREPARE_PASS;
    case 2:
        // resolve
    {
        /*if (precomputed_data)
        {
            TargetOptionsGroup::iterate<WithoutSourceFileStorage, WithNativeOptions>(
                [this](auto &v, auto &s)
            {
                v.Dependencies.clear();
            });

            auto &j = precomputed_data.value();
            for (auto ijs = j.begin(); ijs != j.end(); ++ijs)
            {
                auto i = std::stoi(ijs.key());
                auto &s = getInheritanceStorage()[i];

                for (auto it = j["dependencies"].begin(); it != j["dependencies"].end(); ++it)
                {
                    auto d = std::make_shared<Dependency>(it.key());
                    s += d;
                    d->IncludeDirectoriesOnly = it.value()["idir"];
                    d->Dummy = it.value()["dummy"];
                }
            }
        }*/
    }
    RETURN_PREPARE_PASS;
    case 3:
        // inheritance
    {
        if (precomputed_data)
        {
        }
        else
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
                    if (d->isDummy())
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
                    // simple check
                    if (d->target.lock() == nullptr)
                    {
                        throw std::logic_error("Package: " + pkg.toString() + ": Unresolved package on stage 2: " + d->package.toString());
                        /*LOG_ERROR(logger, "Package: " + pkg.toString() + ": Unresolved package on stage 2: " + d->package.toString() + ". Resolving inplace");
                        auto id = d->package.resolve();
                        d->target = std::static_pointer_cast<NativeTarget>(getSolution()->getTargetPtr(id));*/
                    }

                    // iterate over child deps
                    (*(NativeExecutedTarget*)d->target.lock().get()).TargetOptionsGroup::iterate<WithoutSourceFileStorage, WithNativeOptions>(
                        [this, &new_dependency, &deps, d = d.get(), &deps_ordered](auto &v, auto &s)
                    {
                        // nothing to do with private inheritance
                        if (s.Inheritance == InheritanceType::Private)
                            return;

                        for (auto &d2 : v.Dependencies)
                        {
                            if (d2->target.lock().get() == this)
                                continue;
                            if (d2->isDummy())
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
                        //add(deps.find(d)->first);
                        Dependencies.insert(deps.find(d)->first);
                    break;
                }
            }
        }

        // Here we check if some deps are not included in solution target set (children).
        // They could be in dummy children, because of different target scope, not listed on software network,
        // but still in use.
        // We add them back to children.
        // Example: helpers, small tools, code generators.
        // TODO: maybe reconsider
        {
            auto &c = getSolution()->children;
            auto &dc = getSolution()->dummy_children;
            for (auto &d2 : Dependencies)
            {
                if (d2->target.lock() &&
                    c.find(d2->target.lock()->pkg) == c.end(d2->target.lock()->pkg) &&
                    dc.find(d2->target.lock()->pkg) != dc.end(d2->target.lock()->pkg))
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
        // merge
    {
        // merge self
        merge();

        // merge deps' stuff
        for (auto &d : Dependencies)
        {
            // we also apply targets to deps chains as we finished with deps
            d->propagateTargetToChain();

            if (d->isDummy())
                continue;

            GroupSettings s;
            //s.merge_to_self = false;
            merge(*(NativeExecutedTarget*)d->target.lock().get(), s);
        }
    }
    RETURN_PREPARE_PASS;
    case 5:
        // source files
    {
        // check postponed files first
        for (auto &[p, f] : *this)
        {
            if (!f->postponed || f->skip)
                continue;

            auto ext = p.extension().string();
            auto i = SourceFileStorage::findLanguageByExtension(ext);

            if (!i)
                throw std::logic_error("User defined program not registered");

            /*auto e = target->extensions.find(ext);
            if (e == target->extensions.end())
                throw std::logic_error("Bad extension - someone removed it?");

            auto &program = e->second;
            auto i = target->getLanguage(program);
            if (!i)
            {
                //auto i2 = getSolution()->children.find(program);
                //if (i2 == getSolution()->children.end())
                    throw std::logic_error("User defined program not registered");
                //target->registerLanguage(*i2->second, );
            }*/

            auto L = i->clone(); // clone program here
            f = this->SourceFileMapThis::operator[](p) = L->createSourceFile(*this, p);
        }

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
                if (getCppHeaderFileExtensions().find(e.string()) != getCppHeaderFileExtensions().end())
                    fs::copy_file(f.file, d / f.file.filename());
            }
        }

        // before merge
        if (getSolution()->Settings.Native.ConfigurationType != ConfigurationType::Debug)
            *this += "NDEBUG"_d;
        // allow to other compilers?
        else if (getSolution()->Settings.Native.CompilerType == CompilerType::MSVC)
            *this += "_DEBUG"_d;

        auto vs_setup = [this](auto *f, auto *c)
        {
            if (getSolution()->Settings.Native.MT)
                c->RuntimeLibrary = vs::RuntimeLibraryType::MultiThreaded;

            switch (getSolution()->Settings.Native.ConfigurationType)
            {
            case ConfigurationType::Debug:
                c->RuntimeLibrary =
                    getSolution()->Settings.Native.MT ?
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

            if (IsConfig && c->PrecompiledHeader && c->PrecompiledHeader().create)
            {
                // why?
                c->IncludeDirectories.erase(BinaryDir);
                c->IncludeDirectories.erase(BinaryPrivateDir);
            }
        };

        auto gnu_setup = [this](auto *f, auto *c)
        {
            switch (getSolution()->Settings.Native.ConfigurationType)
            {
            case ConfigurationType::Debug:
                c->GenerateDebugInfo = true;
                //c->Optimizations().Level = 0; this is default
                break;
            case ConfigurationType::Release:
                c->Optimizations().Level = 3;
                break;
            case ConfigurationType::ReleaseWithDebugInformation:
                c->GenerateDebugInfo = true;
                c->Optimizations().Level = 2;
                break;
            case ConfigurationType::MinimalSizeRelease:
                c->Optimizations().SmallCode = true;
                c->Optimizations().Level = 2;
                break;
            }
            if (f->file.extension() != ".c")
                c->CPPStandard = CPPVersion;

            if (ExportAllSymbols)
                c->VisibilityHidden = false;
        };

        // merge file compiler options with target compiler options
        for (auto &f : files)
        {
            // set everything before merge!
            f->compiler->merge(*this);

            if (auto c = f->compiler->as<VisualStudioCompiler>())
            {
                if (UseModules)
                {
                    c->UseModules = UseModules;
                    //c->stdIfcDir = c->System.IncludeDirectories.begin()->parent_path() / "ifc" / (getSolution()->Settings.TargetOS.Arch == ArchType::x86_64 ? "x64" : "x86");
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
            else if (auto c = f->compiler->as<ClangClCompiler>())
            {
                vs_setup(f, c);
            }
            // clang compiler is not working atm, gnu is created instead
            else if (auto c = f->compiler->as<ClangCompiler>())
            {
                gnu_setup(f, c);
            }
            else if (auto c = f->compiler->as<GNUCompiler>())
            {
                gnu_setup(f, c);
            }
        }

        //
        if (::sw::gatherSourceFiles<RcToolSourceFile>(*this).empty()
            && getSelectedTool() == Linker.get()
            && !HeaderOnly.value()
            && !IsConfig
            && getSolution()->Settings.TargetOS.is(OSType::Windows)
            )
        {
            struct RcContext : primitives::Context
            {
                using Base = primitives::Context;

                RcContext(Version file_ver, Version product_ver)
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

            RcContext ctx(pkg.version, pkg.version);
            ctx.begin();

            ctx.beginBlock("StringFileInfo");
            ctx.beginBlock("040904b0");
            //VALUE "CompanyName", "TODO: <Company name>"
            ctx.addValueQuoted("FileDescription", { pkg.ppath.back() + " - " + getConfig() });
            ctx.addValueQuoted("FileVersion", { pkg.version.toString() });
            //VALUE "InternalName", "@PACKAGE@"
            ctx.addValueQuoted("LegalCopyright", { "Powered by Software Network" });
            ctx.addValueQuoted("OriginalFilename", { pkg.toString() });
            ctx.addValueQuoted("ProductName", { pkg.ppath.toString() });
            ctx.addValueQuoted("ProductVersion", { pkg.version.toString() });
            ctx.endBlock();
            ctx.endBlock();

            ctx.beginBlock("VarFileInfo");
            ctx.addValue("Translation", { "0x409","1200" });
            ctx.endBlock();

            ctx.end();

            path p = BinaryPrivateDir / "sw.rc";
            write_file_if_different(p, ctx.getText());
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

        // legit? actually no
        // merge here only compiler options
        // TODO: find more generalized way
        //getSelectedTool()->merge(*this);
        getSelectedTool()->LinkOptions.insert(getSelectedTool()->LinkOptions.end(), LinkOptions.begin(), LinkOptions.end());

        // pdb
        if (auto c = getSelectedTool()->as<VisualStudioLinker>())
        {
            c->GenerateDebugInfo = c->GenerateDebugInfo() ||
                getSolution()->Settings.Native.ConfigurationType == ConfigurationType::Debug ||
                getSolution()->Settings.Native.ConfigurationType == ConfigurationType::ReleaseWithDebugInformation;
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
        if (ExportAllSymbols && getSolution()->Settings.TargetOS.Type == OSType::Windows && getSelectedTool() == Linker.get())
        {
            const path def = NATIVE_TARGET_DEF_SYMBOLS_FILE;
            Files objs;
            for (auto &f : files)
                objs.insert(f->output.file);
            SW_MAKE_EXECUTE_BUILTIN_COMMAND_AND_ADD(c, *this, "sw_create_def_file");
            c->record_inputs_mtime = true;
            c->args.push_back(def.u8string());
            c->push_back(objs);
            c->addInput(objs);
            c->addOutput(def);
            add(def);
        }

        // add def file to linker
        if (getSelectedTool() == Linker.get())
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
        // link libraries
    {
        // add link libraries from deps
        if (!HeaderOnly.value() && getSelectedTool() != Librarian.get())
        {
            String s;
            for (auto &d : Dependencies)
            {
                if (d->target.lock().get() == this)
                    continue;
                if (d->isDummy())
                    continue;

                s += d.get()->target.lock()->pkg.ppath.toString();
                if (d->IncludeDirectoriesOnly)
                {
                    s += ": i";
                    continue;
                }
                s += "\n";

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
                    CircularLinker->setOutputFile(getOutputFileName(getOutputBaseDir()));
                    CircularLinker->setImportLibrary(getOutputFileName(getUserDirectories().storage_dir_lib));
                    IsConfig = o;

                    if (auto c = CircularLinker->as<VisualStudioLinker>())
                    {
                        c->Force = vs::ForceType::Unresolved;
                    }
                }

                if (!dt->HeaderOnly.value())
                {
                    path o;
                    if (dt->getSelectedTool() == dt->Librarian.get())
                        o = d.get()->target.lock()->getOutputFile();
                    else
                        o = d.get()->target.lock()->getImportLibrary();
                    if (!o.empty())
                        LinkLibraries.push_back(o);
                }
            }
            if (!s.empty())
                write_file(BinaryDir.parent_path() / "deps.txt", s);
        }
    }
    RETURN_PREPARE_PASS;
    case 7:
        // linker
    {
        // add more link libraries from deps
        if (!HeaderOnly.value() && getSelectedTool() != Librarian.get())
        {
            std::unordered_set<NativeExecutedTarget*> targets;
            Files added;
            added.insert(LinkLibraries.begin(), LinkLibraries.end());
            gatherStaticLinkLibraries(LinkLibraries, added, targets);
        }

        // linker setup
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
            CircularLinker->getCommand(*this);
        }

        if (!HeaderOnly.value() && getSelectedTool() != Librarian.get())
        {
            for (auto &f : ::sw::gatherSourceFiles<RcToolSourceFile>(*this))
                obj.insert(f->output.file);
        }

        getSelectedTool()->setObjectFiles(obj);
        getSelectedTool()->setInputLibraryDependencies(O1);

        getSolution()->call_event(*this, CallbackType::EndPrepare);
    }
    RETURN_PREPARE_PASS;
    case 8:
        savePrecomputedData();
        break;
    }

    return false;
}

void NativeExecutedTarget::gatherStaticLinkLibraries(LinkLibrariesType &ll, Files &added, std::unordered_set<NativeExecutedTarget*> &targets)
{
    if (!targets.insert(this).second)
        return;
    for (auto &d : Dependencies)
    {
        if (d->target.lock().get() == this)
            continue;
        if (d->isDummy())
            continue;
        if (d->IncludeDirectoriesOnly)
            continue;

        auto dt = ((NativeExecutedTarget*)d->target.lock().get());

        // here we must gather all static (and header only?) lib deps in recursive manner
        if (dt->getSelectedTool() == dt->Librarian.get() || dt->HeaderOnly.value())
        {
            auto add = [&added, &ll](auto &dt, const path &base)
            {
                if (added.find(base) == added.end())
                {
                    ll.push_back(base);
                    ll.insert(ll.end(), dt->LinkLibraries.begin(), dt->LinkLibraries.end()); // also link libs
                }
                else
                {
                    // we added output file but not its system libs
                    for (auto &l : dt->LinkLibraries)
                    {
                        if (std::find(ll.begin(), ll.end(), l) == ll.end())
                            ll.push_back(l);
                    }
                }
            };

            if (!dt->HeaderOnly.value())
                add(dt, dt->getOutputFile());

            // if dep is a static library, we take all its deps link libraries too
            for (auto &d2 : dt->Dependencies)
            {
                if (d2->target.lock().get() == this)
                    continue;
                if (d2->target.lock().get() == d->target.lock().get())
                    continue;
                if (d2->isDummy())
                    continue;
                if (d2->IncludeDirectoriesOnly)
                    continue;

                auto dt2 = ((NativeExecutedTarget*)d2->target.lock().get());
                if (!dt2->HeaderOnly.value())
                    add(dt2, dt2->getImportLibrary());
                dt2->gatherStaticLinkLibraries(ll, added, targets);
            }
        }
    }
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

            if (getSolution()->Settings.TargetOS.Type == OSType::Windows)
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

        if (Type == LibraryType::Shared)
        {
            Definitions["CPPAN_SHARED_BUILD"];
            //Definitions["SW_SHARED_BUILD"];
        }
        else if (Type == LibraryType::Static)
        {
            Definitions["CPPAN_STATIC_BUILD"];
            //Definitions["SW_STATIC_BUILD"];
        }

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
        // probably setting dll must affect .dll extension automatically
        Linker->Extension = getSolution()->Settings.TargetOS.getSharedLibraryExtension();
        if (Linker->Type == LinkerType::MSVC)
        {
            // set machine to target os arch
            auto L = Linker->as<VisualStudioLinker>();
            L->Dll = true;
        }
        else if (Linker->Type == LinkerType::GNU)
        {
            auto L = Linker->as<GNULinker>();
            L->SharedObject = true;
        }
        if (getSolution()->Settings.TargetOS.Type == OSType::Windows)
            Definitions["_WINDLL"];
    }
    else
    {
        SelectedTool = Librarian.get();
    }
}

void NativeExecutedTarget::removeFile(const path &fn, bool binary_dir)
{
    remove_full(fn);
    Target::removeFile(fn, binary_dir);
}

void NativeExecutedTarget::configureFile(path from, path to, ConfigureFlags flags)
{
    // before resolving
    if (!to.is_absolute())
        to = BinaryDir / to;
    File(to, *getSolution()->fs).getFileRecord().setGenerated();

    if (PostponeFileResolving || DryRun)
        return;

    if (!from.is_absolute())
    {
        if (fs::exists(SourceDir / from))
            from = SourceDir / from;
        else if (fs::exists(BinaryDir / from))
            from = BinaryDir / from;
        else
            throw SW_RUNTIME_ERROR("Package: " + pkg.toString() + ", file not found: " + from.string());
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

void NativeExecutedTarget::configureFile1(const path &from, const path &to, ConfigureFlags flags)
{
    static const std::regex cmDefineRegex(R"xxx(#cmakedefine[ \t]+([A-Za-z_0-9]*)([^\r\n]*?)[\r\n])xxx");
    static const std::regex cmDefine01Regex(R"xxx(#cmakedefine01[ \t]+([A-Za-z_0-9]*)[^\r\n]*?[\r\n])xxx");
    static const std::regex mesonDefine(R"xxx(#mesondefine[ \t]+([A-Za-z_0-9]*)[^\r\n]*?[\r\n])xxx");
    static const std::regex undefDefine(R"xxx(#undef[ \t]+([A-Za-z_0-9]*)[^\r\n]*?[\r\n])xxx");
    static const std::regex cmAtVarRegex("@([A-Za-z_0-9/.+-]+)@");
    static const std::regex cmNamedCurly("\\$\\{([A-Za-z0-9/_.+-]+)\\}");

    static const std::set<std::string> offValues{
        "","OFF","0","NO","FALSE","N","IGNORE",
    };

    auto s = read_file(from);

    if ((int)flags & (int)ConfigureFlags::CopyOnly)
    {
        writeFileOnce(to, s);
        return;
    }

    auto find_repl = [this, &from, flags](const auto &key) -> std::string
    {
        auto v = Variables.find(key);
        if (v != Variables.end())
            return v->second.toString();
        // dangerous! should we really check defs?
        auto d = Definitions.find(key);
        if (d != Definitions.end())
            return d->second.toString();
        //if (isLocal()) // put under cl cond
            //LOG_WARN(logger, "Unset variable '" + key + "' in file: " + normalize_path(from));
        if ((int)flags & (int)ConfigureFlags::ReplaceUndefinedVariablesWithZeros)
            return "0";
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

    // #mesondefine
    while (std::regex_search(s, m, mesonDefine))
    {
        auto repl = find_repl(m[1].str());
        if (offValues.find(boost::to_upper_copy(repl)) != offValues.end())
            s = m.prefix().str() + "/* #undef " + m[1].str() + " */" + "\n" + m.suffix().str();
        else
            s = m.prefix().str() + "#define " + m[1].str() + " " + repl + "\n" + m.suffix().str();
    }

    // #undef
    if ((int)flags & (int)ConfigureFlags::EnableUndefReplacements)
        while (std::regex_search(s, m, undefDefine))
        {
            auto repl = find_repl(m[1].str());
            if (offValues.find(boost::to_upper_copy(repl)) != offValues.end())
                s = m.prefix().str() + m.suffix().str();
            else
                s = m.prefix().str() + "#define " + m[1].str() + " " + repl + "\n" + m.suffix().str();
        }

    // #cmakedefine
    while (std::regex_search(s, m, cmDefineRegex))
    {
        auto repl = find_repl(m[1].str());
        if (offValues.find(boost::to_upper_copy(repl)) != offValues.end())
            s = m.prefix().str() + "/* #undef " + m[1].str() + m[2].str() + " */\n" + m.suffix().str();
        else
            s = m.prefix().str() + "#define " + m[1].str() + m[2].str() + "\n" + m.suffix().str();
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

    writeFileOnce(to, s);
}

void NativeExecutedTarget::setChecks(const String &name)
{
    auto i0 = solution->checker.sets.find(getSolution()->current_gn);
    if (i0 == solution->checker.sets.end())
        return;
    auto i = i0->second.find(name);
    if (i == i0->second.end())
        return;
    for (auto &[k, c] : i->second.check_values)
    {
        auto d = c->getDefinition(k);
        const auto v = c->Value.value();
        // make private?
        // remove completely?
        if (d)
        {
            //Public.Definitions[d.value()];
            add(Definition{ d.value() });

            //for (auto &p : c->Prefixes)
                //add(Definition{ p + d.value() });
            /*for (auto &d2 : c->Definitions)
            {
                for (auto &p : c->Prefixes)
                    Definitions[p + d2] = v;
            }*/
        }
        Variables[k] = v;

        //for (auto &p : c->Prefixes)
            //Variables[p + k] = v;
        /*for (auto &d2 : c->Definitions)
        {
            for (auto &p : c->Prefixes)
                Variables[p + d2] = v;
        }*/
    }
}

path NativeExecutedTarget::getPatchDir(bool binary_dir) const
{
    path base;
    if (auto d = pkg.getOverriddenDir(); d)
        base = d.value() / SW_BINARY_DIR;
    else if (!Local)
        base = pkg.getDirSrc();
    else
        base = getSolution()->BinaryDir;
    return base / "patch";

    //auto base = ((binary_dir || Local) ? BinaryDir : SourceDir;
    //return base.parent_path() / "patch";

    /*path base;
    if (isLocal())
        base = "";
    auto base = Local ? getSolution()->bi : SourceDir;
    return base / "patch";*/
}

void NativeExecutedTarget::writeFileOnce(const path &fn, const String &content) const
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
        File f(p, *getSolution()->fs);
        f.getFileRecord().setGenerated();
    }

    if (PostponeFileResolving || DryRun)
        return;

    ::sw::writeFileOnce(p, content, getPatchDir(!source_dir));

    //File f(p, *getSolution()->fs);
    //f.getFileRecord().load();
}

void NativeExecutedTarget::writeFileSafe(const path &fn, const String &content) const
{
    if (PostponeFileResolving || DryRun)
        return;

    bool source_dir = false;
    path p = fn;
    check_absolute(p, false, &source_dir);
    ::sw::writeFileSafe(p, content, getPatchDir(!source_dir));

    //File f(fn, *getSolution()->fs);
    //f.getFileRecord().load();
}

void NativeExecutedTarget::replaceInFileOnce(const path &fn, const String &from, const String &to) const
{
    patch(fn, from, to);
}

void NativeExecutedTarget::patch(const path &fn, const String &from, const String &to) const
{
    if (PostponeFileResolving || DryRun)
        return;

    bool source_dir = false;
    path p = fn;
    check_absolute(p, false, &source_dir);
    ::sw::replaceInFileOnce(p, from, to, getPatchDir(!source_dir));

    //File f(p, *getSolution()->fs);
    //f.getFileRecord().load();
}

void NativeExecutedTarget::patch(const path &fn, const String &patch_str) const
{
    if (PostponeFileResolving || DryRun)
        return;

    bool source_dir = false;
    path p = fn;
    check_absolute(p, false, &source_dir);
    ::sw::patch(p, patch_str, getPatchDir(!source_dir));
}

void NativeExecutedTarget::deleteInFileOnce(const path &fn, const String &from) const
{
    replaceInFileOnce(fn, from, "");
}

void NativeExecutedTarget::pushFrontToFileOnce(const path &fn, const String &text) const
{
    if (PostponeFileResolving || DryRun)
        return;

    bool source_dir = false;
    path p = fn;
    check_absolute(p, false, &source_dir);
    ::sw::pushFrontToFileOnce(p, text, getPatchDir(!source_dir));

    //File f(p, *getSolution()->fs);
    //f.getFileRecord().load();
}

void NativeExecutedTarget::pushBackToFileOnce(const path &fn, const String &text) const
{
    if (PostponeFileResolving || DryRun)
        return;

    bool source_dir = false;
    path p = fn;
    check_absolute(p, false, &source_dir);
    ::sw::pushBackToFileOnce(p, text, getPatchDir(!source_dir));

    //File f(p, *getSolution()->fs);
    //f.getFileRecord().load();
}

void load_source_and_version(const yaml &root, Source &source, Version &version)
{
    String ver;
    YAML_EXTRACT_VAR(root, ver, "version", String);
    if (!ver.empty())
        version = Version(ver);
    if (!load_source(root, source))
        return;
    //visit([&version](auto &v) { v.loadVersion(version); }, source);
}

void NativeExecutedTarget::cppan_load_project(const yaml &root)
{
    load_source_and_version(root, source, pkg.version);

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
            auto nppath = dependency.ppath / v;
            dependency.ppath = nppath;
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


#if 0
    YAML_EXTRACT_AUTO(output_name);
    YAML_EXTRACT_AUTO(condition);
    YAML_EXTRACT_AUTO(include_script);
    license = get_scalar<String>(root, "license");

    read_dir(unpack_directory, "unpack_directory");
    if (unpack_directory.empty())
        read_dir(unpack_directory, "unpack_dir");

    YAML_EXTRACT_AUTO(output_directory);
    if (output_directory.empty())
        YAML_EXTRACT_VAR(root, output_directory, "output_dir", String);

    bs_insertions.load(root);
    options = loadOptionsMap(root);

    read_sources(public_headers, "public_headers");
    include_hints = get_sequence_set<String>(root, "include_hints");

    aliases = get_sequence_set<String>(root, "aliases");

    checks.load(root);
    checks_prefixes = get_sequence_set<String>(root, "checks_prefixes");
    if (checks_prefixes.empty())
        checks_prefixes = get_sequence_set<String>(root, "checks_prefix");

    const auto &patch_node = root["patch"];
    if (patch_node.IsDefined())
        patch.load(patch_node);
#endif
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
            if (getSolution()->Settings.TargetOS.Type == OSType::Windows)
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
        //Definitions["SW_EXECUTABLE"];

        set_api(ApiName);
        for (auto &a : ApiNames)
            set_api(a);
    }
    break;
    }

    return NativeExecutedTarget::prepare();
}

path ExecutableTarget::getOutputBaseDir() const
{
    return getUserDirectories().storage_dir_bin;
}

void ExecutableTarget::cppan_load_project(const yaml &root)
{
    /*String et;
    YAML_EXTRACT_VAR(root, et, "executable_type", String);
    if (et == "win32")
        executable_type = ExecutableType::Win32;*/

    NativeExecutedTarget::cppan_load_project(root);
}

bool LibraryTarget::prepare()
{
    return prepareLibrary(getSolution()->Settings.Native.LibrariesType);
}

void LibraryTarget::init()
{
    NativeExecutedTarget::init();
    initLibrary(getSolution()->Settings.Native.LibrariesType);
    setOutputFile(); // after initLibrary
}

void StaticLibraryTarget::init()
{
    NativeExecutedTarget::init();
    initLibrary(LibraryType::Static);
    setOutputFile(); // after initLibrary
}

void SharedLibraryTarget::init()
{
    NativeExecutedTarget::init();
    initLibrary(LibraryType::Shared);
    setOutputFile(); // after initLibrary
}

}
