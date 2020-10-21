// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>

#include "project_emitter.h"

#include "../generator.h"
#include "vs.h"

#include <sw/builder/os.h>
#include <sw/driver/build_settings.h>
#include <sw/driver/extensions.h>
#include <sw/driver/types.h>

#include <primitives/sw/cl.h>

using namespace sw;

static const std::map<ArchType, String> platforms
{
    {
        ArchType::x86,
        "Win32",
    },
    {
        ArchType::x86_64,
        "x64",
    },
    {
        ArchType::arm,
        "ARM",
    },
    {
        ArchType::aarch64,
        "ARM64",
    },
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

} // namespace generator

VSFileType get_vs_file_type_by_ext(const path &p)
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

std::string getVsToolset(const Version &clver)
{
    if (clver >= Version(19, 20))
        return "v142";
    if (clver >= Version(19, 10))
        return "v141";
    if (clver >= Version(19, 00))
        return "v140";

    throw SW_RUNTIME_ERROR("Unknown vs version (cl = " + clver.toString() + ")");
    /*case 12:
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
    throw SW_RUNTIME_ERROR("Unknown VS version");*/
}

String get_configuration(const BuildSettings &s)
{
    String c = generator::toString(s.Native.ConfigurationType) + generator::toString(s.Native.LibrariesType);
    if (s.Native.MT)
        c += "Mt";
    return c;
}

static std::pair<String, String> get_project_configuration_pair(const BuildSettings &s)
{
    return {"Condition", "'$(Configuration)|$(Platform)'=='" + get_project_configuration(s) + "'"};
}

String get_project_configuration(const BuildSettings &s)
{
    String c;
    c += get_configuration(s);
    if (platforms.find(s.TargetOS.Arch) == platforms.end())
        c += " - " + toString(s.TargetOS.Arch);
    c += "|" + generator::toString(s.TargetOS.Arch);
    return c;
}

XmlEmitter::XmlEmitter(bool print_version)
    : Emitter("  ", "\r\n")
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
    beginBlock("Project", {{"ToolsVersion", "4.0"},
        {"xmlns", "http://schemas.microsoft.com/developer/msbuild/2003"}});
}

void FiltersEmitter::endProject()
{
    endBlock();
}

void ProjectEmitter::beginProject(const sw::Version &version)
{
    beginBlock("Project", {{"DefaultTargets", "Build"},
        {"ToolsVersion", std::to_string(version.getMajor()) + ".0"},
        {"xmlns", "http://schemas.microsoft.com/developer/msbuild/2003"}});
}

void ProjectEmitter::endProject()
{
    endBlock();
}

void ProjectEmitter::addProjectConfigurations(const Project &p)
{
    beginBlock("ItemGroup", {{"Label", "ProjectConfigurations"}});
    for (auto &s : p.getSettings())
    {
        beginBlock("ProjectConfiguration", {{"Include", get_project_configuration(s)}});
        addBlock("Configuration", get_configuration(s));
        addBlock("Platform", generator::toString(BuildSettings(s).TargetOS.Arch));
        endBlock();
    }
    endBlock();
}

void ProjectEmitter::addPropertyGroupConfigurationTypes(const Project &p)
{
    for (auto &s : p.getSettings())
    {
        auto &d = p.getData(s);
        beginBlockWithConfiguration("PropertyGroup", s, {{"Label", "Configuration"}});
        addConfigurationType((int)d.type);
        //addBlock("UseDebugLibraries", generator::toString(s.Settings.Native.ConfigurationType));
        if (1)//toolset.empty())
        {
            addBlock("PlatformToolset", getVsToolset(p.g->toolset_version));
        }
        //else
            //addBlock("PlatformToolset", toolset);

        endBlock();
    }
}

void ProjectEmitter::addConfigurationType(int t)
{
    switch ((VSProjectType)t)
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

void ProjectEmitter::addPropertySheets(const Project &p)
{
    for (auto &s : p.getSettings())
    {
        beginBlock("ImportGroup", {{"Condition", "'$(Configuration)|$(Platform)'=='" +
            get_project_configuration(s) + "'"},
            {"Label", "PropertySheets"}});
        addBlock("Import", "", {
            {"Project", "$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props"},
            {"Condition", "exists('$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props')"},
            {"Label", "LocalAppDataPlatform"},
            });
        endBlock();
    }
}

VSFileType ProjectEmitter::beginFileBlock(const path &p)
{
    auto t = get_vs_file_type_by_ext(p);
    beginBlock(toString(t), { { "Include", to_string(p.u8string()) } });
    return t;
}

void ProjectEmitter::endFileBlock()
{
    endBlock();
}
