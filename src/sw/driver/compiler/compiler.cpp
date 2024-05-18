// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "compiler.h"

#include "../build.h"
#include "../command.h"
#include "compiler_helpers.h"
#include "../target/native.h"

#include <sw/core/sw_context.h>
#include <sw/manager/storage.h>

#include <boost/algorithm/string.hpp>
#include <primitives/sw/settings.h>

#include <regex>
#include <string>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "compiler");

#define SW_CREATE_COMPILER_COMMAND(t, ct)                                                   \
    std::shared_ptr<driver::Command> t::createCommand1(const SwBuilderContext &swctx) const \
    {                                                                                       \
        auto c = std::make_shared<ct>(swctx);                                               \
        c->setProgram(file);                                                                \
        return c;                                                                           \
    }

namespace sw
{

std::map<path, String> &getMsvcIncludePrefixes();

static String get_msvc_prefix(const path &prog)
{
    auto &p = getMsvcIncludePrefixes();
    auto i = p.find(prog);
    if (i == p.end())
        throw SW_RUNTIME_ERROR("Cannot find msvc prefix for: " + prog.string());
    return i->second;
}

static void add_args(driver::Command &c, const Strings &args)
{
    for (auto &a : args)
        c.arguments.push_back(a);
}

CompilerBaseProgram::CompilerBaseProgram(const CompilerBaseProgram &rhs)
    : FileToFileTransformProgram(rhs)
{
    Prefix = rhs.Prefix;
    Extension = rhs.Extension;
    if (rhs.cmd)
        cmd = rhs.cmd->clone();
}

std::shared_ptr<builder::Command> CompilerBaseProgram::getCommand() const
{
    if (!cmd)
        throw SW_RUNTIME_ERROR("Command is not created");
    if (!prepared)
        throw SW_RUNTIME_ERROR("Command is not prepared");
    return cmd;
}

std::shared_ptr<builder::Command> CompilerBaseProgram::createCommand(const SwBuilderContext &swctx)
{
    if (cmd)
        return cmd;
    return cmd = createCommand1(swctx);
}

std::shared_ptr<builder::Command> CompilerBaseProgram::getCommand(const Target &t)
{
    prepareCommand(t);
    return getCommand();
}

std::shared_ptr<builder::Command> CompilerBaseProgram::prepareCommand(const Target &t)
{
    if (prepared)
        return cmd;
    createCommand(t.getMainBuild()); // do some init
    prepareCommand1(t);
    prepared = true;
    return cmd;
}

SW_CREATE_COMPILER_COMMAND(CompilerBaseProgram, driver::Command)

std::shared_ptr<SourceFile> CompilerBaseProgram::createSourceFile(const Target &t, const path &input) const
{
    return std::make_shared<SourceFile>(input);
}

static Strings getCStdOption(CLanguageStandard std, bool gnuext, bool clang, bool appleclang, const Version &clver)
{
    String s = "-std="s + (gnuext ? "gnu" : "c");
    switch (std)
    {
    case CLanguageStandard::C89:
        s += "89";
        break;
    case CLanguageStandard::C99:
        s += "99";
        break;
    case CLanguageStandard::C11:
        s += "11";
        break;
    case CLanguageStandard::C17:
        s += "17";
        break;
    case CLanguageStandard::C23:
        if (
            clang && clver >= Version(18) ||
            !appleclang && !clang && clver >= Version(14))
            s += "23";
        else
            s += "2x";
        break;
    default:
        return {};
    }
    return { s };
}

static Strings getCppStdOption(CPPLanguageStandard std, bool gnuext, bool clang, bool appleclang, const Version &clver)
{
    // for apple clang versions
    // see https://en.wikipedia.org/wiki/Xcode#Toolchain_versions

    String s = "-std="s + (gnuext ? "gnu" : "c") + "++";
    switch (std)
    {
    case CPPLanguageStandard::CPP11:
        s += "11";
        break;
    case CPPLanguageStandard::CPP14:
        s += "14";
        break;
    case CPPLanguageStandard::CPP17:
        if (
            appleclang && clver > Version(9) ||
            clang && clver > Version(5) ||
            !appleclang && !clang && clver > Version(6)
            )
            s += "17";
        else
            s += "1z";
        break;
    case CPPLanguageStandard::CPP20:
        if (
            // appleclang 12.0.0 = llvm (clang) 10.0.0 which does not have c++20 flag (only >= 11)
            //appleclang && clver >= Version(12) ||
            clang && clver > Version(10) ||
            !appleclang && !clang && clver > Version(9)
            )
            s += "20";
        else
            s += "2a";
        break;
    case CPPLanguageStandard::CPP23:
        if (
            clang && clver >= Version(17) ||
            !appleclang && !clang && clver >= Version(11)
            )
            s += "23";
        else
            s += "2b";
        break;
    case CPPLanguageStandard::CPP26:
        if (
            clang && clver >= Version(18) ||
            !appleclang && !clang && clver >= Version(14))
            s += "26";
        else
            s += "2c";
        break;
    default:
        return {};
    }
    return { s };
}

static Strings getCStdOptionMsvc(CLanguageStandard std, const Version &clver, bool clangcl = false)
{
    String s = "-std:c";
    switch (std)
    {
    case CLanguageStandard::C11:
        s += "11";
        break;
    case CLanguageStandard::C17:
        s += "17";
        break;
    case CLanguageStandard::C23:
        s += "latest";
        break;
    default:
        return {};
    }
    return {s};
}

static Strings getCppStdOptionMsvc(CPPLanguageStandard std, const Version &clver, bool clangcl = false)
{
    String s = "-std:c++";
    switch (std)
    {
    case CPPLanguageStandard::CPP14:
        s += "14";
        break;
    case CPPLanguageStandard::CPP17:
        s += "17";
        break;
    case CPPLanguageStandard::CPP20:
        if (clver < Version(19, 30, 30401) && !clangcl) // probably less than vs16.11, not vs17
            s += "latest";
        else
            s += "20";
        break;
    case CPPLanguageStandard::CPP23:
    case CPPLanguageStandard::CPP26:
    //case CPPLanguageStandard::CPPLatest:
        s += "latest";
        break;
    default:
        return {};
    }
    return { s };
}

String NativeCompiler::getObjectExtension(const OS &o) const
{
    return o.getObjectFileExtension();
}

template <class C>
static path getOutputFile(const Target &t, const C &c, const path &input)
{
    auto o = t.BinaryDir.parent_path() / "obj" /
        (SourceFile::getObjectFilename(t, input) += c.getObjectExtension(t.getBuildSettings().TargetOS));
    o = fs::absolute(o);
    return o;
}

std::shared_ptr<SourceFile> NativeCompiler::createSourceFile(const Target &t, const path &input) const
{
    return std::make_shared<NativeSourceFile>(*this, input, ::sw::getOutputFile(t, *this, input));
}

void NativeCompiler::merge(const NativeCompiledTarget &t)
{
    NativeCompilerOptions::merge(t.getMergeObject());
}

SW_CREATE_COMPILER_COMMAND(VisualStudioCompiler, driver::Command)

void VisualStudioCompiler::prepareCommand1(const Target &t)
{
    // msvc compilers - _MSC_VER
    // https://en.wikipedia.org/wiki/Microsoft_Visual_C%2B%2B#Internal_version_numbering

    cmd->deps_processor = builder::Command::DepsProcessor::Msvc;
    cmd->msvc_prefix = get_msvc_prefix(cmd->getProgram());

    if (InputFile)
    {
        cmd->name = to_string(normalize_path(InputFile()));
        cmd->name_short = to_string(InputFile().filename().u8string());
    }

    bool preprocessed_file = false;
    if (CSourceFile)
    {
        cmd->name = to_string(normalize_path(CSourceFile()));
        cmd->name_short = to_string(CSourceFile().filename().u8string());
    }
    else if (CPPSourceFile)
    {
        cmd->name = to_string(normalize_path(CPPSourceFile()));
        cmd->name_short = to_string(CPPSourceFile().filename().u8string());
    }
    else if (InputFile && !CompileAsC && !CompileAsCPP)
    {
        // .C extension is treated as C language by default (Wt library)
        auto &exts = getCppSourceFileExtensions();
        if (exts.find(InputFile().extension().string()) != exts.end())
        {
            CompileAsCPP = true;
        }
        else if (InputFile().extension() == ".i")
        {
            CompileAsC = true;
            preprocessed_file = true;
        }
        else if (InputFile().extension() == ".ii")
        {
            CompileAsCPP = true;
            preprocessed_file = true;
        }
    }

    if (Output)
        cmd->working_directory = Output().parent_path();

    if (PreprocessToFile)
    {
        auto ext = ".i";
        if (CompileAsCPP)
            ext = ".ii";
        if (!PreprocessFileName)
            PreprocessFileName = Output().parent_path() / (to_string(Output().stem().u8string()) + ext);
        Output.clear();
    }

    ReproducibleBuild = t.isReproducibleBuild();

    if (CStandard)
    {
        add_args(*cmd, getCStdOptionMsvc(CStandard(), getVersion(t.getContext(), file)));
        CStandard.skip = true;
    }

    add_args(*cmd, getCppStdOptionMsvc(CPPStandard(), getVersion(t.getContext(), file)));
    CPPStandard.skip = true;

    getCommandLineOptions<VisualStudioCompilerOptions>(cmd.get(), *this);
    if (preprocessed_file)
        addCompileOptions(*cmd);
    else
        addEverything(*cmd);
}

void VisualStudioCompiler::setOutputFile(const path &output_file)
{
    Output = output_file;
}

SW_DEFINE_PROGRAM_CLONE(VisualStudioCompiler)

void VisualStudioCompiler::setSourceFile(const path &input_file, const path &output_file)
{
    InputFile = input_file;
    VisualStudioCompiler::setOutputFile(output_file);
}

path VisualStudioCompiler::getOutputFile() const
{
    return Output();
}

SW_CREATE_COMPILER_COMMAND(VisualStudioASMCompiler, driver::Command)

void VisualStudioASMCompiler::prepareCommand1(const Target &t)
{
    if (file.filename() == "ml64.exe")
        ((VisualStudioASMCompiler*)this)->SafeSEH = false;

    //cmd->deps_processor = builder::Command::DepsProcessor::Msvc;
    //cmd->msvc_prefix = get_msvc_prefix(cmd->getProgram());

    if (InputFile)
    {
        cmd->name = to_string(normalize_path(InputFile()));
        cmd->name_short = to_string(InputFile().filename().u8string());
        //cmd->file = InputFile;
    }
    if (Output)
        cmd->working_directory = Output().parent_path();

    ReproducibleBuild = t.isReproducibleBuild();

    //if (cmd->file.empty())
        //return nullptr;

    //cmd->out.capture = true;
    //cmd->base = clone();

    // defs and idirs for asm must go before file
    addEverything(*cmd);
    getCommandLineOptions<VisualStudioAssemblerOptions>(cmd.get(), *this);
}

SW_DEFINE_PROGRAM_CLONE(VisualStudioASMCompiler)

void VisualStudioASMCompiler::setOutputFile(const path &output_file)
{
    Output = output_file;
}

path VisualStudioASMCompiler::getOutputFile() const
{
    return Output();
}

void VisualStudioASMCompiler::setSourceFile(const path &input_file, const path &output_file)
{
    InputFile = input_file;
    setOutputFile(output_file);
}

SW_CREATE_COMPILER_COMMAND(ClangCompiler, driver::Command)

void ClangCompiler::prepareCommand1(const ::sw::Target &t)
{
    auto cmd = std::static_pointer_cast<driver::Command>(this->cmd);

    cmd->deps_processor = builder::Command::DepsProcessor::Gnu;

    if (InputFile)
    {
        cmd->name = to_string(normalize_path(InputFile()));
        cmd->name_short = to_string(InputFile().filename().u8string());
        //cmd->file = InputFile;
    }
    if (OutputFile)
    {
        cmd->deps_file = OutputFile().parent_path() / (OutputFile().stem() += ".d");
        cmd->output_dirs.insert(cmd->deps_file.parent_path());
        cmd->working_directory = OutputFile().parent_path();
    }

    // not available for msvc triple
    // must be enabled on per target basis (when shared lib is built)?
    if (t.getBuildSettings().TargetOS.is(OSType::Windows) ||
        t.getBuildSettings().TargetOS.is(OSType::Mingw))
    {
        PositionIndependentCode = false;
    }
    if (t.getBuildSettings().TargetOS.is(OSType::Mingw))
    {
        //cmd->push_back("-stdlib=libstdc++");
    }

    add_args(*cmd, getCStdOption(CStandard(), dynamic_cast<const NativeCompiledTarget&>(t).CExtensions,
        !appleclang, appleclang, getVersion(t.getContext(), file)));
    CStandard.skip = true;
    add_args(*cmd, getCppStdOption(CPPStandard(), dynamic_cast<const NativeCompiledTarget&>(t).CPPExtensions,
        !appleclang, appleclang, getVersion(t.getContext(), file)));
    CPPStandard.skip = true;

    getCommandLineOptions<ClangOptions>(cmd.get(), *this);
    addEverything(*this->cmd/*, "-isystem"*/);
    getCommandLineOptions<ClangOptions>(cmd.get(), *this, "", true);
}

void ClangCompiler::setOutputFile(const path &output_file)
{
    OutputFile = output_file;
}

path ClangCompiler::getOutputFile() const
{
    return OutputFile();
}

SW_DEFINE_PROGRAM_CLONE(ClangCompiler)

void ClangCompiler::setSourceFile(const path &input_file, const path &output_file)
{
    InputFile = input_file;
    setOutputFile(output_file);
}

SW_CREATE_COMPILER_COMMAND(ClangClCompiler, driver::Command)

void ClangClCompiler::prepareCommand1(const Target &t)
{
    cmd->deps_processor = builder::Command::DepsProcessor::Msvc;
    cmd->msvc_prefix = get_msvc_prefix(cmd->getProgram());

    if (InputFile)
    {
        cmd->name = to_string(normalize_path(InputFile()));
        cmd->name_short = to_string(InputFile().filename().u8string());
    }

    bool preprocessed_file = false;
    if (CSourceFile)
    {
        cmd->name = to_string(normalize_path(CSourceFile()));
        cmd->name_short = to_string(CSourceFile().filename().u8string());
    }
    else if (CPPSourceFile)
    {
        cmd->name = to_string(normalize_path(CPPSourceFile()));
        cmd->name_short = to_string(CPPSourceFile().filename().u8string());
    }
    else if (InputFile && !CompileAsC && !CompileAsCPP)
    {
        // .C extension is treated as C language by default (Wt library)
        auto &exts = getCppSourceFileExtensions();
        if (exts.find(InputFile().extension().string()) != exts.end())
        {
            CompileAsCPP = true;
        }
        else if (InputFile().extension() == ".i")
        {
            CompileAsC = true;
            preprocessed_file = true;
        }
        else if (InputFile().extension() == ".ii")
        {
            CompileAsCPP = true;
            preprocessed_file = true;
        }
    }
    if (Output)
        cmd->working_directory = Output().parent_path();

    if (PreprocessToFile)
    {
        auto ext = ".i";
        if (CompileAsCPP)
            ext = ".ii";
        if (!PreprocessFileName)
            PreprocessFileName = Output().parent_path() / (to_string(Output().stem().u8string()) + ext);
        Output.clear();
    }

    ReproducibleBuild = t.isReproducibleBuild();

    if (CStandard)
    {
        add_args(*cmd, getCStdOptionMsvc(CStandard(), getVersion(t.getContext(), file), true));
        CStandard.skip = true;
    }

    add_args(*cmd, getCppStdOptionMsvc(CPPStandard(), getVersion(t.getContext(), file), true));
    CPPStandard.skip = true;

    getCommandLineOptions<VisualStudioCompilerOptions>(cmd.get(), *this);
    getCommandLineOptions<ClangClOptions>(cmd.get(), *this/*, "-Xclang"*/);
    if (preprocessed_file)
        addCompileOptions(*cmd);
    else
        addEverything(*cmd/*, "-imsvc"*/);
}

void ClangClCompiler::setOutputFile(const path &output_file)
{
    Output = output_file;
}

path ClangClCompiler::getOutputFile() const
{
    return Output();
}

SW_DEFINE_PROGRAM_CLONE(ClangClCompiler)

void ClangClCompiler::setSourceFile(const path &input_file, const path &output_file)
{
    InputFile = input_file;
    setOutputFile(output_file);
}

SW_CREATE_COMPILER_COMMAND(GNUASMCompiler, driver::Command)

static String getRandomSeed(const path &p, const path &sw_storage_dir)
{
    if (p.empty())
        return "0"s;
    auto np = to_string(normalize_path(p));
    auto nsp = to_string(normalize_path(sw_storage_dir));
    if (np.find(nsp) != 0)
        return "0"s;
    // size() + next slash
    return std::to_string(std::hash<String>()(np.substr(nsp.size() + 1)));
}

void GNUASMCompiler::prepareCommand1(const Target &t)
{
    cmd->deps_processor = builder::Command::DepsProcessor::Gnu;

    bool assembly = false;
    if (InputFile)
    {
        cmd->name = to_string(normalize_path(InputFile()));
        cmd->name_short = to_string(InputFile().filename().u8string());
        //cmd->file = InputFile;
        assembly = InputFile().extension() == ".s";
    }
    if (OutputFile)
        cmd->working_directory = OutputFile().parent_path();

    // asm files does not have deps AFAIK
    //std::dynamic_pointer_cast<driver::GNUCommand>(cmd)->deps_file.clear();

    //if (cmd->file.empty())
        //return nullptr;

    //cmd->out.capture = true;

    getCommandLineOptions<GNUAssemblerOptions>(cmd.get(), *this);

    if (!InputFile && !assembly)
        addEverything(*cmd);

    if (t.isReproducibleBuild())
    {
        cmd->push_back("-frandom-seed=" + getRandomSeed(InputFile ? InputFile() : path{}, t.getContext().getLocalStorage().storage_dir));
        cmd->environment["SOURCE_DATE_EPOCH"] = "0";
    }
}

SW_DEFINE_PROGRAM_CLONE(GNUASMCompiler)

void GNUASMCompiler::setOutputFile(const path &output_file)
{
    OutputFile = output_file;
}

path GNUASMCompiler::getOutputFile() const
{
    return OutputFile();
}

void GNUASMCompiler::setSourceFile(const path &input_file, const path &output_file)
{
    InputFile = input_file;
    setOutputFile(output_file);
}

SW_DEFINE_PROGRAM_CLONE(ClangASMCompiler)

SW_CREATE_COMPILER_COMMAND(GNUCompiler, driver::Command)

void GNUCompiler::prepareCommand1(const Target &t)
{
    auto cmd = std::static_pointer_cast<driver::Command>(this->cmd);

    cmd->deps_processor = builder::Command::DepsProcessor::Gnu;

    if (InputFile)
    {
        cmd->name = to_string(normalize_path(InputFile()));
        cmd->name_short = to_string(InputFile().filename().u8string());
        //cmd->file = InputFile;
    }
    if (OutputFile)
    {
        cmd->deps_file = OutputFile().parent_path() / (OutputFile().stem() += ".d");
        cmd->output_dirs.insert(cmd->deps_file.parent_path());
        cmd->working_directory = OutputFile().parent_path();
    }

    //if (cmd->file.empty())
        //return nullptr;

    //cmd->out.capture = true;

    add_args(*cmd, getCStdOption(CStandard(), dynamic_cast<const NativeCompiledTarget &>(t).CExtensions,
                                 false, false, getVersion(t.getContext(), file)));
    CStandard.skip = true;
    add_args(*cmd, getCppStdOption(CPPStandard(), dynamic_cast<const NativeCompiledTarget&>(t).CPPExtensions,
        false, false, getVersion(t.getContext(), file)));
    CPPStandard.skip = true;

    getCommandLineOptions<GNUOptions>(cmd.get(), *this);
    addEverything(*this->cmd/*, "-isystem"*/);
    getCommandLineOptions<GNUOptions>(cmd.get(), *this, "", true);

    if (t.isReproducibleBuild())
    {
        cmd->push_back("-frandom-seed=" + getRandomSeed(InputFile ? InputFile() : path{}, t.getContext().getLocalStorage().storage_dir));
        cmd->environment["SOURCE_DATE_EPOCH"] = "0";
    }
}

void GNUCompiler::setOutputFile(const path &output_file)
{
    OutputFile = output_file;
}

path GNUCompiler::getOutputFile() const
{
    return OutputFile();
}

SW_DEFINE_PROGRAM_CLONE(GNUCompiler)

void GNUCompiler::setSourceFile(const path &input_file, const path &output_file)
{
    InputFile = input_file;
    // gcc does not accept this, clang does
    if (input_file.extension() == ".c")
        VisibilityInlinesHidden = false;
    setOutputFile(output_file);
}

FilesOrdered NativeLinker::gatherLinkDirectories() const
{
    FilesOrdered dirs;

    auto get_ldir = [&dirs](const auto &a)
    {
        for (auto &d : a)
            dirs.push_back(d);
    };

    get_ldir(NativeLinkerOptions::gatherLinkDirectories());
    get_ldir(NativeLinkerOptions::System.gatherLinkDirectories());

    return dirs;
}

LinkLibrariesType NativeLinker::gatherLinkLibraries(bool system) const
{
    LinkLibrariesType dirs;

    auto get_ldir = [&dirs](const auto &a)
    {
        for (auto &d : a)
            dirs.push_back(d);
    };

    if (system)
        get_ldir(NativeLinkerOptions::System.gatherLinkLibraries());
    else
        get_ldir(NativeLinkerOptions::gatherLinkLibraries());

    return dirs;
}

void VisualStudioLibraryTool::setObjectFiles(const FilesOrdered &files)
{
    InputFiles().insert(InputFiles().end(), files.begin(), files.end());
}

void VisualStudioLibraryTool::setOutputFile(const path &out)
{
    Output = out;
    Output() += Extension;
}

void VisualStudioLibraryTool::setImportLibrary(const path &out)
{
    ImportLibrary = out;
    ImportLibrary() += ".lib";
}

path VisualStudioLibraryTool::getOutputFile() const
{
    return Output;
}

path VisualStudioLibraryTool::getImportLibrary() const
{
    if (ImportLibrary)
        return ImportLibrary();
    path p = Output;
    return p.parent_path() / (p.filename().stem() += ".lib");
}

void VisualStudioLibraryTool::prepareCommand1(const Target &t)
{
    // can be zero imput files actually: lib.exe /DEF:my.def /OUT:x.lib
    //if (InputFiles().empty())
        //return nullptr;

    //LinkDirectories() = gatherLinkDirectories();
    //LinkLibraries() = gatherLinkLibraries();

    //cmd->out.capture = true;
    //cmd->base = clone();
    if (Output)
    {
        cmd->working_directory = Output().parent_path();
        cmd->name = to_string(normalize_path(Output()));
        cmd->name_short = to_string(Output().filename().u8string());
    }

    ((VisualStudioLibraryTool*)this)->VisualStudioLibraryToolOptions::LinkDirectories() = gatherLinkDirectories();

    ReproducibleBuild = t.isReproducibleBuild();

    getCommandLineOptions<VisualStudioLibraryToolOptions>(cmd.get(), *this);
    addEverything(*cmd);
    getAdditionalOptions(cmd.get());
}

SW_DEFINE_PROGRAM_CLONE(VisualStudioLinker)

void VisualStudioLinker::getAdditionalOptions(driver::Command *cmd) const
{
    getCommandLineOptions<VisualStudioLinkerOptions>(cmd, *this);
}

void VisualStudioLinker::setInputLibraryDependencies(const LinkLibrariesType &files)
{
    InputLibraryDependencies().insert(files.begin(), files.end());
}

void VisualStudioLinker::prepareCommand1(const Target &t)
{
    // can be zero imput files actually: lib.exe /DEF:my.def /OUT:x.lib
    //if (InputFiles().empty())
        //return nullptr;

    //LinkDirectories() = gatherLinkDirectories();
    //LinkLibraries() = gatherLinkLibraries();
    ((VisualStudioLinker *)this)->VisualStudioLinkerOptions::SystemLinkLibraries.value().clear();
    for (auto &l : gatherLinkLibraries(true))
        ((VisualStudioLinker *)this)->VisualStudioLinkerOptions::SystemLinkLibraries().push_back(l.l);

    //cmd->out.capture = true;
    //cmd->base = clone();
    if (Output)
    {
        cmd->working_directory = Output().parent_path();
        cmd->name = to_string(normalize_path(Output()));
        cmd->name_short = to_string(Output().filename().u8string());
    }

    ((VisualStudioLibraryTool*)this)->VisualStudioLibraryToolOptions::LinkDirectories() = gatherLinkDirectories();

    ReproducibleBuild = t.isReproducibleBuild();

    getCommandLineOptions<VisualStudioLibraryToolOptions>(cmd.get(), *this);
    addEverything(*cmd);
    getAdditionalOptions(cmd.get());
}

SW_DEFINE_PROGRAM_CLONE(VisualStudioLibrarian)

void VisualStudioLibrarian::getAdditionalOptions(driver::Command *cmd) const
{
    getCommandLineOptions<VisualStudioLibrarianOptions>(cmd, *this);
}

// https://dev.gentoo.org/~vapier/crt.txt
// http://gcc.gnu.org/onlinedocs/gccint/Initialization.html

SW_DEFINE_PROGRAM_CLONE(GNULinker)

void GNULinker::setObjectFiles(const FilesOrdered &files)
{
    InputFiles().insert(InputFiles().end(), files.begin(), files.end());
}

static auto add_prefix_and_suffix(const path &p, const String &prefix, const String &ext)
{
    return p.parent_path() / (prefix + to_string(p.filename().u8string()) + ext);
}

static auto remove_prefix_and_suffix(const path &p)
{
    auto s = to_path_string(p.stem());
    if (to_printable_string(s).find("lib") == 0)
        s = s.substr(3);
    return s;
}

void GNULinker::setOutputFile(const path &out)
{
    Output = add_prefix_and_suffix(out, Prefix, Extension).u8string();
}

void GNULinker::setImportLibrary(const path &out)
{
    //ImportLibrary = out.u8string();// + ".lib";
}

void GNULinker::setLinkLibraries(const LinkLibrariesType &in)
{
    for (auto &lib : in)
        NativeLinker::LinkLibraries.push_back(lib);
}

void GNULinker::setInputLibraryDependencies(const LinkLibrariesType &files)
{
    if (files.empty())
		return;
    // use start/end groups
    // https://eli.thegreenplace.net/2013/07/09/library-order-in-static-linking
    if (use_start_end_groups)
        StartGroup = true;
    InputLibraryDependencies().insert(files.begin(), files.end());
    if (use_start_end_groups)
        EndGroup = true;
}

path GNULinker::getOutputFile() const
{
    return Output;
}

path GNULinker::getImportLibrary() const
{
    //if (ImportLibrary)
        //return ImportLibrary();
    //path p = Output;
    //return p.parent_path() / (p.filename().stem() += ".a");
    return Output;
}

void GNULinker::getAdditionalOptions(driver::Command *cmd) const
{
    getCommandLineOptions<GNULinkerOptions>(cmd, *this);
}

void GNULinker::prepareCommand1(const Target &t)
{
    // can be zero imput files actually: lib.exe /DEF:my.def /OUT:x.lib
    //if (InputFiles().empty())
        //return nullptr;

    if (t.getBuildSettings().TargetOS.isApple())
    {
        for (auto &f : NativeLinkerOptions::Frameworks)
            ((GNULinker *)this)->GNULinkerOptions::Frameworks().push_back(f);
        for (auto &f : NativeLinkerOptions::System.Frameworks)
            ((GNULinker *)this)->GNULinkerOptions::Frameworks().push_back(f);
    }

    ((GNULinker*)this)->GNULinkerOptions::LinkDirectories = gatherLinkDirectories();
    //((GNULinker*)this)->GNULinkerOptions::LinkLibraries() = gatherLinkLibraries();
    ((GNULinker*)this)->GNULinkerOptions::SystemLinkLibraries = gatherLinkLibraries(true);

    //if (t.getSolution().getHostOs().is(OSType::Windows))
    {
        // lld will add windows absolute paths to libraries
        //
        //  ldd -d test-0.0.1
        //      linux-vdso.so.1 (0x00007ffff724c000)
        //      D:\temp\9\musl\.sw\linux_x86_64_clang_9.0_shared_Release\musl-1.1.21.so => not found
        //      D:\temp\9\musl\.sw\linux_x86_64_clang_9.0_shared_Release\compiler_rt.builtins-0.0.1.so => not found
        //
        // so we strip abs paths and pass them to -L

        UniqueVector<path> dirs;
        auto &origin_dirs = GNULinkerOptions::LinkDirectories();
        for (auto &d : origin_dirs)
            dirs.push_back(d);

        auto update_libs = [&dirs, this](auto &a, bool add_inputs = false, bool sys = false)
        {
            for (auto &ll : a)
            {
                if (ll.l.is_relative())
                    continue;
                if (add_inputs)
                    cmd->addInput(ll.l);
                if (ll.whole_archive && ll.style == ll.AppleLD)
                    continue; // on whole archive + apple ld we do not change path

                // may be set earlier
                if (ll.static_)
                    continue;

                // if comes from saved config
                // more reliable condition?
                if (ll.l.extension() == ".a")
                {
                    ll.static_ = true;
                    continue;
                }

                //
                dirs.insert(ll.l.parent_path());

                if (sys)
                    ll.l = remove_prefix_and_suffix(ll.l);
                else
                    ll.l = remove_prefix_and_suffix(ll.l.filename());
            }
        };

        // we also now provide manual handling of input files

        update_libs(NativeLinker::LinkLibraries);
        update_libs(NativeLinker::System.LinkLibraries, false, true);
        update_libs(GNULinkerOptions::InputLibraryDependencies(), true);
        update_libs(GNULinkerOptions::LinkLibraries(), true);
        update_libs(GNULinkerOptions::SystemLinkLibraries(), false, true);

        GNULinkerOptions::InputLibraryDependencies.input_dependency = false;
        GNULinkerOptions::LinkLibraries.input_dependency = false;

        origin_dirs.clear();
        for (auto &d : dirs)
            origin_dirs.push_back(d);

        // remove later?
        //cmd->arguments.push_back("-rpath");
        //cmd->arguments.push_back("./");
    }

    //cmd->out.capture = true;
    //cmd->base = clone();
    if (Output)
    {
        cmd->working_directory = Output().parent_path();
        cmd->name = to_string(normalize_path(Output()));
        cmd->name_short = to_string(Output().filename().u8string());
    }

    getCommandLineOptions<GNULinkerOptions>(cmd.get(), *this);
    addEverything(*cmd);
    //getAdditionalOptions(cmd.get());

    if (t.isReproducibleBuild())
    {
        cmd->environment["ZERO_AR_DATE"] = "1";
    }
}

SW_DEFINE_PROGRAM_CLONE(GNULibrarian)

void GNULibrarian::setObjectFiles(const FilesOrdered &files)
{
    InputFiles().insert(InputFiles().end(), files.begin(), files.end());
}

void GNULibrarian::setOutputFile(const path &out)
{
    Output = add_prefix_and_suffix(out, Prefix, Extension).u8string();
}

void GNULibrarian::setImportLibrary(const path &out)
{
    //ImportLibrary = out.u8string();// + ".lib";
}

path GNULibrarian::getOutputFile() const
{
    return Output;
}

path GNULibrarian::getImportLibrary() const
{
    //if (ImportLibrary)
        //return ImportLibrary();
    path p = Output;
    return p.parent_path() / (p.filename().stem() += ".a");
}

void GNULibrarian::getAdditionalOptions(driver::Command *cmd) const
{
    getCommandLineOptions<GNULibrarianOptions>(cmd, *this);
}

void GNULibrarian::prepareCommand1(const Target &t)
{
    // these's some issue with archives not recreated, but keeping old symbols
    // TODO: investigate, fix and remove?
    cmd->remove_outputs_before_execution = true;

    //if (t.getSolution().getHostOs().isApple() || t.getBuildSettings().TargetOS.isApple())
        //cmd->use_response_files = false;

    // can be zero imput files actually: lib.exe /DEF:my.def /OUT:x.lib
    //if (InputFiles().empty())
        //return nullptr;

    //LinkDirectories() = gatherLinkDirectories();
    //LinkLibraries() = gatherLinkLibraries();

    //cmd->out.capture = true;
    //cmd->base = clone();
    if (Output)
    {
        cmd->working_directory = Output().parent_path();
        cmd->name = to_string(normalize_path(Output()));
        cmd->name_short = to_string(Output().filename().u8string());
    }

    //((GNULibraryTool*)this)->GNULibraryToolOptions::LinkDirectories() = gatherLinkDirectories();

    getCommandLineOptions<GNULibrarianOptions>(cmd.get(), *this);
    //addEverything(*cmd); // actually librarian does not need LINK options
    //getAdditionalOptions(cmd.get());

    if (t.isReproducibleBuild())
    {
        cmd->environment["ZERO_AR_DATE"] = "1";
    }
}

SW_DEFINE_PROGRAM_CLONE(RcTool)

void RcTool::prepareCommand1(const Target &t)
{
    //
    // https://docs.microsoft.com/en-us/windows/win32/menurc/resource-compiler
    // What we know:
    // - rc can use .rsp files
    // - include dirs with spaces cannot be placed into rsp and must go into INCLUDE env var
    //   ms bug: https://developercommunity.visualstudio.com/content/problem/417189/rcexe-incorrect-behavior-with.html
    // - we do not need to protect args with quotes: "-Dsomevar" - not needed
    // - definition value MUST be escaped: -DKEY="VALUE" because of possible spaces ' ' and braces '(', ')'
    // - include dir without spaces MUST NOT be escaped: -IC:/SOME/DIR
    //

    cmd->protect_args_with_quotes = false;

    if (InputFile)
    {
        cmd->name = to_string(normalize_path(InputFile()));
        cmd->name_short = to_string(InputFile().filename().u8string());
    }

    // defs
    auto print_def = [&c = *cmd](const auto &a)
    {
        for (auto &[k,v] : a)
        {
            if (v.empty())
                c.arguments.push_back("-D" + k);
            else
            {
                String s = "-D" + k + "=";
                auto v2 = v.toString();
                auto has_spaces = true;
                // new win sdk contains rc.exe that can work without quotes around def values
                // we should check rc version here, if it > winsdk 10.19041, then run the following line
                has_spaces = std::find(v2.begin(), v2.end(), ' ') != v2.end();
                // some targets gives def values with spaces
                // like pcre 'SW_PCRE_EXP_VAR=extern __declspec(dllimport)'
                // in this case we protect the value with quotes
                if (has_spaces && v2[0] != '\"')
                    s += "\"";
                s += v2;
                if (has_spaces && v2[0] != '\"')
                    s += "\"";
                c.arguments.push_back(s);
            }
        }
    };

    print_def(t.template as<NativeCompiledTarget>().getMergeObject().NativeCompilerOptions::Definitions);
    print_def(t.template as<NativeCompiledTarget>().getMergeObject().NativeCompilerOptions::System.Definitions);

    // idirs
    Strings env_idirs;
    for (auto &d : t.template as<NativeCompiledTarget>().getMergeObject().NativeCompilerOptions::gatherIncludeDirectories())
    {
        auto i = to_string(normalize_path(d));
        if (i.find(' ') != i.npos)
            env_idirs.push_back(i);
        else
            cmd->arguments.push_back("-I" + i);
    }

    // use env
    String s;
    // it is ok when INCLUDE is empty, do not check for it!
    for (auto &i : env_idirs)
        s += i + ";";
    for (auto &i : idirs)
        s += to_string(normalize_path(i)) + ";";
    cmd->environment["INCLUDE"] = s;

    getCommandLineOptions<RcToolOptions>(cmd.get(), *this);
}

void RcTool::setOutputFile(const path &output_file)
{
    Output = output_file;
}

void RcTool::setSourceFile(const path &input_file)
{
    InputFile = input_file;
}

std::shared_ptr<SourceFile> RcTool::createSourceFile(const Target &t, const path &input) const
{
    return std::make_shared<RcToolSourceFile>(*this, input, ::sw::getOutputFile(t, *this, input));
}

SW_DEFINE_PROGRAM_CLONE(AdaCompiler)

void AdaCompiler::prepareCommand1(const ::sw::Target &t)
{
    getCommandLineOptions<AdaCompilerOptions>(cmd.get(), *this);
}

void AdaCompiler::setOutputFile(const path &output_file)
{
    Output = output_file;
    Output() += Extension;
}

void AdaCompiler::addSourceFile(const path &input_file)
{
    InputFiles().push_back(input_file);
}

SW_DEFINE_PROGRAM_CLONE(VisualStudioCSharpCompiler)

void VisualStudioCSharpCompiler::prepareCommand1(const ::sw::Target &t)
{
    getCommandLineOptions<VisualStudioCSharpCompilerOptions>(cmd.get(), *this);
}

void VisualStudioCSharpCompiler::setOutputFile(const path &output_file)
{
    Output = output_file;
    Output() += Extension;
}

void VisualStudioCSharpCompiler::addSourceFile(const path &input_file)
{
    InputFiles().push_back(input_file);
}

SW_DEFINE_PROGRAM_CLONE(RustCompiler)

void RustCompiler::prepareCommand1(const Target &t)
{
    getCommandLineOptions<RustCompilerOptions>(cmd.get(), *this);
}

void RustCompiler::setOutputFile(const path &output_file)
{
    Output = output_file;
    Output() += Extension;
}

void RustCompiler::setSourceFile(const path &input_file)
{
    InputFile() = input_file;
}

SW_DEFINE_PROGRAM_CLONE(GoCompiler)

void GoCompiler::prepareCommand1(const Target &t)
{
    getCommandLineOptions<GoCompilerOptions>(cmd.get(), *this);
}

void GoCompiler::setOutputFile(const path &output_file)
{
    Output = output_file;
    Output() += Extension;
}

void GoCompiler::setSourceFile(const path &input_file)
{
    InputFiles().push_back(input_file);
}

SW_DEFINE_PROGRAM_CLONE(FortranCompiler)

void FortranCompiler::prepareCommand1(const Target &t)
{
    getCommandLineOptions<FortranCompilerOptions>(cmd.get(), *this);
}

void FortranCompiler::setOutputFile(const path &output_file)
{
    Output = output_file;
    Output() += Extension;
}

void FortranCompiler::setSourceFile(const path &input_file)
{
    InputFiles().push_back(input_file);
}

SW_DEFINE_PROGRAM_CLONE(JavaCompiler)

void JavaCompiler::prepareCommand1(const Target &t)
{
    getCommandLineOptions<JavaCompilerOptions>(cmd.get(), *this);

    for (auto &f : InputFiles())
    {
        auto o = OutputDir() / (f.filename().stem() += ".class");
        cmd->addOutput(o);
    }
}

void JavaCompiler::setOutputDir(const path &output_dir)
{
    OutputDir = output_dir;
}

void JavaCompiler::setSourceFile(const path &input_file)
{
    InputFiles().push_back(input_file);
}

SW_DEFINE_PROGRAM_CLONE(KotlinCompiler)

void KotlinCompiler::prepareCommand1(const Target &t)
{
    getCommandLineOptions<KotlinCompilerOptions>(cmd.get(), *this);
}

void KotlinCompiler::setOutputFile(const path &output_file)
{
    Output = output_file;
    Output() += ".jar";
}

void KotlinCompiler::setSourceFile(const path &input_file)
{
    InputFiles().push_back(input_file);
}

SW_DEFINE_PROGRAM_CLONE(DCompiler)

void DCompiler::prepareCommand1(const Target &t)
{
    getCommandLineOptions<DLinkerOptions>(cmd.get(), *this);
}

void DCompiler::setObjectDir(const path &output_dir)
{
    ObjectDir = output_dir;
}

path DCompiler::getOutputFile() const
{
    return Output();
}

void DCompiler::setOutputFile(const path &output_file)
{
    Output = output_file;
    Output() += Extension;
}

void DCompiler::setSourceFile(const path &input_file)
{
    InputFiles().push_back(input_file);
}

SW_DEFINE_PROGRAM_CLONE(PascalCompiler)

void PascalCompiler::prepareCommand1(const ::sw::Target &t)
{
    getCommandLineOptions<PascalCompilerOptions>(cmd.get(), *this);
}

void PascalCompiler::setOutputFile(const path &output_file)
{
    Output = output_file;
    Output() += Extension;
}

void PascalCompiler::addSourceFile(const path &input_file)
{
    InputFiles().push_back(input_file);
}

SW_DEFINE_PROGRAM_CLONE(ValaCompiler)

void ValaCompiler::prepareCommand1(const Target &t)
{
    sw::getCommandLineOptions<ValaOptions>(cmd.get(), *this);
}

}
