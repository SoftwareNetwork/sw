// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "generator.h"
#include "context.h"

#include "sw/driver/command.h"
#include "sw/driver/compiler.h"
#include "sw/driver/compiler_helpers.h"
#include "sw/driver/build.h"
#include "sw/driver/sw_context.h"
#include "sw/driver/target/native.h"

#include <sw/builder/execution_plan.h>
#include <sw/support/filesystem.h>

#include <primitives/sw/cl.h>
#include <primitives/win32helpers.h>

#include <boost/algorithm/string.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <nlohmann/json.hpp>

#include <sstream>
#include <stack>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "generator.vs");

//extern cl::SubCommand subcommand_ide;
bool gPrintDependencies;
bool gPrintOverriddenDependencies;
bool gOutputNoConfigSubdir;

static cl::opt<String> toolset("toolset", cl::desc("Set VS generator toolset"));

extern std::map<sw::PackagePath, sw::Version> gUserSelectedPackages;
static String vs_project_ext = ".vcxproj";

namespace sw
{

static int vsVersionFromString(const String &s)
{
	String t;
	for (auto c : s)
	{
		if (isdigit(c))
			t += c;
	}
	if (t.empty())
		return 0;
	auto v = std::stoi(t);
	if (t.size() == 4)
	{
		switch (v)
		{
            // 2003
        case 2005:
            return 8;
        case 2008:
            return 9;
		case 2010:
			return 10;
		case 2012:
			return 11;
		case 2013:
			return 12;
		case 2015:
			return 14;
		case 2017:
			return 15;
		case 2019:
			return 16;
		}
	}
	else if (t.size() == 2)
	{
		return v;
	}
	throw SW_RUNTIME_ERROR("Unknown or bad VS version: " + t);
}

std::string getVsToolset(const Version &v)
{
    switch (v.getMajor())
    {
    case 16:
        return "v142";
    case 15:
        return "v141";
    case 14:
        return "v14";
    case 12:
        return "v12";
    case 11:
        return "v11";
    case 10:
        return "v10";
    case 9:
        return "v9";
    case 8:
        return "v8";
        // _xp?
        // v71?
    }
    throw SW_RUNTIME_ERROR("Unknown VS version");
}

std::unique_ptr<Generator> Generator::create(const String &s)
{
    auto t = fromString(s);
    std::unique_ptr<Generator> g;
    switch (t)
    {
    case GeneratorType::VisualStudio:
    case GeneratorType::VisualStudioNMake:
    case GeneratorType::VisualStudioUtility:
    case GeneratorType::VisualStudioNMakeAndUtility:
	{
		auto g1 = std::make_unique<VSGenerator>();
		g1->version = Version(vsVersionFromString(s));
		g = std::move(g1);
		break;
	}
    case GeneratorType::Ninja:
        g = std::make_unique<NinjaGenerator>();
        break;
    case GeneratorType::NMake:
    case GeneratorType::Make:
        g = std::make_unique<MakeGenerator>();
        break;
    case GeneratorType::Batch:
        g = std::make_unique<BatchGenerator>();
        break;
    case GeneratorType::Shell:
        g = std::make_unique<ShellGenerator>();
        break;
    case GeneratorType::CompilationDatabase:
        g = std::make_unique<CompilationDatabaseGenerator>();
        break;
    default:
        throw std::logic_error("not implemented");
    }
    g->type = t;
    return g;
}

void Generator::generate(const path &f, const Build &b)
{
    file = f;
    generate(b);
}

String uuid2string(const boost::uuids::uuid &u)
{
    std::ostringstream ss;
    ss << u;
    return boost::to_upper_copy(ss.str());
}

String make_backslashes(String s)
{
    std::replace(s.begin(), s.end(), '/', '\\');
    return s;
}

static const std::map<ArchType, String> platforms{
    {ArchType::x86,"Win32",},
    {ArchType::x86_64,"x64",},
    {ArchType::arm,"ARM",},
    {ArchType::aarch64,"ARM64",},
};

// vsgen
namespace generator
{

static String toString(ConfigurationType t)
{
    switch (t)
    {
    case ConfigurationType::Debug:
        return "Debug";
    case ConfigurationType::Release:
        return "Release";
    case ConfigurationType::MinimalSizeRelease:
        return "MinSizeRel";
    case ConfigurationType::ReleaseWithDebugInformation:
        return "RelWithDebInfo";
    default:
        throw SW_RUNTIME_ERROR("no such config");
    }
}

static String toString(ArchType t)
{
    auto i = platforms.find(t);
    if (i == platforms.end())
        return generator::toString(ArchType::x86); // return dummy default
        //throw SW_RUNTIME_ERROR("no such platform");
    return i->second;
}

static String toString(LibraryType t)
{
    switch (t)
    {
    case LibraryType::Static:
        return "Static";
    case LibraryType::Shared:
        return "Dll";
    default:
        throw SW_RUNTIME_ERROR("no such lib type");
    }
}

}

static std::map<VSProjectType, String> project_type_uuids{
    {VSProjectType::Directory,"{2150E333-8FDC-42A3-9474-1A3956D46DE8}",},

    // other?
    {VSProjectType::Makefile,"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}",},
    {VSProjectType::Application,"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}",},
    {VSProjectType::DynamicLibrary,"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}",},
    {VSProjectType::StaticLibrary,"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}",},
    {VSProjectType::Utility,"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}",},
};

static String add_space_if_not_empty(const String &s)
{
    if (s.empty())
        return {};
    return " " + s;
}

static String get_configuration(const BuildSettings &s)
{
    String c = generator::toString(s.Native.ConfigurationType) + generator::toString(s.Native.LibrariesType);
    if (s.Native.MT)
        c += "Mt";
    return c;
}

static String get_project_configuration(const BuildSettings &s)
{
    String c;
    c += get_configuration(s);
    if (platforms.find(s.TargetOS.Arch) == platforms.end())
        c += " - " + toString(s.TargetOS.Arch);
    c += "|" + generator::toString(s.TargetOS.Arch);
    return c;
}

static path get_out_dir(const path &dir, const path &projects_dir, const BuildSettings &s)
{
    auto p = fs::current_path();
    p /= "bin";
    if (!gOutputNoConfigSubdir)
        p /= get_configuration(s);
    return p;
}

static std::pair<String, String> get_project_configuration_pair(const BuildSettings &s)
{
    return { "Condition", "'$(Configuration)|$(Platform)'=='" + get_project_configuration(s) + "'" };
}

enum class VSFileType
{
    None,
    ResourceCompile,
    CustomBuild,
    ClInclude,
    ClCompile,
    MASM,
    Manifest,
};

static VSFileType get_vs_file_type_by_ext(const path &p)
{
    if (p.extension() == ".rc")
        return VSFileType::ResourceCompile;
    else if (p.extension() == ".rule")
        return VSFileType::CustomBuild;
    else if (isCppHeaderFileExtension(p.extension().string()))
        return VSFileType::ClInclude;
    else if (isCppSourceFileExtensions(p.extension().string()) || p.extension() == ".c")
        return VSFileType::ClCompile;
    else if (p.extension() == ".asm")
        return VSFileType::MASM;
    else if (p.extension() == ".manifest")
        return VSFileType::Manifest;
    return VSFileType::None;
}

String toString(VSFileType t)
{
    switch (t)
    {
    case VSFileType::ClCompile:
        return "ClCompile";
    case VSFileType::ClInclude:
        return "ClInclude";
    case VSFileType::ResourceCompile:
        return "ResourceCompile";
    case VSFileType::CustomBuild:
        return "CustomBuild";
    case VSFileType::MASM:
        return "MASM";
    case VSFileType::Manifest:
        return "Manifest";
    default:
        return "None";
    }
}

static VSProjectType get_vs_project_type(const BuildSettings &s, const Target &t)
{
    if (auto nt = t.as<NativeCompiledTarget>())
    {
        if (!nt->getCommand())
            return VSProjectType::Utility;
    }
    switch (t.getType())
    {
    case TargetType::NativeLibrary:
        if (s.Native.LibrariesType == LibraryType::Shared)
            return VSProjectType::DynamicLibrary;
        else
            return VSProjectType::StaticLibrary;
        break;
    case TargetType::NativeExecutable:
        return VSProjectType::Application;
        break;
    case TargetType::NativeSharedLibrary:
        return VSProjectType::DynamicLibrary;
        break;
    case TargetType::NativeStaticLibrary:
        return VSProjectType::StaticLibrary;
        break;
    default:
        throw SW_RUNTIME_ERROR("bad type");
        break;
    }
}

static path get_int_dir(const path &dir, const path &projects_dir, const String &name)
{
    auto tdir = dir / projects_dir;
    return tdir / "i" / shorten_hash(blake2b_512(name), 6);
}

static path get_int_dir(const path &dir, const path &projects_dir, const String &name, const BuildSettings &s)
{
    return get_int_dir(dir, projects_dir, name) / shorten_hash(blake2b_512(get_project_configuration(s)), 6);
}

XmlEmitter::XmlEmitter(bool print_version)
    : Emitter("  ")
{
    if (print_version)
        addLine(R"(<?xml version="1.0" encoding="utf-8"?>)");
}

void XmlEmitter::beginBlock(const String &n, const std::map<String, String> &params, bool empty)
{
    beginBlock1(n, params, empty);
    increaseIndent();
}

void XmlEmitter::beginBlockWithConfiguration(const String &n, const BuildSettings &s, std::map<String, String> params, bool empty)
{
    params.insert(get_project_configuration_pair(s));
    beginBlock(n, params, empty);
}

void XmlEmitter::endBlock(bool text)
{
    decreaseIndent();
    endBlock1(text);
}

void XmlEmitter::addBlock(const String &n, const String &v, const std::map<String, String> &params)
{
    beginBlock1(n, params, v.empty());
    if (v.empty())
        return;
    addText(v);
    endBlock1(true);
}

void XmlEmitter::beginBlock1(const String &n, const std::map<String, String> &params, bool empty)
{
    blocks.push(n);
    addLine("<" + blocks.top());
    for (auto &[k, v] : params)
        addText(" " + k + "=\"" + v + "\"");
    if (empty)
        addText(" /");
    addText(">");
    if (empty)
        blocks.pop();
}

void XmlEmitter::endBlock1(bool text)
{
    if (text)
        addText("</" + blocks.top() + ">");
    else
        addLine("</" + blocks.top() + ">");
    blocks.pop();
}

void FiltersEmitter::beginProject()
{
    beginBlock("Project", { {"ToolsVersion", "4.0"},
                            {"xmlns", "http://schemas.microsoft.com/developer/msbuild/2003"} });
}

void FiltersEmitter::endProject()
{
    endBlock();
}

void ProjectEmitter::beginProject()
{
    beginBlock("Project", { {"DefaultTargets", "Build"},
                            {"ToolsVersion", std::to_string(parent->version.getMajor()) + ".0"},
                            {"xmlns", "http://schemas.microsoft.com/developer/msbuild/2003"} });
}

void ProjectEmitter::endProject()
{
    endBlock();
}

void ProjectEmitter::addProjectConfigurations(const Build &b)
{
    beginBlock("ItemGroup", { {"Label","ProjectConfigurations"} });
    for (auto &s : b.settings)
    {
        beginBlock("ProjectConfiguration", { {"Include", get_project_configuration(s) } });
        addBlock("Configuration", get_configuration(s));
        addBlock("Platform", generator::toString(s.TargetOS.Arch));
        endBlock();
    }
    endBlock();
}

void ProjectEmitter::addConfigurationType(VSProjectType t)
{
    switch (t)
    {
    case VSProjectType::Makefile:
        addBlock("ConfigurationType", "Makefile");
        break;
    case VSProjectType::Utility:
        addBlock("ConfigurationType", "Utility");
        break;
    case VSProjectType::Application:
        addBlock("ConfigurationType", "Application");
        break;
    case VSProjectType::DynamicLibrary:
        addBlock("ConfigurationType", "DynamicLibrary");
        break;
    case VSProjectType::StaticLibrary:
        addBlock("ConfigurationType", "StaticLibrary");
        break;
    default:
        break;
    }
}

void ProjectEmitter::addPropertyGroupConfigurationTypes(const Build &b, VSProjectType t)
{
    for (auto &s : b.settings)
    {
        beginBlockWithConfiguration("PropertyGroup", s, {{ "Label","Configuration" } });
        addConfigurationType(t);
        //addBlock("UseDebugLibraries", generator::toString(s.Settings.Native.ConfigurationType));
		if (toolset.empty())
		{
            addBlock("PlatformToolset", getVsToolset(parent->version));
		}
		else
			addBlock("PlatformToolset", toolset);

        endBlock();
    }
}

void ProjectEmitter::addPropertyGroupConfigurationTypes(const Build &b)
{
    addPropertyGroupConfigurationTypes(b, ptype);
}

void ProjectEmitter::addPropertyGroupConfigurationTypes(const Build &b, const PackageId &p)
{
    for (auto &s : b.settings)
    {
        beginBlockWithConfiguration("PropertyGroup", s, { { "Label","Configuration" } });

        auto i = b.children.find(p);
        if (i == b.children.end())
            throw SW_RUNTIME_ERROR("bad target: " + p.toString());

        addConfigurationType(get_vs_project_type(s, *i->second.find(TargetSettings{ s })->second));

        //addBlock("UseDebugLibraries", generator::toString(s.Settings.Native.ConfigurationType));
		if (toolset.empty())
		{
            addBlock("PlatformToolset", getVsToolset(parent->version));
		}
		else
			addBlock("PlatformToolset", toolset);

        endBlock();
    }
}

void ProjectEmitter::addPropertySheets(const Build &b)
{
    for (auto &s : b.settings)
    {
        beginBlock("ImportGroup", { { "Condition", "'$(Configuration)|$(Platform)'=='" +
            get_project_configuration(s) + "'" },{ "Label","PropertySheets" } });
        addBlock("Import", "", {
            {"Project","$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props" },
            { "Condition","exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')" },
            { "Label","LocalAppDataPlatform" }, });
        endBlock();
    }
}

String getWin10KitDirName();

static bool shouldAddTarget(const Target &t)
{
    // now without overridden
    return 0
        || gPrintDependencies
        || t.isLocal()
        || (gPrintOverriddenDependencies && t.getPackage().getOverriddenDir())
        ;
}

void ProjectEmitter::printProject(
    const String &name, const PackageId &p, const Build &b, SolutionEmitter &ctx, Generator &g,
    PackagePathTree::Directories &parents, PackagePathTree::Directories &local_parents,
    const path &dir, const path &projects_dir
)
{
    beginProject();

    addProjectConfigurations(b);

    if (b.getChildren().find(p) == b.getChildren().end())
        throw SW_RUNTIME_ERROR("bad target");

    auto &t = *b.getChildren().find(p)->second.begin()->second;
    auto &base_nt = *t.as<NativeCompiledTarget>();

    // project name helper
    auto pp = p.ppath.parent();
    auto &prnts = t.Local ? local_parents : parents;
    while (!pp.empty() && prnts.find(pp) == prnts.end())
        pp = pp.parent();

    beginBlock("PropertyGroup", { {"Label", "Globals"} });
    addBlock("VCProjectVersion", std::to_string(ctx.version.getMajor()) + ".0");
    addBlock("ProjectGuid", "{" + ctx.uuids[name] + "}");
    addBlock("Keyword", "Win32Proj");
	if (ctx.version.getMajor() < 16)
		addBlock("WindowsTargetPlatformVersion", base_nt.getSettings().Native.SDK.getWindowsTargetPlatformVersion());
	else
	{
		auto v = base_nt.getSettings().Native.SDK.Version.u8string();
		if (v == getWin10KitDirName())
			addBlock("WindowsTargetPlatformVersion", v + ".0");
		else
			addBlock("WindowsTargetPlatformVersion", base_nt.getSettings().Native.SDK.getWindowsTargetPlatformVersion());
	}
    if (g.type == GeneratorType::VisualStudioNMakeAndUtility && ptype == VSProjectType::Makefile)
        addBlock("ProjectName", PackageId(p.ppath.slice(pp.size()), p.version).toString() + "-build");
    else
        addBlock("ProjectName", PackageId(p.ppath.slice(pp.size()), p.version).toString());
    addBlock("PreferredToolArchitecture", "x64"); // also x86
    endBlock();

    addBlock("Import", "", { {"Project", "$(VCTargetsPath)\\Microsoft.Cpp.Default.props"} });
    addPropertyGroupConfigurationTypes(b, p);
    addBlock("Import", "", { { "Project", "$(VCTargetsPath)\\Microsoft.Cpp.props" } });
    addPropertySheets(b);

    // make conditional if .asm files are present
    beginBlock("ImportGroup", { {"Label", "ExtensionSettings"} });
    addBlock("Import", "", { {"Project", "$(VCTargetsPath)\\BuildCustomizations\\masm.props"} });
    endBlock();
    beginBlock("ImportGroup", { {"Label", "ExtensionTargets"} });
    addBlock("Import", "", { {"Project", "$(VCTargetsPath)\\BuildCustomizations\\masm.targets"} });
    endBlock();

    auto get_int_dir = [&dir, &projects_dir](auto &nt, auto &s)
    {
        return ::sw::get_int_dir(dir, projects_dir, nt.getPackage().toString(), s);
    };

    auto add_excluded_from_build = [this, &b](auto &s)
    {
        for (auto &s2 : b.settings)
        {
            if (&s != &s2)
            {
                beginBlockWithConfiguration("ExcludedFromBuild", s2);
                addText("true");
                endBlock(true);
            }
        }
    };

    StringSet filters; // dirs
    FiltersEmitter fctx;
    fctx.beginProject();
    fctx.beginBlock("ItemGroup");

    bool add_sources =
        ptype == VSProjectType::Utility ||
        g.type == GeneratorType::VisualStudio ||
        g.type == GeneratorType::VisualStudioNMake
        ;

    Files files_added;
    for (auto &s : b.settings)
    {
        beginBlockWithConfiguration("PropertyGroup", s);

        auto &t = *b.getChildren().find(p)->second.find(TargetSettings{ s })->second;
        auto &nt = *t.as<NativeCompiledTarget>();

        auto o = nt.getOutputFile();

        String cfg = "--configuration " + toString(s.Native.ConfigurationType) + " --platform " + toString(s.TargetOS.Arch);
        if (s.Native.LibrariesType == LibraryType::Static)
            cfg += " --static";

        String compiler;
        /*if (s.Settings.Native.CompilerType == CompilerType::Clang)
            compiler = "--compiler clang";
        else if (s.Settings.Native.CompilerType == CompilerType::ClangCl)
            compiler = "--compiler clang-cl";
        else if (s.Settings.Native.CompilerType == CompilerType::GNU)
            compiler = "--compiler gnu";*/

        auto build_cmd = "sw -d " + normalize_path(b.config_file_or_dir) + " " + cfg + " " + compiler +
            " --do-not-rebuild-config" +
            " --target " + p.toString() + " ide";

        String defs;
        for (auto &[k, v] : nt.Definitions)
        {
            if (v.empty())
                defs += k + ";";
            else
                defs += k + "=" + v + ";";
        }

        String defs1;
        for (auto &[k, v] : nt.Definitions2)
        {
            if (v.empty())
                defs1 += k + ";";
            else
                defs1 += k + "=" + v + ";";
        }

        String idirs;
        String idirs1;
        for (auto &i : nt.gatherIncludeDirectories())
            idirs1 += i.string() + ";";
        idirs += idirs1;
        String add_opts;
        if (!nt.empty())
        {
            if (auto sf = std::dynamic_pointer_cast<NativeSourceFile>(nt.begin()->second); sf)
            {
                if (auto v = std::static_pointer_cast<VisualStudioCompiler>(sf->compiler); v)
                {
                    for (auto &i : v->gatherIncludeDirectories())
                        idirs += i.string() + ";";

                    auto cmd = std::make_shared<driver::Command>(b.swctx);
                    getCommandLineOptions<VisualStudioCompilerOptions>(cmd.get(), *v);
                    for (auto &a : cmd->arguments)
                        add_opts += a->toString() + " ";
                }
            }
        }

        if (g.type != GeneratorType::VisualStudio && ptype != VSProjectType::Utility)
        {
            addBlock("NMakeBuildCommandLine", build_cmd);
            addBlock("NMakeOutput", o.u8string());
            addBlock("NMakeCleanCommandLine", "sw -d " + normalize_path(b.config_file_or_dir) + " " + cfg + " ide --clean");
            addBlock("NMakeReBuildCommandLine", "sw -d " + normalize_path(b.config_file_or_dir) + " " + cfg + " " + compiler + " ide --rebuild");
            addBlock("NMakePreprocessorDefinitions", defs);
            addBlock("NMakeIncludeSearchPath", idirs);
            //addBlock("NMakeForcedIncludes", "Makefile");
            //addBlock("NMakeAssemblySearchPath", "Makefile");
            //addBlock("NMakeForcedUsingAssemblies", "Makefile");
            if (!add_opts.empty())
                addBlock("AdditionalOptions", add_opts);
        }

        endBlock();

        if (g.type == GeneratorType::VisualStudioNMake)
            return;

        beginBlockWithConfiguration("PropertyGroup", s);
        {
            addBlock("OutDir", normalize_path_windows(get_out_dir(dir, projects_dir, s)) + "\\");
            addBlock("IntDir", normalize_path_windows(get_int_dir(nt, s)) + "\\int\\");
            // full name of target, keep as is (it might have subdirs)
            addBlock("TargetName", nt.getPackage().toString());
            //addBlock("TargetExt", ext);
        }
        endBlock();

        // pre build event for utility
        if (g.type != GeneratorType::VisualStudio)
        {
            beginBlockWithConfiguration("ItemDefinitionGroup", s);
            beginBlock("PreBuildEvent");
            addBlock("Command", build_cmd);
            endBlock();
            endBlock();
        }

        // cl properties, make them like in usual VS project
        beginBlockWithConfiguration("ItemDefinitionGroup", s);
        beginBlock("ResourceCompile");
        addBlock("AdditionalIncludeDirectories", idirs1); // command line is too long
        addBlock("PreprocessorDefinitions", defs1); // command line is too long
        endBlock();

        beginBlock("ClCompile");
        addBlock("AdditionalIncludeDirectories", idirs);
        addBlock("PreprocessorDefinitions", defs);
        switch (nt.CPPVersion)
        {
        case CPPLanguageStandard::CPP17:
            addBlock("LanguageStandard", "stdcpp17");
            break;
        case CPPLanguageStandard::CPP20:
            addBlock("LanguageStandard", "stdcpplatest");
            break;
        }

        beginBlockWithConfiguration("AdditionalOptions", s);
        for (auto &o : nt.CompileOptions)
            addText(o + " ");
        endBlock(true);

        endBlock();
        endBlock();

        StringMap<String> replacements;
        if (g.type == GeneratorType::VisualStudio)
        {
            beginBlockWithConfiguration("ItemDefinitionGroup", s);

            beginBlock("ClCompile");
            auto sf = nt.gatherSourceFiles();
            // FIXME: now taking cl settings from just one file
            if (!sf.empty())
            {
                if (auto L = (*sf.begin())->compiler->as<VisualStudioCompiler>())
                {
                    L->printIdeSettings(*this);

                    // TODO: remove
                    beginBlock("RuntimeLibrary");
                    switch (L->RuntimeLibrary.value())
                    {
                    case vs::RuntimeLibraryType::MultiThreaded:
                        addText("MultiThreaded");
                        break;
                    case vs::RuntimeLibraryType::MultiThreadedDebug:
                        addText("MultiThreadedDebug");
                        break;
                    case vs::RuntimeLibraryType::MultiThreadedDLL:
                        addText("MultiThreadedDLL");
                        break;
                    case vs::RuntimeLibraryType::MultiThreadedDLLDebug:
                        addText("MultiThreadedDebugDLL");
                        break;
                    default:
                        throw SW_RUNTIME_ERROR("unreachable code");
                    }
                    endBlock(true);

                    beginBlockWithConfiguration("Optimization", s);
                    auto o = L->Optimizations.getCommandLine();
                    if (o[0] == "-Od")
                        addText("Disabled");
                    else if (o[0] == "-O1")
                        addText("MinSpace");
                    else if (o[0] == "-O2")
                        addText("MaxSpeed");
                    endBlock(true);
                }
            }
            endBlock();

            // export all symbols
            for (auto &[p, sf] : nt)
            {
                File ff(p, nt.getFs());
                auto gen = ff.getGenerator();

                if (auto dc = gen->as<ExecuteBuiltinCommand>())
                {
                    if (dc->arguments.size() > toIndex(driver::BuiltinCommandArgumentId::ArgumentKeyword) &&
                        dc->arguments[toIndex(driver::BuiltinCommandArgumentId::ArgumentKeyword)]->toString() == sw::builder::getInternalCallBuiltinFunctionName())
                    {
                        if (dc->arguments.size() > toIndex(driver::BuiltinCommandArgumentId::FunctionName) &&
                            dc->arguments[toIndex(driver::BuiltinCommandArgumentId::FunctionName)]->toString() == "sw_create_def_file")
                        {
                            beginBlock("PreLinkEvent");

                            Files filenames;
                            for (int i = toIndex(driver::BuiltinCommandArgumentId::FirstArgument) + 2; i < dc->arguments.size(); i++)
                            {
                                path f = dc->arguments[i]->toString();
                                auto fn = f.stem().stem().stem();
                                fn += f.extension();
                                if (filenames.find(fn) != filenames.end())
                                    fn = f.filename();
                                filenames.insert(fn);
                                dc->arguments[i] = std::make_unique<::primitives::command::SimpleArgument>(normalize_path(get_int_dir(nt, s) / "int" / fn.u8string()));
                            }

                            auto batch = get_int_dir(nt, s) / "commands" / std::to_string(gen->getHash());
                            batch = gen->writeCommand(batch);

                            beginBlock("Command");
                            // call batch files with 'call' command
                            // otherwise it won't run multiple custom commands, only the first one
                            // https://docs.microsoft.com/en-us/cpp/ide/specifying-custom-build-tools?view=vs-2017
                            addText("call \"" + normalize_path_windows(batch) + "\"");
                            endBlock(true);

                            beginBlock("Message");
                            //addText(gen->getName());
                            endBlock();

                            endBlock();
                        }
                    }
                }
            }

            // references does not work well with C++ projects
            // so link directly
            beginBlock("Link");

            if (nt.hasCircularDependency())
                addLine("<ImportLibrary />"); // no produced implib

            Files ll;

            std::set<void*> visited;
            std::function<void(NativeCompiledTarget&)> f;
            f = [&f, &dir, &projects_dir, &s, &visited, &t, &ll, this, &replacements](auto &nt)
            {
                if (visited.find(&nt) != visited.end())
                    return;
                visited.insert(&nt);

                for (auto &d : nt.Dependencies)
                {
                    if (d->IncludeDirectoriesOnly)
                        continue;
                    if (!d->target)
                        continue;
                    if (d->target->getPackage() == t.getPackage())
                        continue;
                    if (d->target->skip || d->target->sw_provided)
                        continue;

                    if (!shouldAddTarget(*d->target))
                    {
                        if (auto nt3 = d->target->template as<NativeCompiledTarget>())
                        {
                            if (d->target->getType() == TargetType::NativeExecutable)
                            {
                                if (d->target->Scope == TargetScope::Build)
                                {
                                    deps.insert(parent->build_dependencies_name);
                                    parent->build_deps.insert(d->target->getPackage());
                                }
                            }
                            else if (!*nt3->HeaderOnly)
                            {
                                ll.insert(nt3->getImportLibrary());
                                deps.insert(parent->build_dependencies_name);
                                parent->build_deps.insert(d->target->getPackage());

                                //if ((s.Settings.Native.LibrariesType == LibraryType::Static && d->target->getType() == TargetType::NativeLibrary) ||
                                    //d->target->getType() == TargetType::NativeStaticLibrary)
                                {
                                    f(*nt3);
                                }
                            }
                        }
                        continue;
                    }

                    // before dummy
                    if (auto nt3 = d->target->template as<NativeCompiledTarget>())
                    {
                        auto tdir = get_out_dir(dir, projects_dir, s);
                        tdir /= d->target->getPackage().toString() + ".exe";
                        replacements[normalize_path_windows(nt3->getOutputFile())] = normalize_path_windows(tdir);
                    }

                    if (d->isDisabledOrDummy())
                        continue;
                    if (d->target->skip || d->target->sw_provided)
                        continue;

                    deps.insert(d->target->getPackage().toString());

                    if (auto nt3 = d->target->template as<NativeCompiledTarget>())
                    {
                        if (!*nt3->HeaderOnly)
                        {
                            auto tdir = get_out_dir(dir, projects_dir, s);
                            tdir /= d->target->getPackage().toString() + ".lib";
                            ll.insert(tdir);
                        }
                    }

                    if ((s.Native.LibrariesType == LibraryType::Static && d->target->getType() == TargetType::NativeLibrary) ||
                        d->target->getType() == TargetType::NativeStaticLibrary)
                    {
                        if (auto nt3 = d->target->template as<NativeCompiledTarget>())
                        {
                            f(*nt3);
                        }
                    }
                }
            };

            f(nt);

            for (auto &l : nt.LinkLibraries2)
                ll.insert(l);
            for (auto &l : nt.NativeLinkerOptions::System.LinkLibraries)
                ll.insert(l);

            beginBlockWithConfiguration("AdditionalDependencies", s);
            for (auto &l : ll)
                addText(normalize_path_windows(l) + ";");
            addText("%(AdditionalDependencies)");
            endBlock(true);

            PathOptionsType ld;
            for (auto &l : nt.LinkDirectories)
                ld.insert(l);
            for (auto &l : nt.NativeLinkerOptions::System.LinkDirectories)
                ld.insert(l);

            beginBlockWithConfiguration("AdditionalLibraryDirectories", s);
            for (auto &l : ld)
                addText(normalize_path_windows(l) + ";");
            endBlock(true);

            if (auto c = nt.getSelectedTool())
            {
                if (auto L = c->as<VisualStudioLinker>())
                {
                    L->VisualStudioLibraryToolOptions::printIdeSettings(*this);
                    L->VisualStudioLinkerOptions::printIdeSettings(*this);
                }
            }

            beginBlockWithConfiguration("AdditionalOptions", s);
            for (auto &o : nt.LinkOptions)
                addText(o + " ");
            addText("%(AdditionalOptions)");
            endBlock(true);

            endBlock(); // Link

            //beginBlock("Manifest");
            //beginBlock("AdditionalManifestFiles");
            //endBlock(true);
            //endBlock();

            endBlock(); // ItemDefinitionGroup
        }

        if (add_sources)
        {
            beginBlock("ItemGroup");

            Files filenames;
            auto add_obj_file = [this, &s, &filenames](auto t, const path &p, auto &sf)
            {
                if (t != VSFileType::ClCompile)
                    return;

                // TODO: but when we turn on file back in IDE, what will happen there?
                if (sf->skip)
                    return;

                // VS disables /MP when it sees object filename
                // so we turn it on only for files with the same names
                if (filenames.find(p.filename()) == filenames.end())
                {
                    filenames.insert(p.filename());
                    return;
                }

                auto nsf = sf->as<NativeSourceFile>();
                if (!nsf)
                    return;

                //beginBlockWithConfiguration("ObjectFileName", s.Settings);
                beginBlock("ObjectFileName");
                addText("$(IntDir)/" + nsf->output.filename().u8string());
                endBlock(true);
            };

            Files rules;
            std::unordered_set<void *> cmds;
            auto generate_file = [
                this, &rules, &get_int_dir, &s, &nt,
                &cmds, &add_excluded_from_build, &replacements, &dir,
                &projects_dir, &filters, &fctx, &files_added, &add_obj_file]
            (const path &p, const std::shared_ptr<SourceFile> &sf)
            {
                File ff(p, nt.getFs());
                auto gen = ff.getGenerator();

                if (auto dc = gen->as<ExecuteBuiltinCommand>())
                {
                    if (dc->arguments.size() > toIndex(driver::BuiltinCommandArgumentId::ArgumentKeyword) &&
                        dc->arguments[toIndex(driver::BuiltinCommandArgumentId::ArgumentKeyword)]->toString() == sw::builder::getInternalCallBuiltinFunctionName())
                    {
                        if (dc->arguments.size() > toIndex(driver::BuiltinCommandArgumentId::FunctionName) &&
                            dc->arguments[toIndex(driver::BuiltinCommandArgumentId::FunctionName)]->toString() == "sw_create_def_file")
                        {
                            return;
                        }
                    }
                }

                auto rule = get_int_dir(nt, s) / "rules" / (p.filename().string() + ".rule");
                write_file_if_not_exists(rule, "");

                if (rules.find(rule) == rules.end() && cmds.find(gen.get()) == cmds.end())
                {
                    rules.insert(rule);
                    cmds.insert(gen.get());

                    // VS crash
                    // beginBlockWithConfiguration(get_vs_file_type_by_ext(rule), s.Settings, { {"Include", rule.string()} });
                    beginBlock(toString(get_vs_file_type_by_ext(rule)), { {"Include", rule.string()} });

                    add_excluded_from_build(s);

                    Files replacement_deps;
                    auto fix_strings = [&replacements, &replacement_deps](const String &s)
                    {
                        auto t = s;
                        for (auto &[k, v] : replacements)
                        {
                            auto prev = t;
                            boost::replace_all(t, k, v);
                            if (t != prev)
                                replacement_deps.insert(v);
                        }
                        return t;
                    };

                    beginBlockWithConfiguration("AdditionalInputs", s);
                    auto inputs = gen->inputs;
                    if (auto dc = gen->as<driver::Command>())
                    {
                        auto d = dc->dependency.lock();
                        if (d)
                        {
                            if (d->target && !d->target->skip && !d->target->sw_provided)
                            {
                                if (!shouldAddTarget(*d->target))
                                {
                                    deps.insert(parent->build_dependencies_name);
                                    parent->build_deps.insert(d->target->getPackage());
                                }
                                else
                                {
                                    auto tdir = get_out_dir(dir, projects_dir, s);
                                    tdir /= d->target->getPackage().toString() + ".exe";
                                    addText(normalize_path_windows(tdir) + ";");

                                    // remove old program dep
                                    inputs.erase(gen->getProgram());

                                    // fix program
                                    gen->setProgram(tdir); // remove this?

                                    deps.insert(d->target->getPackage().toString());
                                }
                            }
                        }
                    }
                    for (auto &o : inputs)
                        addText(normalize_path_windows(o) + ";");

                    // fix commands arguments, env etc.
                    for (auto &a : gen->arguments)
                        a = std::make_unique<::primitives::command::SimpleArgument>(fix_strings(a->toString()));
                    // and add new deps
                    for (auto &d : replacement_deps)
                        addText(normalize_path_windows(d) + ";");

                    endBlock(true);
                    if (!gen->outputs.empty() || gen->always)
                    {
                        beginBlockWithConfiguration("Outputs", s);
                        for (auto &o : gen->outputs)
                            addText(normalize_path_windows(o) + ";");
                        if (gen->always)
                        {
                            if (gen->outputs.empty())
                                throw SW_RUNTIME_ERROR("empty outputs");
                            path missing_file = *gen->outputs.begin();
                            missing_file += ".missing.file";
                            addText(normalize_path_windows(missing_file) + ";");
                        }
                        endBlock(true);
                    }

                    auto batch = get_int_dir(nt, s) / "commands" / std::to_string(gen->getHash());
                    batch = gen->writeCommand(batch);

                    beginBlockWithConfiguration("Command", s);
                    // call batch files with 'call' command
                    // otherwise it won't run multiple custom commands, only the first one
                    // https://docs.microsoft.com/en-us/cpp/ide/specifying-custom-build-tools?view=vs-2017
                    addText("call \"" + normalize_path_windows(batch) + "\"");
                    endBlock(true);

                    beginBlock("Message");
                    //addText(gen->getName());
                    endBlock();

                    endBlock();

                    auto filter = ". SW Rules";
                    filters.insert(filter);

                    fctx.beginBlock(toString(get_vs_file_type_by_ext(rule)), { {"Include", rule.string()} });
                    fctx.addBlock("Filter", make_backslashes(filter));
                    fctx.endBlock();
                }

                if (files_added.find(p) == files_added.end())
                {
                    files_added.insert(p);

                    auto t = get_vs_file_type_by_ext(p);
                    beginBlock(toString(t), { { "Include", p.string() } });
                    if (!sf || sf->skip)
                    {
                        beginBlock("ExcludedFromBuild");
                        addText("true");
                        endBlock(true);
                    }
                    else
                    {
                        add_excluded_from_build(s);
                        add_obj_file(t, p, sf);
                    }
                    endBlock();
                }
            };

            // not really working atm
            if (nt.hasCircularDependency())
            {
                std::cerr << "Target " << nt.getPackage().toString() << " has circular dependency, but it is not supported in IDE right now "
                    "(only console builds via 'sw build' are supported).\n";
                generate_file(*std::prev(nt.Librarian->getCommand()->outputs.end()), nullptr);
            }

            for (auto &[p, sf] : nt)
            {
                if (p.extension() == ".natvis")
                {
                    parent->visualizers.insert(p);
                    continue;
                }

                File ff(p, nt.getFs());
                //if (sf->skip && !ff.isGenerated())
                    //continue;
                if (g.type == GeneratorType::VisualStudio && ff.isGenerated())
                {
                    generate_file(p, sf);
                }
                else if (g.type == GeneratorType::VisualStudio && ff.isGeneratedAtAll())
                {
                    if (files_added.find(p) == files_added.end())
                    {
                        files_added.insert(p);

                        auto t = get_vs_file_type_by_ext(p);
                        beginBlock(toString(t), { { "Include", p.string() } });
                        add_excluded_from_build(s);
                        add_obj_file(t, p, sf);
                        if (sf->skip)
                        {
                            beginBlock("ExcludedFromBuild");
                            addText("true");
                            endBlock(true);
                        }
                        endBlock();
                    }
                }
                else
                {
                    if (files_added.find(p) == files_added.end())
                    {
                        files_added.insert(p);

                        auto t = get_vs_file_type_by_ext(p);
                        beginBlock(toString(t), { { "Include", p.string() } });
                        add_obj_file(t, p, sf);
                        if (sf->skip)
                        {
                            beginBlock("ExcludedFromBuild");
                            addText("true");
                            endBlock(true);
                        }
                        endBlock();
                    }
                }
            }

            endBlock();
        }
    }

    addBlock("Import", "", { { "Project", "$(VCTargetsPath)\\Microsoft.Cpp.targets" } });

    endProject();
    write_file_if_different(dir / projects_dir / (name + ".vcxproj"), getText());

    if (!add_sources)
        return;

    // filters
    files_added.clear();
    for (auto &s : b.settings)
    {
        auto &t = *b.getChildren().find(p)->second.find(TargetSettings{ s })->second;
        auto &nt = *t.as<NativeCompiledTarget>();

        auto sd = normalize_path(nt.SourceDir);
        auto bd = normalize_path(nt.BinaryDir);
        auto bdp = normalize_path(nt.BinaryPrivateDir);
        for (auto &[f, sf] : nt)
        {
            if (f.extension() == ".natvis")
            {
                parent->visualizers.insert(f);
                continue;
            }

            File ff(f, nt.getFs());
            //if (sf->skip && !ff.isGenerated())
                //continue;

            if (files_added.find(f) != files_added.end())
                continue;
            files_added.insert(f);

            String *d = nullptr;
            size_t p = 0;
            auto fd = normalize_path(f);

            auto calc = [&fd, &p, &d](auto &s)
            {
                auto p1 = fd.find(s);
                if (p1 != 0)
                    return;
                //if (p1 > p)
                {
                    p = s.size();
                    d = &s;
                }
            };

            calc(sd);
            calc(bd);
            calc(bdp);

            path filter;
            if (p != -1)
            {
                auto ss = fd.substr(p);
                if (ss[0] == '/')
                    ss = ss.substr(1);
                path r = ss;
                if (d == &sd)
                    r = "Source Files" / r;
                if (d == &bd)
                {
                    auto v = r;
                    r = "Generated Files";
                    r /= get_configuration(s);
                    r /= "Public" / v;
                }
                if (d == &bdp)
                {
                    auto v = r;
                    r = "Generated Files";
                    r /= get_configuration(s);
                    r /= "Private" / v;
                }
                do
                {
                    r = r.parent_path();
                    if (filter.empty())
                        filter = r;
                    filters.insert(r.string());
                } while (!r.empty() && r != r.root_path());
            }

            fctx.beginBlock(toString(get_vs_file_type_by_ext(f)), { {"Include", f.string()} });
            if (!filter.empty() && !filter.is_absolute())
                fctx.addBlock("Filter", make_backslashes(filter.string()));
            fctx.endBlock();
        }
    }
    filters.erase("");
    fctx.endBlock();

    fctx.beginBlock("ItemGroup");
    for (auto &f : filters)
    {
        fctx.beginBlock("Filter", { { "Include", make_backslashes(f) } });
        fctx.addBlock("UniqueIdentifier", "{" + uuid2string(boost::uuids::name_generator_sha1(boost::uuids::ns::oid())(make_backslashes(f))) + "}");
        fctx.endBlock();
    }
    fctx.endBlock();

    fctx.endProject();
    write_file_if_different(dir / projects_dir / (name + ".vcxproj.filters"), fctx.getText());
}

SolutionEmitter::SolutionEmitter()
    : Emitter("\t", "\r\n")
{
}

void SolutionEmitter::printVersion()
{
    addLine("Microsoft Visual Studio Solution File, Format Version 12.00");
	if (version.getMajor() == 15)
	{
		addLine("# Visual Studio " + std::to_string(version.getMajor()));
		addLine("VisualStudioVersion = 15.0.28010.2046");
	}
	else if (version.getMajor() == 16)
	{
		addLine("# Visual Studio Version " + std::to_string(version.getMajor()));
		addLine("VisualStudioVersion = 16.0.28606.126");
	}
	addLine("MinimumVisualStudioVersion = 10.0.40219.1");
}

SolutionEmitter &SolutionEmitter::addDirectory(const String &display_name)
{
    return addDirectory(display_name, display_name);
}

SolutionEmitter &SolutionEmitter::addDirectory(const InsecurePath &n, const String &display_name, const String &solution_dir)
{
    auto s = n.toString();
    auto up = boost::uuids::name_generator_sha1(boost::uuids::ns::oid())(s);
    uuids[s] = uuid2string(up);

    addLine("Project(\"" + project_type_uuids[VSProjectType::Directory] + "\") = \"" +
        display_name + "\", \"" + display_name + "\", \"{" + uuids[n] + "}\"");
    auto &e = addEmitter<SolutionEmitter>();
    addLine("EndProject");

    if (!solution_dir.empty())
        nested_projects[s] = solution_dir;
    return e;
}

SolutionEmitter::Project &SolutionEmitter::addProject(VSProjectType type, const String &n, const String &solution_dir)
{
    auto up = boost::uuids::name_generator_sha1(boost::uuids::ns::oid())(n);
    uuids[n] = uuid2string(up);

    projects[n].name = n;
    projects[n].pctx.parent = this;
    projects[n].pctx.ptype = type;
    projects[n].solution_dir = solution_dir;

    if (!solution_dir.empty())
        nested_projects[n] = solution_dir;

    return projects[n];
}

void SolutionEmitter::beginProject(VSProjectType type, const String &n, const path &dir, const String &solution_dir)
{
	bool has_dash = n.find("-") != n.npos;
	PackageId p(n);
    beginBlock("Project(\"" + project_type_uuids[type] + "\") = \"" +
        p.ppath.back() + (has_dash ? "-" + p.version.toString() : "") + "\", \"" + (dir / (n + vs_project_ext)).u8string() + "\", \"{" + uuids[n] + "}\"");

    projects[n].ctx = &addEmitter<SolutionEmitter>();

    if (!solution_dir.empty())
        nested_projects[n] = solution_dir;
}

void SolutionEmitter::endProject()
{
    endBlock("EndProject");
}

void SolutionEmitter::beginBlock(const String &s)
{
    addLine(s);
    increaseIndent();
}

void SolutionEmitter::endBlock(const String &s)
{
    decreaseIndent();
    addLine(s);
}

void SolutionEmitter::beginGlobal()
{
    beginBlock("Global");
}

void SolutionEmitter::endGlobal()
{
    printNestedProjects();

    endBlock("EndGlobal");
}

void SolutionEmitter::beginGlobalSection(const String &name, const String &post)
{
    beginBlock("GlobalSection(" + name + ") = " + post);
}

void SolutionEmitter::endGlobalSection()
{
    endBlock("EndGlobalSection");
}

struct less
{
	bool operator()(const String &s1, const String &s2) const
	{
		return boost::ilexicographical_compare(s1, s2);
	}
};

void SolutionEmitter::setSolutionConfigurationPlatforms(const Build &b)
{
	// sort like VS does
    beginGlobalSection("SolutionConfigurationPlatforms", "preSolution");
	std::set<String, less> platforms;
    for (auto &s : b.settings)
		platforms.insert(get_project_configuration(s) + " = " + get_project_configuration(s));
	for (auto &s : platforms)
		addLine(s);
    endGlobalSection();
}

void SolutionEmitter::addProjectConfigurationPlatforms(const Build &b, const String &prj, bool build)
{
	// sort like VS does
	std::map<String, String, less> platforms;
    for (auto &s : b.settings)
    {
		platforms[getStringUuid(prj) + "." + get_project_configuration(s) + ".ActiveCfg"] = get_project_configuration(s);
        if (build)
			platforms[getStringUuid(prj) + "." + get_project_configuration(s) + ".Build.0"] = get_project_configuration(s);
    }
	for (auto &[k,v] : platforms)
		addKeyValue(k, v);
}

void SolutionEmitter::beginProjectSection(const String &n, const String &disposition)
{
    beginBlock("ProjectSection(" + n + ") = " + disposition);
}

void SolutionEmitter::endProjectSection()
{
    endBlock("EndProjectSection");
}

void SolutionEmitter::addKeyValue(const String &k, const String &v)
{
    addLine(k + " = " + v);
}

String SolutionEmitter::getStringUuid(const String &k) const
{
    auto i = uuids.find(k);
    if (i == uuids.end())
        throw SW_RUNTIME_ERROR("No such uuid (project) - " + k + ". Check your invocation flags.");
    return "{" + i->second + "}";
}

void SolutionEmitter::materialize(const Build &b, const path &dir, GeneratorType type)
{
    auto bp = [&](const auto &n, const auto &p)
    {
        beginProject(p.pctx.ptype, n, dir, p.solution_dir);
        endProject();
    };

    if (first_project)
        bp(first_project->name, *first_project);

    for (auto &[n, p] : projects)
    {
        if (&p == first_project)
            continue;
        bp(n, p);
    }

    beginGlobal();
    setSolutionConfigurationPlatforms(b);
    beginGlobalSection("ProjectConfigurationPlatforms", "postSolution");
    for (auto &[p, tgts] : b.children)
    {
        auto &t = tgts.begin()->second;
        if (t->skip)
            continue;
        if (t->sw_provided)
            continue;
        if (b.skipTarget(t->Scope))
            continue;
        if (!shouldAddTarget(*t))
            continue;
        addProjectConfigurationPlatforms(b, p.toString(), type == GeneratorType::VisualStudio);
        if (projects.find(p.toString() + "-build") != projects.end())
            addProjectConfigurationPlatforms(b, p.toString() + "-build");
    }
    // we do not need it here
    if (type != GeneratorType::VisualStudio)
        addProjectConfigurationPlatforms(b, all_build_name, true);
    endGlobalSection();
    endGlobal();
}

SolutionEmitter::Text SolutionEmitter::getText() const
{
    for (auto &[n, p] : projects)
    {
        if (p.pctx.deps.empty())
            continue;
        p.ctx->beginProjectSection("ProjectDependencies", "postProject");
        for (auto &d : p.pctx.deps)
            p.ctx->addLine(getStringUuid(d) + " = " + getStringUuid(d));
        p.ctx->endProjectSection();
    }
    return Base::getText();
}

void SolutionEmitter::printNestedProjects()
{
    beginGlobalSection("NestedProjects", "preSolution");
    for (auto &[k,v]: nested_projects)
        addKeyValue(getStringUuid(k), getStringUuid(v));
    endGlobalSection();
}

void PackagePathTree::add(const PackagePath &p)
{
    if (p.empty())
        return;
    tree[p.slice(0, 1).toString()].add(p.slice(1));
}

PackagePathTree::Directories PackagePathTree::getDirectories(const PackagePath &p)
{
    Directories dirs;
    for (auto &[s, t] : tree)
    {
        auto dirs2 = t.getDirectories(p / s);
        dirs.insert(dirs2.begin(), dirs2.end());
    }
    if (tree.size() > 1 && !p.empty())
        dirs.insert(p);
    return dirs;
}

VSGenerator::VSGenerator()
{
    cwd = "\"" + current_thread_path().string() + "\"";
}

void VSGenerator::createSolutions(Build &b)
{
    //if (type == GeneratorType::VisualStudio)
        //b.Settings.Native.CompilerType = CompilerType::MSVC;

    for (auto p : {
             //ArchType::x86,
             ArchType::x86_64,
         })
    {
        auto ss = b.createSettings();
        ss.TargetOS.Arch = p;
        for (auto lt : {
                 //LibraryType::Static,
                 LibraryType::Shared,
             })
        {
            ss.Native.LibrariesType = lt;
            for (auto c : {
                     ConfigurationType::Debug,
                     ConfigurationType::Release,
                     //ConfigurationType::MinimalSizeRelease,
                     ConfigurationType::ReleaseWithDebugInformation,
                 })
            {
                ss.Native.ConfigurationType = c;
                b.addSettings(ss);
            }
        }
    }
}

void VSGenerator::initSolutions(Build &b)
{
    if (type != GeneratorType::VisualStudio)
        return;

    version = Version(16);

    /*for (auto &s : b.settings)
    {
        ProgramPtr prog;
        if (version.getMajor() == 0)
        {
            prog = s.getProgram("com.Microsoft.VisualStudio");
            if (!prog)
                throw SW_RUNTIME_ERROR("Program not found: com.Microsoft.VisualStudio");
        }
        else
        {
            PackageId pkg{ "com.Microsoft.VisualStudio", version };
            prog = s.getProgram(pkg, false);
            if (!prog)
                throw SW_RUNTIME_ERROR("Program not found: " + pkg.toString());
        }

        auto vs = prog->as<VSInstance>();
        if (!vs)
            throw SW_RUNTIME_ERROR("bad vs");

        version = vs->version;
        vs->activate(s);
    }*/
}

void VSGenerator::generate(const Build &b)
{
    dir = b.getIdeDir() / toPathString(type) / version.toString(1);
    PackagePathTree tree, local_tree, overridden_tree;
    PackagePathTree::Directories parents, local_parents;

    SolutionEmitter ctx;
	ctx.all_build_name = all_build_name;
	ctx.build_dependencies_name = build_dependencies_name;
	ctx.version = version;
	ctx.printVersion();

    ctx.addDirectory(predefined_targets_dir);
    auto &all_tgts_proj = ctx.addProject(type == GeneratorType::VisualStudio ? VSProjectType::Utility : VSProjectType::Makefile, all_build_name, predefined_targets_dir);

    // add ALL_BUILD target
    {
        auto &proj = all_tgts_proj;
        ctx.first_project = &proj;
        auto &pctx = proj.pctx;

        pctx.beginProject();

        pctx.addProjectConfigurations(b);

        pctx.beginBlock("PropertyGroup", { {"Label", "Globals"} });
        pctx.addBlock("VCProjectVersion", std::to_string(ctx.version.getMajor()) + ".0");
        pctx.addBlock("ProjectGuid", "{" + ctx.uuids[all_build_name] + "}");
        pctx.addBlock("Keyword", "Win32Proj");
        if (type != GeneratorType::VisualStudio)
            pctx.addBlock("ProjectName", all_build_name);
        else
        {
            pctx.addBlock("RootNamespace", all_build_name);
            pctx.addBlock("WindowsTargetPlatformVersion", b.getSettings().Native.SDK.getWindowsTargetPlatformVersion());
        }
        pctx.endBlock();

        pctx.addBlock("Import", "", { {"Project", "$(VCTargetsPath)\\Microsoft.Cpp.Default.props"} });
        pctx.addPropertyGroupConfigurationTypes(b);
        pctx.addBlock("Import", "", { { "Project", "$(VCTargetsPath)\\Microsoft.Cpp.props" } });
        pctx.addPropertySheets(b);

        if (type != GeneratorType::VisualStudio)
        {
            for (auto &s : b.settings)
            {
                using namespace sw;

                pctx.beginBlockWithConfiguration("PropertyGroup", s);

                String cfg = "--configuration " + generator::toString(s.Native.ConfigurationType) + " --platform " + generator::toString(s.TargetOS.Arch);
                if (s.Native.LibrariesType == LibraryType::Static)
                    cfg += " --static";
                if (s.Native.MT)
                    cfg += " --mt";

                String compiler;
                /*if (s.Native.CompilerType == CompilerType::Clang)
                    compiler = "--compiler clang";
                else if (s.Native.CompilerType == CompilerType::ClangCl)
                    compiler = "--compiler clang-cl";
                else if (s.Native.CompilerType == CompilerType::GNU)
                    compiler = "--compiler gnu";
                else if (s.Native.CompilerType == CompilerType::MSVC)*/
                    compiler = "--compiler msvc";

                pctx.addBlock("NMakeBuildCommandLine", "sw -d " + normalize_path(b.config_file_or_dir) + " " + cfg + " " + compiler +
                    " --do-not-rebuild-config" +
                    " ide");
                pctx.addBlock("NMakeCleanCommandLine", "sw -d " + normalize_path(b.config_file_or_dir) + " " + cfg + " ide --clean");
                pctx.addBlock("NMakeReBuildCommandLine", "sw -d " + normalize_path(b.config_file_or_dir) + " " + cfg + " " + compiler + " ide --rebuild");

                pctx.endBlock();
            }
        }
        else
        {
            for (auto &s : b.settings)
            {
                pctx.beginBlockWithConfiguration("PropertyGroup", s);
                pctx.addBlock("IntDir", normalize_path_windows(::sw::get_int_dir(dir, projects_dir, all_build_name, s)) + "\\int\\");
                pctx.endBlock();
            }
        }

        pctx.beginBlock("ItemGroup");
        pctx.beginBlock(toString(get_vs_file_type_by_ext(*b.config)), { { "Include", b.config->u8string() } });
        pctx.endBlock();
        pctx.endBlock();

        pctx.addBlock("Import", "", { { "Project", "$(VCTargetsPath)\\Microsoft.Cpp.targets" } });

        pctx.endProject();
        write_file_if_different(dir / projects_dir / (all_build_name + ".vcxproj"), pctx.getText());
    }

    // gather parents
    bool has_deps = false;
    bool has_overridden = false;
    // use only first
    for (auto &[p, tgts] : b.getChildren())
    {
        auto &t = tgts.begin()->second;
        if (t->skip)
            continue;
        if (t->sw_provided)
            continue;
        if (b.skipTarget(t->Scope))
            continue;
        if (!shouldAddTarget(*t))
            continue;
        has_deps |= !t->isLocal();
        if (t->getPackage().getOverriddenDir())
        {
            overridden_tree.add(p.ppath);
            has_overridden = true;
            //continue; // uncomment for overridden
        }
        (t->Local ? local_tree : tree).add(p.ppath);
    }
    if (has_deps && gPrintDependencies)
        ctx.addDirectory(deps_subdir);
    //if (has_overridden) // uncomment for overridden
        //ctx.addDirectory(overridden_deps_subdir);

    auto add_dirs = [&ctx](auto &t, auto &prnts, const String &root = {})
    {
        for (auto &p : prnts = t.getDirectories())
        {
            auto pp = p.parent();
            while (!pp.empty() && prnts.find(pp) == prnts.end())
                pp = pp.parent();
            ctx.addDirectory(InsecurePath() / p.toString(), p.slice(pp.size()), pp.empty() ? root : pp.toString());
        }
    };
    if (gPrintDependencies)
        add_dirs(tree, parents, deps_subdir.toString());
    //if (has_overridden) // uncomment for overridden
        //add_dirs(overridden_tree, parents, overridden_deps_subdir.toString());
    add_dirs(local_tree, local_parents);

    int n_executable_tgts = 0;
    for (auto &[p, tgts] : b.getChildren())
    {
        auto &t = tgts.begin()->second;
        if (t->skip)
            continue;
        if (t->sw_provided)
            continue;
        if (b.skipTarget(t->Scope))
            continue;
        if (!shouldAddTarget(*t))
            continue;
        if (t->isLocal() && isExecutable(t->getType()))
            n_executable_tgts++;
    }

    // use only first
    bool first_project_set = false;
    for (auto &[p, tgts] : b.getChildren())
    {
        auto &t = tgts.begin()->second;
        if (t->skip)
            continue;
        if (t->sw_provided)
            continue;
        if (b.skipTarget(t->Scope))
            continue;
        if (!shouldAddTarget(*t))
            continue;

        auto pp = p.ppath.parent();
        auto &prnts = t->Local ? local_parents : parents;
        while (!pp.empty() && prnts.find(pp) == prnts.end())
            pp = pp.parent();

        auto pps = pp.toString();
        //if (t->pkg.getOverriddenDir()) // uncomment for overridden
            //pps = overridden_deps_subdir / pps;

        auto t2 = VSProjectType::Makefile;
        if (type == GeneratorType::VisualStudio)
        {
            t2 = get_vs_project_type(b.getSettings(), *t);
        }
        else if (type != GeneratorType::VisualStudioNMake)
        {
            if (type == GeneratorType::VisualStudioNMakeAndUtility)
                ctx.addProject(t2, p.toString() + "-build", pps);
            t2 = VSProjectType::Utility;
        }
        auto &proj = ctx.addProject(t2, p.toString(), pps);
        if (!first_project_set)
        {
            auto nt = t->as<NativeCompiledTarget>();
            if ((nt && nt->StartupProject) || (t->isLocal() && isExecutable(t->getType()) && n_executable_tgts == 1))
            {
                ctx.first_project = &proj;
                first_project_set = true;
            }
        }

        if (type == GeneratorType::VisualStudio)
        {
            all_tgts_proj.pctx.deps.insert(p.toString());
        }
    }

    // gen projects
    // use only first
    for (auto &[p, tgts] : b.getChildren())
    {
        auto &t = tgts.begin()->second;
        if (t->skip)
            continue;
        if (t->sw_provided)
            continue;
        if (b.skipTarget(t->Scope))
            continue;
        if (!shouldAddTarget(*t))
            continue;

        auto nt = t->as<NativeCompiledTarget>();
        if (!nt)
            continue;

        Strings names = { p.toString() };
        if (type == GeneratorType::VisualStudio)
            ;
        else if (
            type != GeneratorType::VisualStudioNMake &&
            type == GeneratorType::VisualStudioNMakeAndUtility)
            names.push_back(p.toString() + "-build");
        for (auto &tn : names)
            ctx.projects[tn].pctx.printProject(tn, nt->getPackage(), b, ctx, *this,
                parents, local_parents,
                dir, projects_dir);
    }

    if (type == GeneratorType::VisualStudio && !ctx.build_deps.empty())
    {
        auto &proj = ctx.addProject(VSProjectType::Utility, build_dependencies_name, predefined_targets_dir);
        auto &pctx = proj.pctx;

        pctx.beginProject();

        pctx.addProjectConfigurations(b);

        pctx.beginBlock("PropertyGroup", { {"Label", "Globals"} });
        pctx.addBlock("VCProjectVersion", std::to_string(ctx.version.getMajor()) + ".0");
        pctx.addBlock("ProjectGuid", "{" + ctx.uuids[build_dependencies_name] + "}");
        pctx.addBlock("Keyword", "Win32Proj");
        if (type != GeneratorType::VisualStudio)
            pctx.addBlock("ProjectName", build_dependencies_name);
        else
        {
            pctx.addBlock("RootNamespace", build_dependencies_name);
            pctx.addBlock("WindowsTargetPlatformVersion", b.getSettings().Native.SDK.getWindowsTargetPlatformVersion());
        }
        pctx.endBlock();

        pctx.addBlock("Import", "", { {"Project", "$(VCTargetsPath)\\Microsoft.Cpp.Default.props"} });
        pctx.addPropertyGroupConfigurationTypes(b);
        pctx.addBlock("Import", "", { { "Project", "$(VCTargetsPath)\\Microsoft.Cpp.props" } });
        pctx.addPropertySheets(b);

        for (auto &s : b.settings)
        {
            auto int_dir = get_int_dir(dir, projects_dir, build_dependencies_name, s);

            pctx.beginBlockWithConfiguration("PropertyGroup", s);
            pctx.addBlock("IntDir", normalize_path_windows(int_dir) + "\\int\\");
            pctx.endBlock();

            Strings arguments;
            arguments.push_back("-configuration");
            arguments.push_back(generator::toString(s.Native.ConfigurationType));
            arguments.push_back("-platform");
            arguments.push_back(generator::toString(s.TargetOS.Arch));
            if (s.Native.LibrariesType == LibraryType::Static)
            arguments.push_back("-static");
            if (s.Native.MT)
            arguments.push_back("-mt");

            /*if (s.Settings.Native.CompilerType == CompilerType::Clang)
            {
                arguments.push_back("-compiler");
                arguments.push_back("clang");
            }
            else if (s.Settings.Native.CompilerType == CompilerType::ClangCl)
            {
                arguments.push_back("-compiler");
                arguments.push_back("clangcl");
            }
            else if (s.Settings.Native.CompilerType == CompilerType::GNU)
            {
                arguments.push_back("-compiler");
                arguments.push_back("gnu");
            }
            else if (s.Settings.Native.CompilerType == CompilerType::MSVC)*/
            {
                arguments.push_back("-compiler");
                arguments.push_back("msvc");
            }

            arguments.push_back("-d");
            arguments.push_back(normalize_path(b.config_file_or_dir));

            arguments.push_back("-activate");
            arguments.push_back(PackageId{ "com.Microsoft.VisualStudio", version }.toString());

            arguments.push_back("build");

            String deps;
            for (auto &p : ctx.build_deps)
            {
                deps += p.toString() + " ";
                arguments.push_back(p.toString());
            }

            //auto base = int_dir / "sw";
            auto base = int_dir / shorten_hash(blake2b_512(deps), 6);

            arguments.push_back("-ide-copy-to-dir");
            arguments.push_back(normalize_path(get_out_dir(dir, projects_dir, s)));

            auto fp = path(base) += ".deps";
            if (fs::exists(fp))
                fs::remove(fp);
            arguments.push_back("-ide-fast-path");
            arguments.push_back(normalize_path(fp));

            auto rsp = normalize_path(path(base) += ".rsp");
            String str;
            for (auto &a : arguments)
                str += a + "\n";
            write_file(rsp, str);

            pctx.beginBlockWithConfiguration("ItemDefinitionGroup", s);
            pctx.beginBlock("PreBuildEvent");
            pctx.addBlock("Command", "sw @" + rsp);
            pctx.endBlock();
            pctx.endBlock();
        }

        auto rule = get_int_dir(dir, projects_dir, build_dependencies_name) / "rules" / (build_dependencies_name + ".rule");
        write_file_if_not_exists(rule, "");

        pctx.beginBlock("ItemGroup");
        pctx.beginBlock(toString(get_vs_file_type_by_ext(rule)), { {"Include", rule.string()} });
        pctx.beginBlock("Outputs");
        pctx.addText(normalize_path_windows(rule.parent_path() / "intentionally_missing.file"));
        pctx.endBlock(true);
        pctx.beginBlock("Message");
        pctx.endBlock();
        pctx.beginBlock("Command");
        pctx.addText("setlocal");
        pctx.endBlock(true);
        pctx.endBlock();
        pctx.endBlock();

        pctx.addBlock("Import", "", { { "Project", "$(VCTargetsPath)\\Microsoft.Cpp.targets" } });

        pctx.endProject();
        write_file_if_different(dir / projects_dir / (build_dependencies_name + ".vcxproj"), pctx.getText());
    }

    if (!ctx.visualizers.empty())
    {
        auto &nvctx = ctx.addDirectory(visualizers_dir, visualizers_dir, predefined_targets_dir);
        nvctx.beginProjectSection("SolutionItems", "preProject");
        for (auto &v : ctx.visualizers)
            nvctx.addLine(normalize_path_windows(v) + " = " + normalize_path_windows(v));
        nvctx.endProjectSection();
    }

    ctx.materialize(b, projects_dir, type);

    //const auto compiler_name = boost::to_lower_copy(toString(b.solutions[0].Settings.Native.CompilerType));
    const String compiler_name = "msvc";
    String fn = b.ide_solution_name + "_";
    fn += compiler_name + "_" + toPathString(type) + "_" + version.toString(1);
    fn += ".sln";
    write_file_if_different(dir / fn, ctx.getText());
    auto lnk = current_thread_path() / fn;
    lnk += ".lnk";
    ::create_link(dir / fn, lnk, "SW link");
}

}
