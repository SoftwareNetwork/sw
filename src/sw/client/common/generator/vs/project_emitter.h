/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2019 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <sw/manager/version.h>

#include <primitives/emitter.h>

namespace sw
{

struct BuildSettings;

}

struct Project;

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

struct XmlEmitter : primitives::Emitter
{
    std::stack<String> blocks;

    XmlEmitter(bool print_version = true);

    void beginBlock(const String &n, const std::map<String, String> &params = {}, bool empty = false);
    void beginBlockWithConfiguration(const String &n, const sw::BuildSettings &s, std::map<String, String> params = {}, bool empty = false);
    void endBlock(bool text = false);
    void addBlock(const String &n, const String &v, const std::map<String, String> &params = {});

protected:
    void beginBlock1(const String &n, const std::map<String, String> &params = {}, bool empty = false);
    void endBlock1(bool text = false);
};

struct FiltersEmitter : XmlEmitter
{
    void beginProject();
    void endProject();
};

struct ProjectEmitter : XmlEmitter
{
    void beginProject(const sw::Version &);
    void endProject();

    void addProjectConfigurations(const Project &);
    void addPropertyGroupConfigurationTypes(const Project &);
    void addConfigurationType(int t);
    void addPropertySheets(const Project &);

    VSFileType beginFileBlock(const path &p);
    void endFileBlock();
};

String get_configuration(const sw::BuildSettings &s);
std::string getVsToolset(const sw::Version &v);

VSFileType get_vs_file_type_by_ext(const path &p);
String toString(VSFileType t);

