/*
 * Copyright (C) 2016-2017, Egor Pugin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cmake.h"

#include <access_table.h>
#include <database.h>
#include <directories.h>
#include <exceptions.h>
#include <hash.h>
#include <lock.h>
#include <inserts.h>
#include <program.h>
#include <resolver.h>
#include <settings.h>
#include <shell_link.h>

#include <boost/algorithm/string.hpp>

#include <primitives/command.h>
#include <primitives/date_time.h>
#include <primitives/executor.h>

#ifdef _WIN32
#include <WinReg.hpp>
#endif

#include <primitives/log.h>
//DECLARE_STATIC_LOGGER(logger, "cmake");

String repeat(const String &e, int n);

// common?
const String cppan_project_name = "__cppan";
const String exports_dir_name = "exports";
const String exports_dir = "${CMAKE_BINARY_DIR}/" + exports_dir_name + "/";
const String cppan_ide_folder = "CPPAN Targets";
const String packages_folder = cppan_ide_folder + "/Packages";
const String service_folder = cppan_ide_folder + "/Service";
const String service_deps_folder = service_folder + "/Dependencies";
const String dependencies_folder = cppan_ide_folder + "/Dependencies";
const String local_dependencies_folder = dependencies_folder + "/Local";

//
const String cmake_config_filename = "CMakeLists.txt";
const String cppan_build_dir = "build";
const String cmake_functions_filename = "functions.cmake";
const String cmake_header_filename = "header.cmake";
const String cppan_cmake_config_filename = "CPPANConfig.cmake";
const String cmake_export_import_filename = "export.cmake";
const String cmake_helpers_filename = "helpers.cmake";
const String cppan_stamp_filename = "cppan_sources.stamp";
const String cppan_checks_yml = "checks.yml";
const String parallel_checks_file = "vars.txt";

const String cmake_src_actions_filename = "actions.cmake";
const String cmake_src_include_guard_filename = "include.cmake";

const String cmake_obj_build_filename = "build.cmake";
const String cmake_obj_generate_filename = "generate.cmake";
const String cmake_obj_exports_filename = "exports.cmake";

const String cmake_minimum_required = "cmake_minimum_required(VERSION 3.2.0)";
const String cmake_debug_message_fun = "cppan_debug_message";
const String cppan_dummy_build_target = "b"; // build
const String cppan_dummy_copy_target = "c"; // copy

const String debug_stack_space_diff = repeat(" ", 4);
const String config_delimeter_short = repeat("#", 40);
const String config_delimeter = repeat(config_delimeter_short, 2);

const String cmake_includes = R"(
include(CheckCXXSymbolExists)
include(CheckFunctionExists)
include(CheckIncludeFiles)
include(CheckIncludeFile)
include(CheckIncludeFileCXX)
include(CheckLibraryExists)
include(CheckTypeSize)
include(CheckCSourceCompiles)
include(CheckCSourceRuns)
include(CheckCXXSourceCompiles)
include(CheckCXXSourceRuns)
include(CheckStructHasMember)
include(GenerateExportHeader)
include(TestBigEndian)
)";

class ScopedDependencyCondition
{
    CMakeContext &ctx;
    const Package &d;
    bool empty_lines;

public:
    ScopedDependencyCondition(CMakeContext &ctx, const Package &d, bool empty_lines = true)
        : ctx(ctx), d(d), empty_lines(empty_lines)
    {
        if (d.conditions.empty())
            return;
        ctx.addLine("# conditions for dependency: " + d.target_name);
        for (auto &c : d.conditions)
            ctx.if_(c);
    }

    ~ScopedDependencyCondition()
    {
        if (d.conditions.empty())
            return;
        for (auto &c [[maybe_unused]] : d.conditions)
            ctx.endif();
        if (empty_lines)
            ctx.emptyLines();
    }
};

String cmake_debug_message(const String &s)
{
    if (!Settings::get_local_settings().debug_generated_cmake_configs)
        return "";

    return cmake_debug_message_fun + "(\"" + s + "\")";
}

String repeat(const String &e, int n)
{
    String s;
    if (n < 0)
        return s;
    s.reserve(e.size() * n);
    for (int i = 0; i < n; i++)
        s += e;
    return s;
}

void config_section_title(CMakeContext &ctx, const String &t, bool nodebug = false)
{
    ctx.emptyLines();
    ctx.addLine(config_delimeter);
    ctx.addLine("#");
    ctx.addLine("# " + t);
    ctx.addLine("#");
    ctx.addLine(config_delimeter);
    ctx.addLine();
    if (!nodebug)
        ctx.addLine(cmake_debug_message("Section: " + t));
    ctx.emptyLines();
}

void file_header(CMakeContext &ctx, const Package &d, bool root)
{
    if (!d.empty())
    {
        ctx.addLine("#");
        ctx.addLine("# cppan");
        ctx.addLine("# package: " + d.ppath.toString());
        ctx.addLine("# version: " + d.version.toString());
        ctx.addLine("#");
        ctx.addLine("# source dir: " + normalize_path(d.getDirSrc().string()));
        ctx.addLine("# binary dir: " + normalize_path(d.getDirObj().string()));
        ctx.addLine("#");
        ctx.addLine("# package hash      : " + d.getHash());
        ctx.addLine("# package hash short: " + d.getHashShort());
        ctx.addLine("#");
    }
    else
    {
        ctx.addLine("#");
        ctx.addLine("# cppan");
        ctx.addLine("#");
    }

    config_section_title(ctx, "header", true);
    ctx.addLine("include(" + normalize_path(directories.get_static_files_dir() / cmake_header_filename) + ")");
    ctx.addLine();

    if (!Settings::get_local_settings().debug_generated_cmake_configs)
        return;

    // before header message
    if (!root)
    {
        ctx.addLine("set(CPPAN_DEBUG_STACK_SPACE \"${CPPAN_DEBUG_STACK_SPACE}" + debug_stack_space_diff + "\" CACHE STRING \"\" FORCE)");
        ctx.addLine();
    }

    if (!d.empty())
    {
        ctx.addLine(cmake_debug_message("Entering file: ${CMAKE_CURRENT_LIST_FILE}"));
        ctx.addLine(cmake_debug_message("Package      : " + d.target_name));
    }
    else
        ctx.addLine(cmake_debug_message("Entering file: ${CMAKE_CURRENT_LIST_FILE}"));
    ctx.addLine();
    ctx.addLine(config_delimeter);
    ctx.addLine();
}

void file_footer(CMakeContext &ctx, const Package &d)
{
    if (!Settings::get_local_settings().debug_generated_cmake_configs)
        return;

    config_section_title(ctx, "footer", true);
    ctx.addLine(cmake_debug_message("Leaving file: ${CMAKE_CURRENT_LIST_FILE}"));
    ctx.addLine();

    // after footer message
    ctx.addLine("string(LENGTH \"${CPPAN_DEBUG_STACK_SPACE}\" len)");
    ctx.addLine("math(EXPR len \"${len}-" + std::to_string(debug_stack_space_diff.size()) + "\")");
    ctx.if_("NOT ${len} LESS 0");
    ctx.addLine("string(SUBSTRING \"${CPPAN_DEBUG_STACK_SPACE}\" 0 ${len} CPPAN_DEBUG_STACK_SPACE)");
    ctx.else_();
    ctx.addLine("set(CPPAN_DEBUG_STACK_SPACE \"\")");
    ctx.endif();
    ctx.addLine("set(CPPAN_DEBUG_STACK_SPACE \"${CPPAN_DEBUG_STACK_SPACE}\" CACHE STRING \"\" FORCE)");
    ctx.addLine();

    ctx.addLine(config_delimeter);
    ctx.addLine();
    ctx.splitLines();
}

void print_storage_dirs(CMakeContext &ctx)
{
    config_section_title(ctx, "storage dirs");
    ctx.addLine("set_cache_var(STORAGE_DIR \"" + normalize_path(directories.storage_dir) + "\")");
    ctx.addLine("set_cache_var(STORAGE_DIR_ETC \"" + normalize_path(directories.storage_dir_etc) + "\")");
    ctx.addLine("set_cache_var(STORAGE_DIR_ETC_STATIC \"" + normalize_path(directories.get_static_files_dir()) + "\")");
    ctx.addLine("set_cache_var(STORAGE_DIR_USR \"" + normalize_path(directories.storage_dir_usr) + "\")");
    ctx.addLine();
}

void print_local_project_files(CMakeContext &ctx, const Project &p)
{
    ctx.increaseIndent("set(src");
    for (auto &f : FilesSorted(p.files.begin(), p.files.end()))
        ctx.addLine("\"" + normalize_path(f) + "\"");
    ctx.decreaseIndent(")");
}

String add_target(const Package &p)
{
    if (p.flags[pfExecutable])
        return "add_executable";
    return "add_library";
}

String add_target_suffix(const String &t)
{
    auto &s = Settings::get_local_settings().meta_target_suffix;
    if (!s.empty())
        return t + "-" + s;
    return t;
}

String cppan_dummy_target(const String &name)
{
    String t = "cppan-d";
    if (!name.empty())
        t += "-" + name;
    return add_target_suffix(t);
}

void set_target_properties(CMakeContext &ctx, const String &name, const String &property, const String &value)
{
    ctx.addLine("set_target_properties(" + name + " PROPERTIES " + property + " " + value + ")");
}

void set_target_properties(CMakeContext &ctx, const String &property, const String &value)
{
    set_target_properties(ctx, "${this}", property, value);
}

void declare_dummy_target(CMakeContext &ctx, const String &name)
{
    config_section_title(ctx, "dummy compiled target " + name);
    ctx.addLine("# this target will be always built before any other");
    ctx.if_("VISUAL_STUDIO");
    ctx.addLine("add_custom_target(" + cppan_dummy_target(name) + " ALL DEPENDS cppan_intentionally_missing_file.txt)");
    ctx.elseif("NINJA");
    ctx.addLine("add_custom_target(" + cppan_dummy_target(name) + " ALL)");
    ctx.else_();
    ctx.addLine("add_custom_target(" + cppan_dummy_target(name) + " ALL)");
    ctx.endif();
    ctx.addLine();
    set_target_properties(ctx, cppan_dummy_target(name), "FOLDER", "\"" + service_folder + "\"");
    ctx.emptyLines();
}

void print_solution_folder(CMakeContext &ctx, const String &target, const path &folder)
{
    set_target_properties(ctx, target, "FOLDER", "\"" + normalize_path(folder) + "\"");
}

String add_subdirectory(String src)
{
    normalize_string(src);
    return "cppan_include(\"" + src + "/" + cmake_src_include_guard_filename + "\")";
}

void add_subdirectory(CMakeContext &ctx, const String &src)
{
    ctx.addLine(add_subdirectory(src));
}

String prepare_include_directory(const String &i)
{
    if (i.find("${") == 0)
        return i;
    return "${SDIR}/" + i;
};

void print_sdir_bdir(CMakeContext &ctx, const Package &d)
{
    if (d.flags[pfLocalProject])
        ctx.addLine("set(SDIR " + normalize_path(rd[d].config->getDefaultProject().root_directory) + ")");
    else
        ctx.addLine("set(SDIR ${CMAKE_CURRENT_SOURCE_DIR})");
    ctx.addLine("set(BDIR ${CMAKE_CURRENT_BINARY_DIR})");
    ctx.addLine("set(BDIR_PRIVATE ${BDIR}/cppan_private)");
    ctx.addLine("execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${BDIR_PRIVATE})");
    ctx.emptyLines();
}

String get_binary_path(const Package &d, const String &prefix)
{
    return prefix + "/cppan/" + d.getHashShort();
}

String get_binary_path(const Package &d)
{
    return get_binary_path(d, "${CMAKE_BINARY_DIR}");
}

void print_dependencies(CMakeContext &ctx, const Package &d, bool use_cache)
{
    const auto &dd = rd[d].dependencies;

    if (dd.empty())
        return;

    std::vector<Package> includes;
    CMakeContext ctx2, ctx_actions;

    config_section_title(ctx, "direct dependencies");

    // make sure this var is 0
    //ctx.addLine("set(CPPAN_BUILD_EXECUTABLES_WITH_SAME_CONFIG 0)");
    //ctx.addLine();

    for (auto &p : dd)
    {
        auto &dep = p.second;

        ScopedDependencyCondition sdc(ctx, dep);
        if (dep.flags[pfLocalProject])
            ctx.addLine("set(" + dep.variable_no_version_name + "_DIR " + normalize_path(rd[dep].config->getDefaultProject().root_directory) + ")");
        else
            ctx.addLine("set(" + dep.variable_no_version_name + "_DIR " + normalize_path(dep.getDirSrc()) +  ")");
    }
    ctx.emptyLines();

    for (auto &p : dd)
    {
        auto &dep = p.second;

        const auto dir = [&dep, &use_cache]
        {
            // do not "optimize" this condition (whole if..else)
            if (dep.flags[pfHeaderOnly] || dep.flags[pfIncludeDirectoriesOnly])
                return dep.getDirSrc();
            else if (use_cache)
                return dep.getDirObj();
            else
                return dep.getDirSrc();
        }();

        if (dep.flags[pfIncludeDirectoriesOnly])
        {
            // MUST be here!
            // actions are executed from include_directories only projects
            ScopedDependencyCondition sdc(ctx_actions, dep);
            ctx_actions.addLine("# " + dep.target_name);
            ctx_actions.addLine("cppan_include(\"" + normalize_path(dir / cmake_src_actions_filename) + "\")");
        }
        else if (!use_cache || dep.flags[pfHeaderOnly])
        {
            ScopedDependencyCondition sdc(ctx, dep);
            ctx.addLine("# " + dep.target_name);
            add_subdirectory(ctx, dir.string());
        }
        else if (dep.flags[pfLocalProject])
        {
            ScopedDependencyCondition sdc(ctx, dep);
            ctx.if_("NOT TARGET " + dep.target_name + "");
            ctx.if_("CPPAN_USE_CACHE");
            ctx.addLine("add_subdirectory(\"" + normalize_path(dir) + "\" \"" + normalize_path(dep.getDirObj() / "build/${config_dir}") + "\")");
            ctx.else_();
            ctx.addLine("add_subdirectory(\"" + normalize_path(dir) + "\" \"" + get_binary_path(dep) + "\")");
            ctx.endif();
            ctx.endif();
        }
        else
        {
            // add local build includes
            ScopedDependencyCondition sdc2(ctx2, dep);
            ctx2.addLine("# " + dep.target_name);
            add_subdirectory(ctx2, dep.getDirSrc().string());
            includes.push_back(dep);
        }
    }
    ctx.addLine();

    if (!includes.empty())
    {
        config_section_title(ctx, "include dependencies (they should be placed at the end)");
        ctx.if_("CPPAN_USE_CACHE");

        if (!d.empty())
        {
            ctx.addLine("set(CPPAN_BUILD_EXECUTABLES_WITH_SAME_CONFIG "s + (
                rd[d].config->getDefaultProject().build_dependencies_with_same_config ? "1" : "0") + ")");
            ctx.addLine();
        }

        for (auto &dep : includes)
        {
            ScopedDependencyCondition sdc(ctx, dep);
            ctx.addLine("# " + dep.target_name + "\n" +
                "cppan_include(\"" + normalize_path(dep.getDirObj() / cmake_obj_generate_filename) + "\")");
        }

        ctx.else_();
        ctx.addLine(boost::trim_copy(ctx2.getText()));
        ctx.endif();

        /*
        cmake currently does not support aliases to imported targets
        config_section_title(ctx, "aliases");
        for (auto &dep : includes)
        {
            add_aliases(ctx, dep, [](const Package &d, const String &s)
            {
                return add_target(d) + "(" + s + " ALIAS " + d.target_name_hash + ")";
            });
        }*/
    }

    // after all deps
    ctx += ctx_actions;

    ctx.splitLines();
}

void gather_build_deps(const Packages &dd, Packages &out, bool recursive = false, int depth = 0)
{
    for (auto &dp : dd)
    {
        auto &d = dp.second;
        if (d.flags[pfHeaderOnly] || d.flags[pfIncludeDirectoriesOnly])
            continue;
        // add only direct dependencies of the invocation package
        if (d.flags[pfExecutable])
        {
            if (depth == 0)
                out.insert(dp);
            continue;
        }
        auto i = out.insert(dp);
        if (i.second && recursive)
            gather_build_deps(rd[d].dependencies, out, recursive, depth + 1);
    }
}

void gather_copy_deps(const Packages &dd, Packages &out)
{
    for (auto &dp : dd)
    {
        auto &d = dp.second;
        if (d.flags[pfHeaderOnly] || d.flags[pfIncludeDirectoriesOnly])
            continue;
        if (d.flags[pfExecutable])
        {
            if (!Settings::get_local_settings().copy_all_libraries_to_output)
            {
                if (!d.flags[pfLocalProject])
                    continue;
                if (d.flags[pfLocalProject] && !d.flags[pfDirectDependency])
                    continue; // but maybe copy its deps?
            }
            else
            {
                // copy only direct executables
                if (!d.flags[pfDirectDependency])
                    continue;
            }
        }
        auto i = out.insert(dp);
        if (i.second)
            gather_copy_deps(rd[d].dependencies, out);
    }
}

auto run_command(const Settings &bs, primitives::Command &c)
{
    if (bs.build_system_verbose)
        c.inherit = true;
    std::error_code ec;
    c.execute(ec);
    if (ec)
        throw std::runtime_error("Run command '" + c.print() + "', error: " + boost::trim_copy(ec.message()));
    if (!bs.build_system_verbose)
        LOG_INFO(logger, "Ok");
    return c.exit_code;
}

auto library_api(const Package &d)
{
    return CPPAN_EXPORT_PREFIX + d.variable_name;
}

void CMakePrinter::print_build_dependencies(CMakeContext &ctx, const String &target) const
{
    // direct deps' build actions for non local build
    config_section_title(ctx, "build dependencies");

    // build deps
    ctx.if_("CPPAN_USE_CACHE");

    // Run building of dependencies before project building.
    // We build all deps because if some dep is removed,
    // build system give you and error about this.
    Packages build_deps;
    gather_build_deps(rd[d].dependencies, build_deps, true);

    if (!build_deps.empty())
    {
        CMakeContext local;
        local.addLine("set(CPPAN_GET_CHILDREN_VARIABLES 1)");
        local.addLine("get_configuration_with_generator(config)"); // children
        local.if_("CPPAN_BUILD_EXECUTABLES_WITH_SAME_CONFIG");
        local.addLine("get_configuration_with_generator(config_exe)");
        local.else_();
        local.addLine("get_configuration_exe(config_exe)");
        local.endif();
        local.addLine("set(CPPAN_GET_CHILDREN_VARIABLES 0)");

        local.emptyLines();
        local.addLine("string(TOUPPER \"${CMAKE_BUILD_TYPE}\" CMAKE_BUILD_TYPE_UPPER)");
        local.emptyLines();

        // we're in helper, set this var to build target
        if (d.empty())
            local.addLine("set(this " + target + ")");
        local.emptyLines();

        // TODO: check with ninja and remove if ok
        //Packages build_deps_all;
        //gather_build_deps(rd[d].dependencies, build_deps_all, true);
        //for (auto &dp : build_deps_all)
        for (auto &dp : build_deps)
        {
            auto &p = dp.second;

            // local projects are always built inside solution
            if (p.flags[pfLocalProject])
                continue;

            ScopedDependencyCondition sdc(local, p);
            local.addLine("get_target_property(implib_" + p.variable_name + " " + p.target_name + " IMPORTED_IMPLIB_${CMAKE_BUILD_TYPE_UPPER})");
            local.addLine("get_target_property(imploc_" + p.variable_name + " " + p.target_name + " IMPORTED_LOCATION_${CMAKE_BUILD_TYPE_UPPER})");
            local.addLine("get_target_property(impson_" + p.variable_name + " " + p.target_name + " IMPORTED_SONAME_${CMAKE_BUILD_TYPE_UPPER})");
        }
        local.emptyLines();

#define ADD_VAR(v) rest += "-D" #v "=${" #v "} "
        String rest;
        // we do not pass this var to children
        //ADD_VAR(CPPAN_BUILD_EXECUTABLES_WITH_SAME_CONFIG);
        ADD_VAR(CPPAN_BUILD_EXECUTABLES_WITH_SAME_CONFIGURATION);
        ADD_VAR(CMAKE_BUILD_TYPE);
        ADD_VAR(CPPAN_BUILD_VERBOSE);
        ADD_VAR(CPPAN_BUILD_WARNING_LEVEL);
        ADD_VAR(CPPAN_RC_ENABLED);
        ADD_VAR(CPPAN_COPY_ALL_LIBRARIES_TO_OUTPUT);
        ADD_VAR(N_CORES);
        ADD_VAR(XCODE);
        ADD_VAR(NINJA);
        ADD_VAR(NINJA_FOUND);
        ADD_VAR(VISUAL_STUDIO);
        ADD_VAR(CLANG);
#undef ADD_VAR

        local.addLine("set(rest \"" + rest + "\")");
        local.emptyLines();

        local.addLine(R"(set(ext sh)
if (WIN32)
    set(ext bat)
endif()
)");
        local.emptyLines();

        if (d.empty())
            local.addLine("set(file ${BDIR}/cppan_build_deps_$<CONFIG>.${ext})");
        else
            // FIXME: this is probably incorrect
            local.addLine("set(file ${BDIR}/cppan_build_deps_" + d.target_name_hash + "_$<CONFIG>.${ext})");
        local.emptyLines();

        local.addLine(R"(#if (NOT CPPAN_BUILD_LEVEL)
    #set(CPPAN_BUILD_LEVEL 0)
#else()
    #math(EXPR CPPAN_BUILD_LEVEL "${CPPAN_BUILD_LEVEL} + 1")
#endif()

set(bat_file_error)
if (WIN32)
    set(bat_file_error "@if %errorlevel% neq 0 goto :cmEnd")
endif()
)");

        bool has_build_deps = false;
        for (auto &dp : build_deps)
        {
            auto &p = dp.second;

            // local projects are always built inside solution
            if (p.flags[pfLocalProject])
                continue;

            String cfg = "config";
            if (p.flags[pfExecutable] && !p.flags[pfLocalProject])
                cfg = "config_exe";

            has_build_deps = true;
            ScopedDependencyCondition sdc(local, p, false);
            local.addLine("set(bd_" + p.variable_name + " \"");
            //local.addLine("@echo Building " + p.target_name + ": ${" + cfg + "}");
#ifdef _WIN32
            local.addNoNewLine("@");
#endif
            local.addText("\\\"${CMAKE_COMMAND}\\\" ");
            //local.addText("-DCPPAN_BUILD_LEVEL=${CPPAN_BUILD_LEVEL} ");
            local.addText("-DTARGET_FILE=$<TARGET_FILE:" + p.target_name + "> ");
            local.addText("-DCONFIG=$<CONFIG> ");
            local.addText("-DBUILD_DIR=" + normalize_path(p.getDirObj()) + "/build/${" + cfg + "} ");
            local.addText("-DEXECUTABLE="s + (p.flags[pfExecutable] ? "1" : "0") + " ");
            if (d.empty())
                local.addText("-DMULTICORE=1 ");
            local.addText("${rest} ");

            local.addText("-P " + normalize_path(p.getDirObj()) + "/" + cmake_obj_build_filename);
#ifndef _WIN32
            // causes system overloads
            //local.addText(" &");
#endif
            local.addText("\n${bat_file_error}\")");
        }
        local.emptyLines();

        local.addLine("set(bat_file_begin)");
        local.if_("WIN32");
        local.addLine("set(bat_file_begin @setlocal)");
        local.addLine(R"(set(bat_file_error "\n
@exit /b 0
:cmEnd
@endlocal & @call :cmErrorLevel %errorlevel%
:cmErrorLevel
@exit /b %1
"))");
        local.endif();

        local.increaseIndent("file(GENERATE OUTPUT ${file} CONTENT \"");
        local.addLine("${bat_file_begin}");
        for (auto &dp : build_deps)
        {
            auto &p = dp.second;

            // local projects are always built inside solution
            if (p.flags[pfLocalProject])
                continue;

            local.addLine("${bd_" + p.variable_name + "}");
        }
        local.addLine("${bat_file_error}");
        local.decreaseIndent("\")");
        local.emptyLines();

        local.addLine(R"(if (UNIX)
    set(file chmod u+x ${file} COMMAND ${file})
endif()
)");

        bool deps = false;
        String build_deps_tgt = "${this}";
        if (d.empty() && target.find("-b") != target.npos)
        {
            build_deps_tgt += "-d"; // deps
            deps = true;
        }
        else
            build_deps_tgt += "-b-d";

        // do not use add_custom_command as it doesn't work
        // add custom target and add a dependency below
        // second way is to use add custom target + add custom command (POST?(PRE)_BUILD)
        local.addLine("set(bp)");
        //for (auto &dp : build_deps_all)
        for (auto &dp : build_deps)
        {
            auto &p = dp.second;

            // local projects are always built inside solution
            if (p.flags[pfLocalProject])
                continue;

            ScopedDependencyCondition sdc(local, p, false);
            local.addLine("set(bp ${bp} ${implib_" + p.variable_name + "})");
            local.addLine("set(bp ${bp} ${imploc_" + p.variable_name + "})");
            local.addLine("set(bp ${bp} ${impson_" + p.variable_name + "})");
        }
        local.emptyLines();

        local.increaseIndent("add_custom_target(" + build_deps_tgt);
        local.addLine("COMMAND ${file}");
        local.increaseIndent("BYPRODUCTS ${bp}");
        local.decreaseIndent(")", 2);
        local.addLine("add_dependencies(${this} " + build_deps_tgt + ")");
        print_solution_folder(local, build_deps_tgt, deps ? service_folder : service_deps_folder);
        //this causes long paths issue
        //if (deps)
        //    set_target_properties(local, build_deps_tgt, "PROJECT_LABEL", "dependencies");
        //else
        //    set_target_properties(local, build_deps_tgt, "PROJECT_LABEL", (d.flags[pfLocalProject] ? d.ppath.back() : d.target_name) + "-build-dependencies");
        local.addLine();

        // alias dependencies
        if (d.empty())
        {
            auto tt = "add_dependencies"s;
            for (auto &dp : build_deps)
            {
                if (dp.second.flags[pfLocalProject])
                    continue;
                add_aliases(local, dp.second, false, [&tt](const auto &s, const auto &v)
                {
                    if (v.patch != -1)
                        return ""s;
                    return tt + "(" + s + " ${this})";
                });
            }
        }

        if (has_build_deps)
            ctx.addWithRelativeIndent(local);
    }

    ctx.endif();
    ctx.addLine();
}

void CMakePrinter::print_copy_dependencies(CMakeContext &ctx, const String &target) const
{
    config_section_title(ctx, "copy dependencies");

    ctx.if_("CPPAN_USE_CACHE");

    // prepare copy files
    ctx.addLine("set(ext sh)");
    ctx.if_("WIN32");
    ctx.addLine("set(ext bat)");
    ctx.endif();
    ctx.emptyLines();
    ctx.addLine("set(file ${BDIR}/cppan_copy_deps_$<CONFIG>.${ext})");
    ctx.emptyLines();
    ctx.addLine("set(copy_content)");
    ctx.if_("WIN32");
    ctx.addLine("set(copy_content \"${copy_content} @setlocal\\n\")");
    ctx.endif();

    // we're in helper, set this var to build target
    if (d.empty())
        ctx.addLine("set(this " + target + ")");
    ctx.emptyLines();

    ctx.addLine("set(output_dir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})");
    ctx.if_("NOT output_dir");
    ctx.addLine("set(output_dir ${CMAKE_BINARY_DIR})");
    ctx.endif();
    ctx.if_("VISUAL_STUDIO OR XCODE");
    ctx.addLine("set(output_dir ${output_dir}/$<CONFIG>)");
    ctx.endif();
    ctx.if_("CPPAN_BUILD_OUTPUT_DIR");
    ctx.addLine("set(output_dir ${CPPAN_BUILD_OUTPUT_DIR})");
    /*ctx.elseif("MSVC");
    ctx.addLine("set(output_dir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})");
    ctx.elseif("CMAKE_RUNTIME_OUTPUT_DIRECTORY");
    ctx.addLine("set(output_dir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})");
    ctx.else_();
    ctx.addLine("set(output_dir ${CMAKE_BINARY_DIR})");*/
    ctx.endif();
    if (d.flags[pfLocalProject])
        ctx.addLine("set(output_dir $<TARGET_FILE_DIR:${this}>)");
    ctx.addLine();

    Packages copy_deps;
    gather_copy_deps(rd[d].dependencies, copy_deps);
    for (auto &dp : copy_deps)
    {
        auto &p = dp.second;

        if (p.flags[pfExecutable])
        {
            // if we have an exe, we must include all dependent targets
            // because they're not visible from exe directly
            ScopedDependencyCondition sdc(ctx, p);
            config_section_title(ctx, "Executable build deps for " + p.target_name);
            print_dependencies(ctx, p, settings.use_cache);
            config_section_title(ctx, "End of executable build deps for " + p.target_name);
            ctx.emptyLines();
        }

        config_section_title(ctx, "Copy " + p.target_name);

        ScopedDependencyCondition sdc(ctx, p);
        ctx.addLine("set(copy 1)");
        ctx.addLine("get_target_property(type " + p.target_name + " TYPE)");

        ctx.if_("\"${type}\" STREQUAL STATIC_LIBRARY");
        ctx.addLine("set(copy 0)");
        ctx.endif();
        ctx.addLine();

        ctx.if_("CPPAN_COPY_ALL_LIBRARIES_TO_OUTPUT");
        ctx.addLine("set(copy 1)");
        ctx.endif();
        ctx.addLine();

        auto prj = rd[p].config->getDefaultProject();

        auto output_directory = "${output_dir}/"s;
        output_directory += prj.output_directory + "/";

        ctx.if_("copy");
        {
            String s;
#ifdef _WIN32
            s += "set(copy_content \"${copy_content} @\")\n";
#endif
            s += "set(copy_content \"${copy_content} \\\"${CMAKE_COMMAND}\\\" -E copy_if_different ";
            String name;
            if (!prj.output_name.empty())
                name = prj.output_name;
            else
            {
                if (p.flags[pfExecutable] || (p.flags[pfLocalProject] && rd[p].config->getDefaultProject().type == ProjectType::Executable))
                {
                    if (settings.full_path_executables)
                        name = "$<TARGET_FILE_NAME:" + p.target_name + ">";
                    else
                        name = p.ppath.back() + "${CMAKE_EXECUTABLE_SUFFIX}";
                }
                else
                {
                    // if we change non-exe name, we still won't fix linker information about dependencies' names
                    name = "$<TARGET_FILE_NAME:" + p.target_name + ">";
                }
            }
            s += "$<TARGET_FILE:" + p.target_name + "> " + output_directory + name;
            s += "\\n\")";
            ctx.addLine(s);
            ctx.addLine("add_dependencies(" + target + " " + p.target_name + ")");

            ctx.if_("WIN32");
            ctx.addLine("set(copy_content \"${copy_content} @if %errorlevel% neq 0 goto :cmEnd\\n\")");
            ctx.endif();
        }
        ctx.addLine();

        // import library for shared libs
        if (settings.copy_import_libs || settings.copy_all_libraries_to_output)
        {
            ctx.if_("\"${type}\" STREQUAL SHARED_LIBRARY");
            String s;
            s += "set(copy_content \"${copy_content} \\\"${CMAKE_COMMAND}\\\" -E copy_if_different ";
            s += "$<TARGET_LINKER_FILE:" + p.target_name + "> " + output_directory + "$<TARGET_LINKER_FILE_NAME:" + p.target_name + ">";
            s += "\\n\")";
            ctx.addLine(s);

            ctx.if_("WIN32");
            ctx.addLine("set(copy_content \"${copy_content} @if %errorlevel% neq 0 goto :cmEnd\\n\")");
            ctx.endif();

            ctx.endif();
        }

        ctx.endif();
        ctx.addLine();
    }

    ctx.if_("WIN32");
    ctx.addLine(R"(set(copy_content "${copy_content}\n
@exit /b 0
:cmEnd
@endlocal & @call :cmErrorLevel %errorlevel%
:cmErrorLevel
@exit /b %1
"))");
    ctx.endif();

    ctx.addLine(R"(
file(GENERATE OUTPUT ${file} CONTENT "
    ${copy_content}
")
if (UNIX)
    set(file chmod u+x ${file} COMMAND ${file})
endif()
add_custom_command(TARGET )" + target + R"( POST_BUILD
    COMMAND ${file}
)
)");

    ctx.endif();
    ctx.addLine();

    // like with build deps
    // only for ninja at the moment
    ctx.if_("NINJA");
    for (auto &dp : copy_deps)
    {
        auto &p = dp.second;

        // local projects are always built inside solution
        if (p.flags[pfLocalProject])
            continue;

        ScopedDependencyCondition sdc(ctx, p);
        ctx.addLine("get_target_property(imploc_" + p.variable_name + " " + p.target_name + " IMPORTED_LOCATION_${CMAKE_BUILD_TYPE_UPPER})");
    }
    ctx.emptyLines();

    bool deps = false;
    String build_deps_tgt = "${this}";
    if (d.empty() && target.find("-c") != target.npos)
    {
        build_deps_tgt += "-d"; // deps
        deps = true;
    }
    else
        build_deps_tgt += "-c-d";

    // do not use add_custom_command as it doesn't work
    // add custom target and add a dependency below
    // second way is to use add custom target + add custom command (POST?(PRE)_BUILD)
    ctx.addLine("set(bp)");
    //for (auto &dp : build_deps_all)
    for (auto &dp : copy_deps)
    {
        auto &p = dp.second;

        // local projects are always built inside solution
        if (p.flags[pfLocalProject])
            continue;

        ScopedDependencyCondition sdc(ctx, p, false);
        ctx.addLine("set(bp ${bp} ${imploc_" + p.variable_name + "})");
    }
    ctx.emptyLines();

    ctx.increaseIndent("add_custom_target(" + build_deps_tgt);
    ctx.addLine("COMMAND ${file}");
    ctx.increaseIndent("BYPRODUCTS ${bp}");
    ctx.decreaseIndent(")", 2);
    ctx.addLine("add_dependencies(${this} " + build_deps_tgt + ")");
    print_solution_folder(ctx, build_deps_tgt, deps ? service_folder : service_deps_folder);
    //this causes long paths issue
    //if (deps)
    //    set_target_properties(ctx, build_deps_tgt, "PROJECT_LABEL", "dependencies");
    //else
    //    set_target_properties(ctx, build_deps_tgt, "PROJECT_LABEL", (d.flags[pfLocalProject] ? d.ppath.back() : d.target_name) + "-build-dependencies");
    ctx.endif();
    ctx.addLine();
}

void CMakePrinter::prepare_rebuild() const
{
    // remove stamp file to start rebuilding
    auto odir = d.getDirObj() / cppan_build_dir;
    if (!fs::exists(odir))
        return;
    for (auto &dir : boost::make_iterator_range(fs::directory_iterator(odir), {}))
    {
        if (!fs::is_directory(dir))
            continue;

        // remove exports to exported definitions, options etc.
        fs::remove_all(dir / exports_dir_name);

        // remove stamp files to cause rebuilding
        for (auto &f : boost::make_iterator_range(fs::directory_iterator(dir), {}))
        {
            if (!fs::is_regular_file(f))
                continue;
            if (f.path().filename().string() != cppan_stamp_filename)
                continue;
            remove_file(f);
        }
    }
}

void CMakePrinter::prepare_build(const BuildSettings &bs) const
{
    auto &s = Settings::get_local_settings();

    CMakeContext ctx;
    file_header(ctx, d);

    config_section_title(ctx, "cmake settings");
    ctx.addLine(cmake_minimum_required);
    ctx.addLine();

    ctx.addLine("include(" + normalize_path(directories.get_static_files_dir() / cmake_functions_filename) + ")");

    config_section_title(ctx, "project settings");
    ctx.addLine("project(" + bs.filename_without_ext + " LANGUAGES C CXX)");
    ctx.addLine();

    config_section_title(ctx, "compiler & linker settings");
    ctx.addLine(R"xxx(# Output directory settings
set(output_dir ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${output_dir})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${output_dir})
#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${output_dir})

if (NOT CMAKE_BUILD_TYPE)
    set_cache_var(CMAKE_BUILD_TYPE )xxx" + s.default_configuration + R"xxx()
endif()

if (WIN32)
    set(CMAKE_INSTALL_PREFIX "C:\\\\cppan")
else()
    set(CMAKE_INSTALL_PREFIX "/opt/local/cppan")
endif()

set_cache_var(XCODE 0)
if (CMAKE_GENERATOR STREQUAL Xcode)
    set_cache_var(XCODE 1)
endif()

set_cache_var(NINJA 0)
if (CMAKE_GENERATOR STREQUAL Ninja)
    set_cache_var(NINJA 1)
endif()

find_program(ninja ninja)
if (NOT "${ninja}" STREQUAL "ninja-NOTFOUND")
    set_cache_var(NINJA_FOUND 1)
elseif()
    find_program(ninja ninja-build)
    if (NOT "${ninja}" STREQUAL "ninja-NOTFOUND")
        set_cache_var(NINJA_FOUND 1)
    endif()
endif()

set_cache_var(VISUAL_STUDIO 0)
if (MSVC AND NOT NINJA)
    set_cache_var(VISUAL_STUDIO 1)
endif()

set_cache_var(CLANG 0)
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
    set_cache_var(CLANG 1)
endif()
if (CMAKE_VS_PLATFORM_TOOLSET MATCHES "(v[0-9]+_clang_.*|LLVM-vs[0-9]+.*)")
    set_cache_var(CLANG 1)
endif()

if (VISUAL_STUDIO AND CLANG AND NOT NINJA_FOUND)
    message(STATUS "Warning: Build with MSVC and Clang without ninja will be single threaded - very very slow.")
endif()

if (VISUAL_STUDIO AND CLANG AND NINJA_FOUND AND NOT NINJA)
    set_cache_var(VISUAL_STUDIO_ACCELERATE_CLANG 1)
    #if ("${CMAKE_LINKER}" STREQUAL "CMAKE_LINKER-NOTFOUND")
    #    message(FATAL_ERROR "CMAKE_LINKER must be set in order to accelerate clang build with MSVC!")
    #endif()
endif()

if (MSVC)
    if (NOT CLANG)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
    endif()
endif()
)xxx");

    if (!s.install_prefix.empty())
    {
        ctx.addLine("set(CMAKE_INSTALL_PREFIX " + s.install_prefix + ")");
        ctx.addLine();
    }

    // compiler flags
    ctx.addLine("set(CMAKE_C_FLAGS \"${CMAKE_C_FLAGS} " + s.c_compiler_flags + "\")");
    ctx.addLine("set(CMAKE_CXX_FLAGS \"${CMAKE_CXX_FLAGS} " + s.cxx_compiler_flags + "\")");
    ctx.addLine();

    for (int i = 0; i < Settings::CMakeConfigurationType::Max; i++)
    {
        auto &cfg = configuration_types[i];
        ctx.addLine("set(CMAKE_C_FLAGS_" + cfg + " \"${CMAKE_C_FLAGS_" + cfg + "} " + s.c_compiler_flags_conf[i] + "\")");
        ctx.addLine("set(CMAKE_CXX_FLAGS_" + cfg + " \"${CMAKE_CXX_FLAGS_" + cfg + "} " + s.cxx_compiler_flags_conf[i] + "\")");
        ctx.addLine();
    }

    // linker flags
    ctx.addLine("set(CMAKE_EXE_LINKER_FLAGS \"${CMAKE_EXE_LINKER_FLAGS} " + s.link_flags + "\")");
    ctx.addLine("set(CMAKE_MODULE_LINKER_FLAGS \"${CMAKE_MODULE_LINKER_FLAGS} " + s.link_flags + "\")");
    ctx.addLine("set(CMAKE_SHARED_LINKER_FLAGS \"${CMAKE_SHARED_LINKER_FLAGS} " + s.link_flags + "\")");
    ctx.addLine("set(CMAKE_STATIC_LINKER_FLAGS \"${CMAKE_STATIC_LINKER_FLAGS} " + s.link_flags + "\")");
    ctx.addLine();

    for (int i = 0; i < Settings::CMakeConfigurationType::Max; i++)
    {
        auto &cfg = configuration_types[i];
        ctx.addLine("set(CMAKE_EXE_LINKER_FLAGS_" + cfg + " \"${CMAKE_EXE_LINKER_FLAGS_" + cfg + "} " + s.link_flags_conf[i] + "\")");
        ctx.addLine("set(CMAKE_MODULE_LINKER_FLAGS_" + cfg + " \"${CMAKE_MODULE_LINKER_FLAGS_" + cfg + "} " + s.link_flags_conf[i] + "\")");
        ctx.addLine("set(CMAKE_SHARED_LINKER_FLAGS_" + cfg + " \"${CMAKE_SHARED_LINKER_FLAGS_" + cfg + "} " + s.link_flags_conf[i] + "\")");
        ctx.addLine("set(CMAKE_STATIC_LINKER_FLAGS_" + cfg + " \"${CMAKE_STATIC_LINKER_FLAGS_" + cfg + "} " + s.link_flags_conf[i] + "\")");
        ctx.addLine();
    }

    // should be after flags
    config_section_title(ctx, "CPPAN include");
    ctx.addLine("set(CPPAN_BUILD_OUTPUT_DIR \"" + normalize_path(current_thread_path() / s.output_dir) + "\")");
    ctx.addLine("set(CPPAN_BUILD_SHARED_LIBS "s + (s.use_shared_libs ? "1" : "0") + ")");
    ctx.addLine("set(CPPAN_DISABLE_CHECKS "s + (bs.disable_checks ? "1" : "0") + ")");
    ctx.addLine("set(CPPAN_BUILD_VERBOSE "s + (s.build_system_verbose ? "1" : "0") + ")");
    ctx.addLine("set(CPPAN_BUILD_WARNING_LEVEL "s + std::to_string(s.build_warning_level) + ")");
    ctx.addLine("set(CPPAN_RC_ENABLED "s + (s.rc_enabled ? "1" : "0") + ")");
    ctx.addLine("set(CPPAN_COPY_ALL_LIBRARIES_TO_OUTPUT "s + (s.copy_all_libraries_to_output ? "1" : "0") + ")");
    // build top level executables with input settings
    // otherwise it won't use them
    ctx.addLine("set(CPPAN_BUILD_EXECUTABLES_WITH_SAME_CONFIG 1)");
    ctx.addLine();
    ctx.addLine("add_subdirectory(" + normalize_path(s.cppan_dir) + ")");
    ctx.addLine();

    // trigger building of requested target(s)
    /*ctx.addLine("add_custom_target(cppan_all ALL)");
    ctx.increaseIndent("add_dependencies(cppan_all ");
    for (auto &d : rd[d].dependencies)
        ctx.addLine(d.second.target_name);
    ctx.decreaseIndent(")");
    ctx.addLine();*/

    // vs startup project
    bool once = false;
    for (auto &dep : rd[Package()].dependencies)
    {
        if (!dep.second.flags[pfLocalProject])
            continue;
        if (dep.second.flags[pfExecutable])
        {
            if (!once)
            {
                // this or selected project below
                ctx.addLine("set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT " + dep.second.target_name_hash + ")");
                once = true;
            }
        }
    }
    // TODO: add local setting 'default_project'
    //ctx.addLine("set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT " + ls.default_project + ")");

    file_footer(ctx, d);

    write_file_if_different(bs.source_directory / cmake_config_filename, ctx.getText());
}

int CMakePrinter::generate(const BuildSettings &bs) const
{
    LOG_INFO(logger, "Generating build files...");

    auto &s = Settings::get_local_settings();

    primitives::Command c;
    c.args.push_back("cmake");
    c.args.push_back("-H" + normalize_path(bs.source_directory));
    c.args.push_back("-B" + normalize_path(bs.binary_directory));
    if (!s.c_compiler.empty())
        c.args.push_back("-DCMAKE_C_COMPILER=" + s.c_compiler);
    if (!s.cxx_compiler.empty())
        c.args.push_back("-DCMAKE_CXX_COMPILER=" + s.cxx_compiler);
    if (!s.generator.empty())
    {
        c.args.push_back("-G");
        c.args.push_back(s.generator);
    }
    if (!s.system_version.empty())
        c.args.push_back("-DCMAKE_SYSTEM_VERSION=" + s.system_version);
    if (!s.toolset.empty())
    {
        c.args.push_back("-T");
        c.args.push_back(s.toolset);
    }
    c.args.push_back("-DCMAKE_BUILD_TYPE=" + s.configuration);
    c.args.push_back("-DCPPAN_COMMAND=" + normalize_path(get_program()));
    if (s.debug_generated_cmake_configs)
        c.args.push_back("-DCPPAN_CMAKE_VERBOSE="s + (s.cmake_verbose ? "1" : "0"));
    c.args.push_back("-DCPPAN_BUILD_VERBOSE="s + (s.build_system_verbose ? "1" : "0"));
    c.args.push_back("-DCPPAN_BUILD_WARNING_LEVEL="s + std::to_string(s.build_warning_level));
    //c.args.push_back("-DCPPAN_TEST_RUN="s + (bs.test_run ? "1" : "0"));
    for (auto &o : s.cmake_options)
        c.args.push_back(o);
    for (auto &o : s.env)
    {
#ifdef _WIN32
        _putenv_s(o.first.c_str(), o.second.c_str());
#else
        setenv(o.first.c_str(), o.second.c_str(), 1);
#endif
    }

    c.buf_size = 256; // for frequent flushes
    auto ret = run_command(s, c);

    if (bs.allow_links)
    {
        if (!s.silent || s.is_custom_build_dir())
        {
            auto bld_dir = current_thread_path();
#ifdef _WIN32
            // add more != generators
            if (s.generator != "Ninja")
            {
                auto name = bs.filename_without_ext + "-" + bs.config + ".sln.lnk";
                if (s.is_custom_build_dir())
                {
                    bld_dir = bs.binary_directory / ".." / "..";
                    name = bs.config + ".sln.lnk";
                }
                auto sln = bs.binary_directory / (bs.filename_without_ext + ".sln");
                auto sln_new = bld_dir / name;
                if (fs::exists(sln))
                    create_link(sln, sln_new, "Link to CPPAN Solution");
            }
#else
            if (s.generator == "Xcode")
            {
                auto name = bs.filename_without_ext + "-" + bs.config + ".xcodeproj";
                if (s.is_custom_build_dir())
                {
                    bld_dir = bs.binary_directory / ".." / "..";
                    name = bs.config + ".xcodeproj";
                }
                auto sln = bs.binary_directory / (bs.filename_without_ext + ".xcodeproj");
                auto sln_new = bld_dir / name;
                boost::system::error_code ec;
                fs::create_symlink(sln, sln_new, ec);
            }
            else if (!s.is_custom_build_dir())
            {
                bld_dir /= path(CPPAN_LOCAL_BUILD_PREFIX + bs.filename) / bs.config;
                fs::create_directories(bld_dir);
                boost::system::error_code ec;
                fs::create_symlink(bs.source_directory / cmake_config_filename, bld_dir / cmake_config_filename, ec);
            }
#endif
        }
    }

    return ret.value();
}

int CMakePrinter::build(const BuildSettings &bs) const
{
    LOG_INFO(logger, "Starting build process...");

    primitives::Command c;
    c.args.push_back("cmake");
    c.args.push_back("--build");
    c.args.push_back(normalize_path(bs.binary_directory));
    c.args.push_back("--config");
    c.args.push_back(settings.configuration);

    auto &us = Settings::get_local_settings();
    if (!us.additional_build_args.empty())
    {
        c.args.push_back("--");
        for (auto &a : us.additional_build_args)
            c.args.push_back(a);
    }

    return run_command(settings, c).value();
}

void CMakePrinter::clear_cache() const
{
    auto &sdb = getServiceDatabase();
    auto pkgs = sdb.getInstalledPackages();

    // projects
    for (auto &pkg : pkgs)
    {
        auto d = pkg.getDirObj() / cppan_build_dir;
        if (!fs::exists(d))
            continue;
        for (auto &fc : boost::make_iterator_range(fs::directory_iterator(d), {}))
        {
            if (!fs::is_directory(fc))
                continue;

            auto fn = fc / "CMakeCache.txt";
            remove_file(fn);
        }
    }

    clear_exports();
}

void CMakePrinter::clear_exports() const
{
    auto &sdb = getServiceDatabase();
    auto pkgs = sdb.getInstalledPackages();

    // projects
    for (auto &pkg : pkgs)
        clear_export(pkg.getDirObj());
}

void CMakePrinter::clear_export(const path &p) const
{
    auto d = p / cppan_build_dir;
    if (!fs::exists(d))
        return;
    for (auto &fc : boost::make_iterator_range(fs::directory_iterator(d), {}))
    {
        if (!fs::is_directory(fc))
            continue;

        boost::system::error_code ec;
        fs::remove_all(fc / exports_dir_name, ec);
    }
}

void CMakePrinter::print() const
{
    print_configs();
}

void CMakePrinter::print_meta() const
{
    print_meta_config_file(cwd / settings.cppan_dir / cmake_config_filename);
    print_helper_file(cwd / settings.cppan_dir / cmake_helpers_filename);

    // print inserted files (they'll be printed only once)
    access_table->write_if_older(directories.get_static_files_dir() / cmake_functions_filename,
        "# global options from cppan source code\n"
        "set(CPPAN_CONFIG_HASH_METHOD " CPPAN_CONFIG_HASH_METHOD ")\n"
        "set(CPPAN_CONFIG_HASH_SHORT_LENGTH " + std::to_string(CPPAN_CONFIG_HASH_SHORT_LENGTH) + ")\n"
        "\n"
        "set(CPPAN_CONFIG_PART_DELIMETER -)\n"
        "\n"
        + cmake_functions);

    // register cmake package
#ifdef _WIN32
    winreg::RegKey icon(HKEY_CURRENT_USER, L"Software\\Kitware\\CMake\\Packages\\CPPAN");
    icon.SetStringValue(L"", directories.get_static_files_dir().wstring().c_str());
    access_table->write_if_older(directories.get_static_files_dir() / cppan_cmake_config_filename, cppan_cmake_config);
#else
    access_table->write_if_older(get_home_directory() / ".cmake" / "packages" / cppan_cmake_config_filename, cppan_cmake_config);
#endif

    access_table->write_if_older(directories.get_static_files_dir() / cmake_header_filename, cmake_header);
    access_table->write_if_older(directories.get_static_files_dir() / cmake_export_import_filename, cmake_export_import_file);
    access_table->write_if_older(directories.get_static_files_dir() / cmake_obj_generate_filename, cmake_generate_file);
    access_table->write_if_older(directories.get_static_files_dir() / cmake_obj_build_filename, cmake_build_file);
    access_table->write_if_older(directories.get_static_files_dir() / "branch.rc.in", branch_rc_in);
    access_table->write_if_older(directories.get_static_files_dir() / "version.rc.in", version_rc_in);
    access_table->write_if_older(directories.get_include_dir() / CPP_HEADER_FILENAME, cppan_h);

    if (d.empty())
    {
        // we write some static files to root project anyway
        access_table->write_if_older(cwd / settings.cppan_dir / CPP_HEADER_FILENAME, cppan_h);

        // checks file
        access_table->write_if_older(cwd / settings.cppan_dir / cppan_checks_yml, rd[d].config->getDefaultProject().checks.save());
    }
}

void CMakePrinter::print_configs() const
{
    auto src_dir = d.getDirSrc();
    fs::create_directories(src_dir);

    print_src_config_file(src_dir / cmake_config_filename);
    print_src_actions_file(src_dir / cmake_src_actions_filename);
    print_src_include_file(src_dir / cmake_src_include_guard_filename);

    if (d.flags[pfHeaderOnly])
        return;

    auto obj_dir = d.getDirObj();
    fs::create_directories(obj_dir);

    // print object config files for non-local building
    print_obj_config_file(obj_dir / cmake_config_filename);
    print_obj_generate_file(obj_dir / cmake_obj_generate_filename);
    print_obj_export_file(obj_dir / cmake_obj_exports_filename);
    print_obj_build_file(obj_dir / cmake_obj_build_filename);
}

void CMakePrinter::print_bs_insertion(CMakeContext &ctx, const Project &p, const String &name, const String BuildSystemConfigInsertions::*i) const
{
    config_section_title(ctx, name);

    // project's bsi
    ctx.addLine(p.bs_insertions.*i);
    ctx.emptyLines();

    // options' specific bsi
    for (auto &ol : p.options)
    {
        auto &s = ol.second.bs_insertions.*i;
        if (s.empty())
            continue;

        if (ol.first == "any")
        {
            ctx.addLine(s);
        }
        else
        {
            ctx.if_("\"${LIBRARY_TYPE}\" STREQUAL \"" + boost::algorithm::to_upper_copy(ol.first) + "\"");
            ctx.addLine(s);
            ctx.endif();
            ctx.emptyLines();
        }
    }

    ctx.emptyLines();
}

void CMakePrinter::print_references(CMakeContext &ctx) const
{
    const auto &p = rd[d].config->getDefaultProject();

    config_section_title(ctx, "references");
    for (const auto &dep : p.dependencies)
    {
        auto &dd = dep.second;
        if (dd.reference.empty())
            continue;
        ScopedDependencyCondition sdc(ctx, dd);
        ctx.addLine("set(" + dd.reference + " " + rd[d].dependencies[dd.ppath.toString()].target_name + ")");
        if (dd.ppath.is_loc())
            ctx.addLine("set(" + dd.reference + "_SDIR " + normalize_path(rd.get_local_package_dir(dd.ppath.toString())) + ")");
        else
            ctx.addLine("set(" + dd.reference + "_SDIR " + normalize_path(rd[d].dependencies[dd.ppath.toString()].getDirSrc()) + ")");
        ctx.addLine("set(" + dd.reference + "_BDIR " + normalize_path(rd[d].dependencies[dd.ppath.toString()].getDirObj()) + ")");
        ctx.addLine("set(" + dd.reference + "_DIR ${" + dd.reference + "_SDIR})");
        ctx.addLine();
    }
}

void CMakePrinter::print_settings(CMakeContext &ctx) const
{
    const auto &p = rd[d].config->getDefaultProject();

    config_section_title(ctx, "settings");
    print_storage_dirs(ctx);
    ctx.addLine("set(PACKAGE ${this})");
    ctx.addLine("set(PACKAGE_NAME " + d.ppath.toString() + ")");
    ctx.addLine("set(PACKAGE_NAME_LAST " + d.ppath.back() + ")");
    ctx.addLine("set(PACKAGE_VERSION " + d.version.toString() + ")");
    ctx.addLine("set(PACKAGE_STRING " + d.target_name + ")");
    ctx.addLine("set(PACKAGE_TARNAME)");
    ctx.addLine("set(PACKAGE_URL)");
    ctx.addLine("set(PACKAGE_BUGREPORT)");
    ctx.addLine();

    auto n2hex = [](int n, int w)
    {
        std::ostringstream ss;
        ss << std::hex << std::setfill('0') << std::setw(w) << n;
        return ss.str();
    };

    if (d.version.isBranch())
    {
        ctx.addLine("set(PACKAGE_VERSION_NUM  \"0\")");
        ctx.addLine("set(PACKAGE_VERSION_NUM2 \"0LL\")");
    }
    else
    {
        auto ver2hex = [this, &n2hex](int n)
        {
            std::ostringstream ss;
            ss << n2hex(d.version.major, n);
            ss << n2hex(d.version.minor, n);
            ss << n2hex(d.version.patch, n);
            return ss.str();
        };

        ctx.addLine("set(PACKAGE_VERSION_NUM  \"0x" + ver2hex(2) + "\")");
        ctx.addLine("set(PACKAGE_VERSION_NUM2 \"0x" + ver2hex(4) + "LL\")");
    }
    ctx.addLine();

    ctx.addLine("set(CPPAN_LOCAL_PROJECT "s + (d.flags[pfLocalProject] ? "1" : "0") + ")");
    ctx.addLine();

    // duplicate if someone will do a mistake
    {
        auto v = d.version;
        if (d.flags[pfLocalProject])
        {
            if (p.pkg.version.isValid())
                v = p.pkg.version;
            else
            {
                v.major = 0;
                v.minor = 0;
                v.patch = 0;
            }
        }

        auto print_ver = [&ctx, &v](const String &name)
        {
            auto b = v.isBranch();
            ctx.addLine("set(" + name + "_VERSION_MAJOR " + std::to_string(b ? 0 : v.major) + ")");
            ctx.addLine("set(" + name + "_VERSION_MINOR " + std::to_string(b ? 0 : v.minor) + ")");
            ctx.addLine("set(" + name + "_VERSION_PATCH " + std::to_string(b ? 0 : v.patch) + ")");
            ctx.addLine();
            ctx.addLine("set(" + name + "_MAJOR_VERSION " + std::to_string(b ? 0 : v.major) + ")");
            ctx.addLine("set(" + name + "_MINOR_VERSION " + std::to_string(b ? 0 : v.minor) + ")");
            ctx.addLine("set(" + name + "_PATCH_VERSION " + std::to_string(b ? 0 : v.patch) + ")");
            ctx.addLine();
        };
        print_ver("PACKAGE");
        print_ver("PROJECT");

        ctx.addLine("set(PACKAGE_VERSION_MAJOR_NUM " + n2hex(v.major, 2) + ")");
        ctx.addLine("set(PACKAGE_VERSION_MINOR_NUM " + n2hex(v.minor, 2) + ")");
        ctx.addLine("set(PACKAGE_VERSION_PATCH_NUM " + n2hex(v.patch, 2) + ")");
        ctx.addLine();
    }

    ctx.addLine("set(PACKAGE_IS_BRANCH " + String(d.version.isBranch() ? "1" : "0") + ")");
    ctx.addLine("set(PACKAGE_IS_VERSION " + String(d.version.isVersion() ? "1" : "0") + ")");
    ctx.addLine();
    ctx.addLine("set(LIBRARY_TYPE STATIC)");
    ctx.addLine();
    ctx.if_("CPPAN_BUILD_SHARED_LIBS");
    ctx.addLine("set(LIBRARY_TYPE SHARED)");
    // when linking to shared libs (even if lib is static only)
    // lib must have PIC enabled
    ctx.addLine("set(CMAKE_POSITION_INDEPENDENT_CODE ON)");
    ctx.endif();
    ctx.addLine();
    ctx.if_("NOT \"${LIBRARY_TYPE_${this_variable}}\" STREQUAL \"\"");
    ctx.addLine("set(LIBRARY_TYPE ${LIBRARY_TYPE_${this_variable}})");
    ctx.endif();
    ctx.addLine();

    ctx.addLine("read_variables_file(GEN_CHILD_VARS \"${VARIABLES_FILE}\")");
    ctx.addLine();

    if (!d.flags[pfLocalProject])
    {
        // read check vars file
        ctx.addLine("set(vars_dir \"" + normalize_path(directories.storage_dir_cfg) + "\")");
        ctx.addLine("set(vars_file \"${vars_dir}/${config}.cmake\")");
        ctx.addLine("read_check_variables_file(${vars_file})");
        ctx.addLine();
    }

    ctx.if_("NOT CPPAN_COMMAND");
    ctx.addLine("find_program(CPPAN_COMMAND cppan)");
    ctx.if_("\"${CPPAN_COMMAND}\" STREQUAL \"CPPAN_COMMAND-NOTFOUND\"");
    ctx.addLine("message(WARNING \"'cppan' program was not found. Please, add it to PATH environment variable\")");
    ctx.addLine("set(CPPAN_COMMAND 0)");
    ctx.endif();
    ctx.endif();
    ctx.addLine("set(CPPAN_COMMAND ${CPPAN_COMMAND} CACHE STRING \"CPPAN program.\" FORCE)");
    ctx.addLine();

    if (p.static_only)
        ctx.addLine("set(LIBRARY_TYPE STATIC)");
    else if (p.shared_only)
        ctx.addLine("set(LIBRARY_TYPE SHARED)");
    else if (d.flags[pfHeaderOnly])
        ctx.addLine("set(LIBRARY_TYPE INTERFACE)");
    ctx.emptyLines();
    ctx.addLine("set(EXECUTABLE " + String(d.flags[pfExecutable] ? "1" : "0") + ")");
    ctx.addLine();

    ctx.addLine("set(EXPORT_IF_STATIC " + String(p.export_if_static ? "1" : "0") + ")");
    ctx.addLine();

    print_sdir_bdir(ctx, d);

    ctx.addLine("set(LIBRARY_API " + library_api(d) + ")");
    ctx.addLine();

    // configs
    ctx.addLine("get_configuration_variables()"); // not children
    ctx.addLine();

    // copy exe cmake settings
    ctx.if_("EXECUTABLE AND CPPAN_USE_CACHE");
    ctx.addLine("set(to \"" + normalize_path(directories.storage_dir_cfg) + "/${config}/CMakeFiles/${CMAKE_VERSION}\")");
    ctx.if_("NOT EXISTS ${to}");
    ctx.addLine("execute_process(");
    ctx.addLine("COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_BINARY_DIR}/CMakeFiles/${CMAKE_VERSION} ${to}");
    ctx.addLine("    RESULT_VARIABLE ret");
    ctx.addLine(")");
    ctx.endif();
    ctx.endif();
    ctx.addLine();

    ctx.emptyLines();
}

void CMakePrinter::print_src_config_file(const path &fn) const
{
    if (!must_update_contents(fn))
        return;

    const auto &p = rd[d].config->getDefaultProject();

    CMakeContext ctx;
    file_header(ctx, d);

    // variables for target
    ctx.addLine("set(this " + d.target_name_hash + ")");
    ctx.addLine("set(this_variable " + d.variable_name + ")");
    ctx.addLine();

    // prevent errors
    ctx.if_("TARGET ${this}");
    ctx.addLine("return()");
    ctx.endif();
    ctx.addLine();

    if (!p.condition.empty())
    {
        ctx.if_("NOT (" + p.condition + ")");
        ctx.addLine("return()");
        ctx.endif();
        ctx.addLine();
    }

    // build type
    ctx.if_("NOT CMAKE_BUILD_TYPE");
    ctx.addLine("set_cache_var(CMAKE_BUILD_TYPE " + Settings::get_local_settings().default_configuration + ")");
    ctx.endif();

    print_references(ctx);
    print_dependencies(ctx, d, Settings::get_local_settings().use_cache);
    print_settings(ctx);

    config_section_title(ctx, "export/import");
    ctx.addLine("include(\"" + normalize_path(directories.get_static_files_dir() / cmake_export_import_filename) + "\")");

    print_bs_insertion(ctx, p, "pre sources", &BuildSystemConfigInsertions::pre_sources);

    // sources
    {
        config_section_title(ctx, "sources");
        if (d.flags[pfLocalProject])
        {
            print_local_project_files(ctx, p);
        }
        else if (p.build_files.empty())
        {
            ctx.addLine("file(GLOB_RECURSE src \"*\")");
        }
        else
        {
            ctx.increaseIndent("set(src");
            for (auto &f : p.build_files)
                ctx.addLine("${SDIR}/" + normalize_string_copy(f));
            ctx.decreaseIndent(")");
        }
        ctx.addLine();

        // exclude files
        auto exclude_files = [&ctx](const auto &exclude_from_build)
        {
            if (!exclude_from_build.empty())
            {
                auto cpp_regex_2_cmake_regex = [](auto &s)
                {
                    boost::replace_all(s, ".*", "*");
                };

                config_section_title(ctx, "exclude files");
                for (auto &f : exclude_from_build)
                {
                    // try to remove twice (double check) - as a file and as a dir
                    auto s = normalize_path(f);
                    cpp_regex_2_cmake_regex(s);
                    ctx.addLine("remove_src    (\"" + s + "\")");
                    ctx.addLine("remove_src_dir(\"" + s + "\")");
                    ctx.addLine();
                }
                ctx.emptyLines();
            }
        };
        exclude_files(p.exclude_from_build);

        // exclude main CMakeLists.txt, it is added automatically
        ctx.if_("src");
        ctx.addLine("list(FILTER src EXCLUDE REGEX \".*" + cmake_config_filename + "\")");
        // add CMakeLists.txt from object dir
        if (!p.pkg.flags[pfLocalProject])
            ctx.addLine("set(src ${src} \"" + normalize_path(d.getDirObj() / cmake_config_filename) + "\")");
        else
            ctx.addLine("set(src ${src} \"" + normalize_path(d.getDirSrc() / cmake_config_filename) + "\")");
        ctx.endif();
    }

    print_bs_insertion(ctx, p, "post sources", &BuildSystemConfigInsertions::post_sources);

    for (auto &ol : p.options)
        for (auto &ll : ol.second.link_directories)
            ctx.addLine("link_directories(" + ll + ")");
    ctx.emptyLines();

    // do this right before target
    if (!d.empty() && p.rc_enabled)
    {
        ctx.if_("CPPAN_RC_ENABLED");
        ctx.addLine("add_win32_version_info(\"" + normalize_path(d.getDirObj()) + "\")");
        ctx.endif();
    }

    // warning level, before target
    config_section_title(ctx, "warning levels");
    ctx.addLine(R"(
if (DEFINED CPPAN_BUILD_WARNING_LEVEL AND
    CPPAN_BUILD_WARNING_LEVEL GREATER -1 AND CPPAN_BUILD_WARNING_LEVEL LESS 5)
    if (MSVC)
        # clear old flag (/W3) by default
        #string(REPLACE "/W3" "" CMAKE_C_FLAGS \"${CMAKE_C_FLAGS}\")
        #string(REPLACE "/W3" "" CMAKE_CXX_FLAGS \"${CMAKE_CXX_FLAGS}\")

        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /W${CPPAN_BUILD_WARNING_LEVEL}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W${CPPAN_BUILD_WARNING_LEVEL}")
    endif()
    if (CLANG OR GCC)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")
    endif()
endif()
)");

    // target
    {
        config_section_title(ctx, "target: " + d.target_name);
        if (d.flags[pfExecutable])
        {
            ctx.addLine("add_executable                (${this} " +
                String(p.executable_type == ExecutableType::Win32 ? "WIN32" : "") + " ${src})");
        }
        else
        {
            if (d.flags[pfHeaderOnly])
                ctx.addLine("add_library                   (${this} INTERFACE)");
            else
                ctx.addLine("add_library                   (${this} ${LIBRARY_TYPE} ${src})");
        }
        ctx.addLine();
    }

    // properties
    {
        // standards
        // always off extensions by default
        // if you need gnuXX extensions, set compiler flags in options or post target
        // TODO: propagate standards to dependent packages
        // (i.e. dependent packages should be built >= this standard value)
        if (!d.flags[pfHeaderOnly])
        {
            if (p.c_standard != 0)
                ctx.addLine("set_property(TARGET ${this} PROPERTY C_STANDARD " + std::to_string(p.c_standard) + ")");
            ctx.addLine("set_property(TARGET ${this} PROPERTY C_EXTENSIONS "s + (p.c_extensions ? "ON" : "OFF") + ")");

            ctx.addLine("set_property(TARGET ${this} PROPERTY CXX_EXTENSIONS "s + (p.cxx_extensions ? "ON" : "OFF") + ")");
            if (p.cxx_standard != 0)
            {
                switch (p.cxx_standard)
                {
                case 14:
                    ctx.if_("MSVC");
                    ctx.if_("CLANG");
                    ctx.addLine("target_compile_options(${this} PRIVATE -Xclang -std=c++14)");
                    ctx.else_();
                    ctx.addLine("target_compile_options(${this} PRIVATE -std:c++14)");
                    ctx.endif();
                    ctx.else_();
                    ctx.addLine("set_property(TARGET ${this} PROPERTY CXX_STANDARD " + std::to_string(p.cxx_standard) + ")");
                    ctx.endif();
                    break;
                case 17:
                    ctx.if_("UNIX");
                    // if compiler supports c++17, set it
                    ctx.addLine("target_compile_options(${this} PRIVATE -std=c++1z)");
                    ctx.elseif("MSVC");
                    ctx.if_("CLANG");
                    ctx.addLine("target_compile_options(${this} PRIVATE -Xclang -std=c++1z)");
                    ctx.else_();
                    ctx.addLine("target_compile_options(${this} PRIVATE -std:c++17)");
                    ctx.endif();
                    ctx.else_();
                    ctx.addLine("set_property(TARGET ${this} PROPERTY CXX_STANDARD " + std::to_string(p.cxx_standard) + ")");
                    ctx.endif();
                    break;
                case 20:
                    ctx.if_("UNIX");
                    ctx.addLine("target_compile_options(${this} PRIVATE -std=c++2a)");
                    ctx.elseif("MSVC");
                    ctx.if_("CLANG");
                    ctx.addLine("target_compile_options(${this} PRIVATE -Xclang -std=c++2a)");
                    ctx.else_();
                    ctx.addLine("target_compile_options(${this} PRIVATE -std:c++latest)");
                    ctx.endif();
                    ctx.endif();
                    break;
                default:
                    ctx.addLine("set_property(TARGET ${this} PROPERTY CXX_STANDARD " + std::to_string(p.cxx_standard) + ")");
                    break;
                }
            }
        }
        ctx.emptyLines();

        if (p.export_all_symbols)
        {
            ctx.if_("WIN32 AND (CMAKE_VERSION VERSION_EQUAL 3.6 OR (CMAKE_VERSION VERSION_GREATER 3.6 AND CMAKE_VERSION VERSION_LESS 3.7))");
            ctx.addLine("message(FATAL_ERROR \"You have bugged CMake version 3.6 which is known to not work with CPPAN. Please, upgrade CMake.\")");
            ctx.endif();
            set_target_properties(ctx, "WINDOWS_EXPORT_ALL_SYMBOLS", "True");
            if (d.flags[pfExecutable])
                set_target_properties(ctx, "ENABLE_EXPORTS", "1");
        }
        ctx.emptyLines();

        if (!d.flags[pfHeaderOnly])
        {
            if (!p.output_name.empty())
            {
                set_target_properties(ctx, "OUTPUT_NAME", p.output_name);
            }
            else
            {
                if (!d.flags[pfLocalProject])
                    set_target_properties(ctx, "OUTPUT_NAME", d.target_name);
                else
                {
                    set_target_properties(ctx, "OUTPUT_NAME", Settings::get_local_settings().short_local_names ? d.ppath.back() : d.target_name);
                }
            }
            set_target_properties(ctx, "PROJECT_LABEL", d.flags[pfLocalProject] ? d.ppath.back() : d.target_name);
            ctx.emptyLines();
        }
    }

    // include directories
    {
        std::vector<Package> include_deps;
        for (auto &dep : rd[d].dependencies)
        {
            if (!dep.second.flags[pfIncludeDirectoriesOnly])
                continue;
            include_deps.push_back(dep.second);
        }
        if (!p.include_directories.empty() || !include_deps.empty())
        {
            auto print_ideps = [&ctx, include_deps, this]()
            {
                String visibility = "INTERFACE";
                if (!d.flags[pfHeaderOnly])
                    visibility = d.flags[pfExecutable] ? "PRIVATE" : "PUBLIC";
                visibility += " ";

                for (auto &pkg : include_deps)
                {
                    auto &proj = rd[pkg].config->getDefaultProject();
                    // only public idirs here
                    for (auto &i : proj.include_directories.public_)
                    {
                        path ipath;
                        if (!pkg.flags[pfLocalProject])
                            ipath = pkg.getDirSrc();
                        else
                            ipath = rd.get_local_package_dir(pkg.ppath);
                        ipath /= i;
                        boost::system::error_code ec;
                        if (fs::exists(ipath, ec))
                        {
                            ScopedDependencyCondition sdc(ctx, pkg);
                            ctx.increaseIndent("target_include_directories    (${this}");
                            ctx.addLine(visibility + normalize_path(ipath));
                            ctx.decreaseIndent(")");
                            ctx.emptyLines();
                        }
                    }
                }
            };

            ctx.increaseIndent("target_include_directories    (${this}");
            if (d.flags[pfHeaderOnly])
            {
                for (auto &idir : p.include_directories.public_)
                    ctx.addLine("INTERFACE " + prepare_include_directory(idir.string()));
            }
            else
            {
                for (auto &idir : p.include_directories.public_)
                    // executable can export include dirs too (e.g. flex - FlexLexer.h)
                    // TODO: but check it ^^^
                    // export only exe's idirs, not deps' idirs
                    // that's why target_link_libraries always private for exe
                    ctx.addLine("PUBLIC " + prepare_include_directory(idir.string()));
                for (auto &idir : p.include_directories.private_)
                    ctx.addLine("PRIVATE " + prepare_include_directory(idir.string()));
                for (auto &idir : p.include_directories.interface_)
                    ctx.addLine("INTERFACE " + prepare_include_directory(idir.string()));
            }
            ctx.decreaseIndent(")");
            ctx.emptyLines();

            print_ideps();

            // add BDIRs
            for (auto &pkg : include_deps)
            {
                if (pkg.flags[pfHeaderOnly])
                    continue;

                ScopedDependencyCondition sdc(ctx, pkg);
                ctx.addLine("# Binary dir of include_directories_only dependency");
                ctx.if_("CPPAN_USE_CACHE");

                {
                    auto bdir = pkg.getDirObj() / cppan_build_dir / "${config_dir}";
                    auto p = normalize_path(get_binary_path(pkg, bdir.string()));
                    ctx.if_("EXISTS \"" + p + "\"");
                    ctx.increaseIndent("target_include_directories    (${this}");
                    if (d.flags[pfHeaderOnly])
                        ctx.addLine("INTERFACE " + p);
                    else
                        ctx.addLine((d.flags[pfExecutable] ? "PRIVATE " : "PUBLIC ") + p);
                    ctx.decreaseIndent(")");
                    ctx.endif();
                }

                ctx.else_();

                {
                    auto p = normalize_path(get_binary_path(pkg));
                    ctx.if_("EXISTS \"" + p + "\"");
                    ctx.increaseIndent("target_include_directories    (${this}");
                    if (d.flags[pfHeaderOnly])
                        ctx.addLine("INTERFACE " + p);
                    else
                        ctx.addLine((d.flags[pfExecutable] ? "PRIVATE " : "PUBLIC ") + p);
                    ctx.decreaseIndent(")");
                    ctx.endif();
                }

                ctx.endif();
                ctx.addLine();
                ctx.emptyLines();
            }
        }
    }

    // deps (direct)
    {
        config_section_title(ctx, "dependencies");

        for (auto &[k,v] : rd[d].dependencies)
        {
            if (v.flags[pfExecutable] || v.flags[pfIncludeDirectoriesOnly])
                continue;

            ScopedDependencyCondition sdc(ctx, v);
            ctx.if_("NOT TARGET " + v.target_name + "");
            ctx.addLine("message(FATAL_ERROR \"Target '" + v.target_name + "' is not visible at this place\")");
            ctx.endif();
            ctx.addLine();

            ctx.increaseIndent("target_link_libraries         (${this}");
            if (d.flags[pfHeaderOnly])
                ctx.addLine("INTERFACE " + v.target_name);
            else
                ctx.addLine((v.flags[pfPrivateDependency] ? "PRIVATE" : "PUBLIC") + " "s + v.target_name);
            ctx.decreaseIndent(")");
            ctx.addLine();
        }
    }

    // solution folder
    config_section_title(ctx, "options");
    if (!d.flags[pfHeaderOnly])
    {
        if (!d.flags[pfLocalProject])
            print_solution_folder(ctx, "${this}", path(packages_folder) / d.ppath.toString() / d.version.toString());
        else if (d.ppath.back().find('.') != -1)
        {
            auto f = d.ppath.back();
            auto p = f.rfind('.');
            auto l = f.substr(p + 1);
            f = f.substr(0, p);
            std::replace(f.begin(), f.end(), '.', '/');
            print_solution_folder(ctx, "${this}", f);
            set_target_properties(ctx, "PROJECT_LABEL", l);
        }
        ctx.emptyLines();
    }

    // options (defs, compile options etc.)
    {
        if (!d.flags[pfHeaderOnly])
        {
            // pkg
            ctx.increaseIndent("target_compile_definitions    (${this}");
            ctx.addLine("PRIVATE   PACKAGE=\"" + d.ppath.toString() + "\"");
            ctx.addLine("PRIVATE   PACKAGE_NAME=\"" + d.ppath.toString() + "\"");
            ctx.addLine("PRIVATE   PACKAGE_NAME_LAST=\"" + d.ppath.back() + "\"");
            ctx.addLine("PRIVATE   PACKAGE_VERSION=\"" + d.version.toString() + "\"");
            ctx.addLine("PRIVATE   PACKAGE_STRING=\"${this}\"");
            ctx.addLine("PRIVATE   PACKAGE_BUILD_CONFIG=\"$<CONFIG>\"");
            ctx.addLine("PRIVATE   PACKAGE_BUGREPORT=\"\"");
            ctx.addLine("PRIVATE   PACKAGE_URL=\"\"");
            ctx.addLine("PRIVATE   PACKAGE_COPYRIGHT_YEAR=2017"); // FIXME: take current year
            ctx.decreaseIndent(")");
        }

        // export/import
        ctx.if_("\"${LIBRARY_TYPE}\" STREQUAL \"SHARED\"");
        ctx.increaseIndent("target_compile_definitions    (${this}");
        if (!d.flags[pfHeaderOnly])
        {
            ctx.addLine("PRIVATE   ${LIBRARY_API}"s + (d.flags[pfExecutable] ? "" : "=${CPPAN_EXPORT}"));
            if (!d.flags[pfExecutable])
                ctx.addLine("INTERFACE ${LIBRARY_API}=${CPPAN_IMPORT}");
        }
        else
        {
            if (d.flags[pfExecutable])
                throw std::runtime_error("Header only target should not be executable: " + d.target_name);
            ctx.addLine("INTERFACE ${LIBRARY_API}=");
        }
        ctx.decreaseIndent(")");
        ctx.else_(); // STATIC
        ctx.increaseIndent("target_compile_definitions    (${this}");
        if (!d.flags[pfHeaderOnly])
        {
            if (p.export_if_static)
                // must be public, because when exporting from exe
                // dllexport must be both in library and exe
                ctx.addLine("PUBLIC    ${LIBRARY_API}=${CPPAN_EXPORT}");
            else
                // must be public
                ctx.addLine("PUBLIC    ${LIBRARY_API}=");
        }
        else
            ctx.addLine("INTERFACE ${LIBRARY_API}=");
        ctx.decreaseIndent(")");
        ctx.endif();
        ctx.addLine();

        if (!d.flags[pfExecutable] && !d.flags[pfHeaderOnly])
        {
            set_target_properties(ctx, "INSTALL_RPATH", ".");
            set_target_properties(ctx, "BUILD_WITH_INSTALL_RPATH", "True");
        }
        ctx.addLine();

        for (auto &ol : p.options)
        {
            ctx.emptyLines();

            auto print_target_options = [&ctx, this](const auto &opts, const String &comment, const String &type, const std::function<String(String)> &f = {})
            {
                if (opts.empty())
                    return;
                ctx.addLine("# " + comment);
                ctx.increaseIndent(type + "(${this}");
                for (auto &opt : opts)
                {
                    auto s = opt.second;
                    if (f)
                        s = f(s);
                    if (d.flags[pfHeaderOnly])
                        ctx.addLine("INTERFACE " + s);
                    else if (d.flags[pfExecutable])
                        ctx.addLine("PRIVATE " + s);
                    else
                        ctx.addLine(boost::algorithm::to_upper_copy(opt.first) + " " + s);
                }
                ctx.decreaseIndent(")");
            };

            auto print_defs = [&print_target_options](const auto &defs)
            {
                print_target_options(defs, "definitions", "target_compile_definitions");
            };
            auto print_include_dirs = [&print_target_options](const auto &defs)
            {
                print_target_options(defs, "include directories", "target_include_directories", &prepare_include_directory);
            };
            auto print_compile_opts = [&print_target_options](const auto &copts)
            {
                print_target_options(copts, "compile options", "target_compile_options");
            };
            auto print_linker_opts = [&print_target_options](const auto &lopts)
            {
                print_target_options(lopts, "link options", "target_link_libraries");
            };
            /*auto print_set = [&ctx, this](const auto &a, const String &s)
            {
                if (a.empty())
                    return;
                ctx.increaseIndent(s + "(${this}");
                for (auto &def : a)
                {
                    String i;
                    if (d.flags[pfHeaderOnly])
                        i = "INTERFACE";
                    else if (d.flags[pfExecutable])
                        i = "PRIVATE";
                    else
                        i = "PUBLIC";
                    ctx.addLine(i + " " + def);
                }
                ctx.decreaseIndent(")");
                ctx.addLine();
            };*/
            auto print_options = [&ctx, &ol, &print_defs, &print_compile_opts, &print_linker_opts, &print_include_dirs]
            {
                print_defs(ol.second.definitions);
                print_include_dirs(ol.second.include_directories);
                print_compile_opts(ol.second.compile_options);
                print_linker_opts(ol.second.link_options);
                print_linker_opts(ol.second.link_libraries);

                auto print_system = [&ctx](const auto &a, auto f)
                {
                    for (auto &kv : a)
                    {
                        auto k = boost::to_upper_copy(kv.first);
                        ctx.if_("" + k + "");
                        f(kv.second);
                        ctx.endif();
                    }
                };

                print_system(ol.second.system_definitions, print_defs);
                print_system(ol.second.system_include_directories, print_include_dirs);
                print_system(ol.second.system_compile_options, print_compile_opts);
                print_system(ol.second.system_link_options, print_linker_opts);
                print_system(ol.second.system_link_libraries, print_linker_opts);
            };

            if (ol.first == "any")
            {
                print_options();
            }
            else
            {
                ctx.if_("\"${LIBRARY_TYPE}\" STREQUAL \"" + boost::algorithm::to_upper_copy(ol.first) + "\"");
                print_options();
                ctx.endif();
            }
        }
        ctx.emptyLines();
    }

    print_bs_insertion(ctx, p, "post target", &BuildSystemConfigInsertions::post_target);

    // private definitions
    if (!d.flags[pfHeaderOnly])
    {
        config_section_title(ctx, "private definitions");

        // some compiler options
        ctx.addLine(R"(if (MSVC)
    target_compile_definitions(${this}
        PRIVATE _CRT_SECURE_NO_WARNINGS # disable warning about non-standard functions
    )
    target_compile_options(${this}
        PRIVATE /wd4005 # macro redefinition
        PRIVATE /wd4996 # The POSIX name for this item is deprecated.
    )
endif()

if (CLANG)
    target_compile_options(${this}
        PRIVATE -Wno-macro-redefined
    )
endif()
)");
    }

    // public definitions
    {
        // visibility is set for some common vars
        // the appropriate value is selected based on target type
        // do not remove!
        String visibility;
        if (!d.flags[pfExecutable])
            visibility = !d.flags[pfHeaderOnly] ? "PUBLIC" : "INTERFACE";
        else
            visibility = "PRIVATE";

        config_section_title(ctx, "public definitions");

        // common include directories
        ctx.increaseIndent("target_include_directories(${this}");
        ctx.addLine(visibility + " ${SDIR}"); // why??? add an explanation
        ctx.decreaseIndent(")");
        ctx.addLine();

        // common definitions
        // this variables do a small but very important thing
        // they expose CPPAN vars to their users
        // do not remove!
        ctx.increaseIndent("target_compile_definitions(${this}");
        ctx.addLine(visibility + " CPPAN"); // build is performed under CPPAN
        ctx.addLine(visibility + " CPPAN_BUILD"); // build is performed under CPPAN
        if (!d.flags[pfHeaderOnly])
        {
            // CPPAN_CONFIG is private for a package!
            ctx.addLine("PRIVATE CPPAN_CONFIG=\"${config}\"");
        }
        for (auto &a : p.api_name)
            ctx.addLine(visibility + " " + a + "=${LIBRARY_API}");
        ctx.decreaseIndent(")");
        ctx.addLine();

        // CPPAN_EXPORT is a macro that will be expanded
        // to proper export/import decls after install from server
        if (d.flags[pfLocalProject])
        {
            ctx.increaseIndent("target_compile_definitions(${this}");
            ctx.addLine(visibility + " CPPAN_EXPORT=");
            ctx.decreaseIndent(")");
            ctx.addLine();
        }

        // common link libraries
        if (!d.flags[pfHeaderOnly])
        {
            ctx.increaseIndent(R"(if (WIN32)
    target_link_libraries(${this}
        PUBLIC Ws2_32
    )
else())");
            auto add_unix_lib = [&ctx](const String &s)
            {
                ctx.addLine("find_library(" + s + " " + s + ")");
                ctx.if_("NOT ${" + s + "} STREQUAL \"" + s + "-NOTFOUND\"");
                ctx.addLine("target_link_libraries(${this}");
                ctx.addLine("    PUBLIC " + s + "");
                ctx.addLine(")");
                ctx.endif();
            };
            add_unix_lib("m");
            add_unix_lib("pthread");
            add_unix_lib("rt");
            add_unix_lib("dl");
            ctx.endif();
            ctx.addLine();
        }
    }

    // definitions
    config_section_title(ctx, "definitions");
    p.checks.write_definitions(ctx, d, p.checks_prefixes);

    // build deps
    print_build_dependencies(ctx, "${this}");

    // copy deps for local projects
    // this is needed for executables that may go to custom folder but without deps
    if (d.flags[pfLocalProject] && !d.flags[pfHeaderOnly])
        print_copy_dependencies(ctx, "${this}");

    // export
    config_section_title(ctx, "export");
    ctx.addLine("export(TARGETS ${this} FILE " + exports_dir + "${this_variable}.cmake)");
    ctx.emptyLines();

    // aliases
    {
        // target type
        const String tt = d.flags[pfExecutable] ? "add_executable" : "add_library";

        config_section_title(ctx, "aliases");
        add_aliases(ctx, d, [&tt](const auto &s, const auto &v)
        {
            return tt + "(" + s + " ALIAS ${this})";
        });

        if (d.flags[pfLocalProject])
        {
            ctx.addLine(tt + "(" + d.ppath.back() + " ALIAS ${this})");
            ctx.emptyLines();
        }
    }

    print_bs_insertion(ctx, p, "post alias", &BuildSystemConfigInsertions::post_alias);

    // test
    {
        // cotire
        //ctx.addLine("cppan_include(" + normalize_path(directories.get_static_files_dir() / "cotire.cmake") + ")");
        //ctx.addLine("cotire(${this})");
    }

    // dummy target for IDEs with headers only
    if (d.flags[pfHeaderOnly])
    {
        config_section_title(ctx, "IDE dummy target for headers");

        String tgt = "${this}-headers";
        ctx.if_("CPPAN_SHOW_IDE_PROJECTS");
        ctx.addLine("add_custom_target(" + tgt + " SOURCES ${src})");
        ctx.addLine();
        print_solution_folder(ctx, tgt, path(packages_folder) / d.ppath.toString() / d.version.toString());
        ctx.endif();
        ctx.emptyLines();
    }

    // source groups
    print_source_groups(ctx);

    file_footer(ctx, d);

    write_if_older(fn, ctx.getText());
}

void CMakePrinter::print_src_actions_file(const path &fn) const
{
    if (!must_update_contents(fn))
        return;

    const auto &p = rd[d].config->getDefaultProject();

    CMakeContext ctx;
    file_header(ctx, d);

    // build type
    ctx.if_("NOT CMAKE_BUILD_TYPE");
    ctx.addLine("set_cache_var(CMAKE_BUILD_TYPE" + Settings::get_local_settings().default_configuration + ")");
    ctx.endif();

    ctx.addLine(config_delimeter);
    ctx.addLine();
    ctx.addLine("set(CMAKE_CURRENT_SOURCE_DIR_OLD ${CMAKE_CURRENT_SOURCE_DIR})");
    ctx.addLine("set(CMAKE_CURRENT_SOURCE_DIR \"" + normalize_path(fn.parent_path().string()) + "\")");
    ctx.addLine("set(CMAKE_CURRENT_BINARY_DIR_OLD ${CMAKE_CURRENT_BINARY_DIR})");
    ctx.addLine("set(CMAKE_CURRENT_BINARY_DIR \"" + normalize_path(get_binary_path(d)) + "\")");
    ctx.addLine();
    print_sdir_bdir(ctx, d);
    ctx.addLine("set(LIBRARY_API " + library_api(d) + ")");
    ctx.emptyLines();
    print_bs_insertion(ctx, p, "pre sources", &BuildSystemConfigInsertions::pre_sources);
    ctx.addLine();
    ctx.addLine("file(GLOB_RECURSE src \"*\")");
    ctx.addLine();
    print_bs_insertion(ctx, p, "post sources", &BuildSystemConfigInsertions::post_sources);
    ctx.addLine();
    print_bs_insertion(ctx, p, "post target", &BuildSystemConfigInsertions::post_target);
    ctx.addLine();
    print_bs_insertion(ctx, p, "post alias", &BuildSystemConfigInsertions::post_alias);
    ctx.addLine();
    ctx.addLine("set(CMAKE_CURRENT_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR_OLD})");
    ctx.addLine("set(CMAKE_CURRENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR_OLD})");
    ctx.addLine();

    file_footer(ctx, d);

    write_if_older(fn, ctx.getText());
}

void CMakePrinter::print_src_include_file(const path &fn) const
{
    if (!must_update_contents(fn))
        return;

    CMakeContext ctx;
    file_header(ctx, d);

    ctx.if_("TARGET " + d.target_name + "");
    ctx.addLine("return()");
    ctx.endif();
    ctx.addLine();
    if (d.flags[pfLocalProject])
        ctx.addLine("cppan_include(\"" + normalize_path(fn.parent_path().string()) + "/" + cmake_config_filename + "\")");
    else
        ctx.addLine("add_subdirectory(\"" + normalize_path(fn.parent_path().string()) + "\" \"" + get_binary_path(d) + "\")");
    ctx.addLine();

    file_footer(ctx, d);

    write_if_older(fn, ctx.getText());
}

void CMakePrinter::print_obj_config_file(const path &fn) const
{
    if (!must_update_contents(fn))
        return;

    const auto &p = rd[d].config->getDefaultProject();

    CMakeContext ctx;
    file_header(ctx, d);

    {
        config_section_title(ctx, "cmake settings");
        ctx.addLine(cmake_minimum_required);
        ctx.addLine();
        config_section_title(ctx, "macros & functions");
        ctx.addLine("include(" + normalize_path(directories.get_static_files_dir() / cmake_functions_filename) + ")");
        ctx.addLine();
        //if (!d.flags[pfLocalProject])
        {
            config_section_title(ctx, "read passed variables");
            if (d.flags[pfLocalProject])
                ctx.if_("VARIABLES_FILE");
            ctx.addLine("read_variables_file(GEN_CHILD_VARS \"${VARIABLES_FILE}\")");
            if (d.flags[pfLocalProject])
            {
                ctx.else_();
                ctx.addLine("set(OUTPUT_DIR ${config})");
                ctx.endif();
            }
            ctx.addLine();
        }
        //else
        {
            //config_section_title(ctx, "setup variables");
            //ctx.addLine("set(OUTPUT_DIR ${config})");
        }
        ctx.addLine();

        config_section_title(ctx, "global settings");
        ctx.addLine(R"(if (NOT CMAKE_BUILD_TYPE)
    set_cache_var(CMAKE_BUILD_TYPE )" + Settings::get_local_settings().default_configuration + R"()
endif()

# TODO:
#set_property(GLOBAL APPEND PROPERTY JOB_POOLS compile_job_pool=8)
#set(CMAKE_JOB_POOL_COMPILE compile_job_pool)
)");

        config_section_title(ctx, "output settings");
        ctx.if_("NOT DEFINED CPPAN_USE_CACHE");
        ctx.if_("NOT (VISUAL_STUDIO OR XCODE)");
        ctx.addLine("set(output_dir_suffix ${CMAKE_BUILD_TYPE})");
        ctx.endif();
        ctx.addLine();
        ctx.addLine("set(CMAKE_RUNTIME_OUTPUT_DIRECTORY " + normalize_path(directories.storage_dir_bin) + "/${OUTPUT_DIR}/${output_dir_suffix})");
        ctx.addLine("set(CMAKE_LIBRARY_OUTPUT_DIRECTORY " + normalize_path(directories.storage_dir_lib) + "/${OUTPUT_DIR}/${output_dir_suffix})");
        ctx.addLine("set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY " + normalize_path(directories.storage_dir_lib) + "/${OUTPUT_DIR}/${output_dir_suffix})");
        ctx.addLine();

        ctx.addLine("set(CPPAN_USE_CACHE 1)");
        ctx.endif();
    }

    print_bs_insertion(ctx, p, "pre project", &BuildSystemConfigInsertions::pre_project);

    // no need to create a solution for local project
    config_section_title(ctx, "project settings");
    ctx.addLine("project(" + d.getHashShort() + " LANGUAGES C CXX)");
    ctx.addLine();

    print_bs_insertion(ctx, p, "post project", &BuildSystemConfigInsertions::post_project);

    config_section_title(ctx, "compiler & linker settings");
    ctx.addLine(R"(
if (MSVC)
    if (NOT CLANG)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
    endif()

    # not working for some reason
    #set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} /nologo")

    if (CPPAN_MT_BUILD)
        set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /MT")
        set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} /MT")
        set(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL} /MT")
        set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /MTd")

        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MT")
        set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL} /MT")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
    endif()
endif()
)");

    config_section_title(ctx, "cppan setup");
    ctx.addLine("add_subdirectory(" + normalize_path(settings.cppan_dir) + ")");

    // main include
    {
        config_section_title(ctx, "main include");
        auto mi = d.getDirSrc();
        add_subdirectory(ctx, mi.string());
        ctx.emptyLines();
    }

    file_footer(ctx, d);

    write_if_older(fn, ctx.getText());
}

void CMakePrinter::print_obj_generate_file(const path &fn) const
{
    if (!must_update_contents(fn))
        return;

    const auto &p = rd[d].config->getDefaultProject();

    CMakeContext ctx;
    file_header(ctx, d);

    ctx.addLine("set(target " + d.target_name + ")");
    ctx.addLine();
    if (!p.aliases.empty())
    {
        ctx.increaseIndent("set(aliases");
        for (auto &a : p.aliases)
            ctx.addLine(a);
        ctx.decreaseIndent(")");
        ctx.addLine();
    }
    ctx.addLine("set(current_dir " + normalize_path(fn.parent_path()) + ")");
    ctx.addLine("set(storage_dir_cfg " + normalize_path(directories.storage_dir_cfg) + ")");
    ctx.addLine("set(storage_dir_exp " + normalize_path(directories.storage_dir_exp) + ")");
#ifdef _WIN32
    ctx.addLine("set(storage_dir_lnk " + normalize_path(directories.storage_dir_lnk) + ")");
#endif
    ctx.addLine();
    ctx.addLine("set(variable_name " + d.variable_name + ")");
    ctx.addLine("set(package_hash_short " + d.getHashShort() + ")");
    ctx.addLine();
    ctx.addLine("set(EXECUTABLE " + String(d.flags[pfExecutable] ? "1" : "0") + ")");
    ctx.addLine();

    ctx.addLine("include(" + normalize_path(directories.get_static_files_dir() / cmake_obj_generate_filename) + ")");

    // executable (non-direct) is the last in the chain
    // we do not use its exported symbols or whatever
    if (!(d.flags[pfExecutable] && !d.flags[pfDirectDependency]))
    {
        config_section_title(ctx, "import direct deps");
        ctx.addLine("cppan_include(${current_dir}/exports.cmake)");
        ctx.addLine();
    }

    config_section_title(ctx, "include current export file");
    ctx.if_("NOT TARGET " + d.target_name + "");
    ctx.addLine("cppan_include(${import_fixed})");
    ctx.endif();
    ctx.addLine();

    // src target
    {
        auto target = d.target_name + "-sources";
        auto dir = d.getDirSrc();

        // begin
        if (!d.flags[pfLocalProject])
        {
            ctx.if_("CPPAN_SHOW_IDE_PROJECTS");
            ctx.addLine();
        }
        config_section_title(ctx, "sources target (for IDE only)");
        ctx.if_("NOT TARGET " + target + "");
        if (d.flags[pfLocalProject])
        {
            ctx.addLine("set(SDIR " + normalize_path(p.root_directory) + ")");
            print_local_project_files(ctx, p);
            ctx.addLine("set(SDIR)");
        }
        else
            ctx.addLine("file(GLOB_RECURSE src \"" + normalize_path(dir) + "/*\")");
        ctx.addLine();
        ctx.addLine("add_custom_target(" + target);
        ctx.addLine("    SOURCES ${src}");
        ctx.addLine(")");
        ctx.addLine();

        // solution folder
        if (!d.flags[pfLocalProject])
            print_solution_folder(ctx, target, path(packages_folder) / d.ppath.toString() / d.version.toString());
        ctx.endif();
        ctx.emptyLines();

        // source groups
        print_source_groups(ctx);

        // end
        if (!d.flags[pfLocalProject])
            ctx.endif();
    }

    file_footer(ctx, d);

    write_if_older(fn, ctx.getText());
}

void CMakePrinter::print_obj_export_file(const path &fn) const
{
    if (!must_update_contents(fn))
        return;

    CMakeContext ctx;
    file_header(ctx, d);

    // before every export include 'cmake_obj_generate_filename'
    // set CPPAN_BUILD_EXECUTABLES_WITH_SAME_CONFIG var
    ctx.addLine("set(CPPAN_BUILD_EXECUTABLES_WITH_SAME_CONFIG "s + (
        rd[d].config->getDefaultProject().build_dependencies_with_same_config ? "1" : "0") + ")");
    ctx.addLine();

    // we skip executables because they may introduce wrong targets
    // (dependent libraries in static config instead of shared)
    if (!d.flags[pfDirectDependency] && d.flags[pfExecutable])
        ctx.if_("CPPAN_BUILD_EXECUTABLES_WITH_SAME_CONFIG");

    for (auto &dp : rd[d].dependencies)
    {
        auto &dep = dp.second;

        if (dep.flags[pfIncludeDirectoriesOnly])
            continue;

        auto b = dep.getDirObj();
        auto p = directories.storage_dir_exp / "${config_dir}" / (dep.target_name + ".cmake");

        ScopedDependencyCondition sdc(ctx, dep);
        if (!dep.flags[pfHeaderOnly])
            ctx.addLine("cppan_include(\"" + normalize_path(b / cmake_obj_generate_filename) + "\")");
        ctx.if_("NOT TARGET " + dep.target_name + "");
        if (dep.flags[pfHeaderOnly])
            add_subdirectory(ctx, dep.getDirSrc().string());
        else
        {
            ctx.if_("NOT EXISTS \"" + normalize_path(p) + "\"");
            ctx.addLine("cppan_include(\"" + normalize_path(b / cmake_obj_generate_filename) + "\")");
            ctx.endif();
            ctx.addLine("cppan_include(\"" + normalize_path(p) + "\")");
        }
        ctx.endif();
        ctx.addLine();
    }

    if (!d.flags[pfDirectDependency] && d.flags[pfExecutable])
        ctx.endif();

    file_footer(ctx, d);

    write_if_older(fn, ctx.getText());
}

void CMakePrinter::print_obj_build_file(const path &fn) const
{
    if (!must_update_contents(fn))
        return;

    CMakeContext ctx;
    file_header(ctx, d);

    ctx.addLine("set(PACKAGE_NAME " + d.ppath.toString() + ")");
    ctx.addLine("set(PACKAGE_STRING " + d.target_name + ")");

    config_section_title(ctx, "macros & functions");
    ctx.addLine("include(" + normalize_path(directories.get_static_files_dir() / cmake_functions_filename) + ")");

    ctx.addLine("set(fn1 \"" + normalize_path(d.getStampFilename()) + "\")");
    ctx.addLine("set(fn2 \"${BUILD_DIR}/" + cppan_stamp_filename + "\")");
    ctx.addLine();

    ctx.addLine("include(" + normalize_path(directories.get_static_files_dir() / cmake_obj_build_filename) + ")");

    file_footer(ctx, d);

    write_if_older(fn, ctx.getText());
}

void CMakePrinter::print_meta_config_file(const path &fn) const
{
    if (!must_update_contents(fn))
        return;

    /*const auto &ls = */Settings::get_local_settings();

    CMakeContext ctx;
    file_header(ctx, d, true);

    ctx.addLine("#");
    ctx.addLine("# cppan");
    ctx.addLine("# meta config file");
    ctx.addLine("#");
    ctx.addLine();

    if (d.empty())
    {
        ctx.addLine("set(CPPAN_DEBUG_STACK_SPACE \"\" CACHE STRING \"\" FORCE)");
        ctx.addLine();
    }

    config_section_title(ctx, "cmake setup");
    ctx.addLine(cmake_minimum_required);

    config_section_title(ctx, "macros & functions");
    ctx.addLine("include(" + normalize_path(directories.get_static_files_dir() / cmake_functions_filename) + ")");

    print_sdir_bdir(ctx, d);

    config_section_title(ctx, "variables");
    ctx.addLine("set(CPPAN_BUILD 1 CACHE STRING \"CPPAN is turned on\")");
    ctx.addLine();
    print_storage_dirs(ctx);
    ctx.addLine("set_cache_var(CMAKE_POSITION_INDEPENDENT_CODE ON)");
    ctx.addLine();
    ctx.addLine("set_cache_var(${CMAKE_CXX_COMPILER_ID} 1)");
    ctx.addLine();
    ctx.if_("NOT DEFINED CPPAN_USE_CACHE");
    ctx.addLine("set_cache_var(CPPAN_USE_CACHE "s + (settings.use_cache ? "1" : "0") + ")");
    ctx.endif();
    ctx.addLine();
    ctx.if_("NOT DEFINED CPPAN_SHOW_IDE_PROJECTS");
    ctx.addLine("set_cache_var(CPPAN_SHOW_IDE_PROJECTS "s + (settings.show_ide_projects ? "1" : "0") + ")");
    ctx.endif();
    ctx.addLine();
    //ctx.if_("NOT DEFINED CPPAN_BUILD_EXECUTABLES_WITH_SAME_CONFIG");
    //ctx.addLine("set(CPPAN_BUILD_EXECUTABLES_WITH_SAME_CONFIG 0)");
    //ctx.endif();
    //ctx.addLine();
    ctx.if_("NOT DEFINED CPPAN_BUILD_EXECUTABLES_WITH_SAME_CONFIGURATION");
    ctx.addLine("set_cache_var(CPPAN_BUILD_EXECUTABLES_WITH_SAME_CONFIGURATION 0)");
    ctx.endif();
    ctx.addLine();
    ctx.if_("NOT DEFINED CPPAN_BUILD_VERBOSE");
    ctx.addLine("set_cache_var(CPPAN_BUILD_VERBOSE "s + (settings.build_system_verbose ? "1" : "0") + ")");
    ctx.endif();
    ctx.if_("NOT DEFINED CPPAN_BUILD_SHARED_LIBS");
    ctx.addLine("set_cache_var(CPPAN_BUILD_SHARED_LIBS "s + (settings.use_shared_libs ? "1" : "0") + ")");
    ctx.endif();
    ctx.addLine();
    ctx.if_("NOT DEFINED CPPAN_BUILD_WARNING_LEVEL");
    ctx.addLine("set_cache_var(CPPAN_BUILD_WARNING_LEVEL "s + std::to_string(settings.build_warning_level) + ")");
    ctx.endif();
    ctx.if_("NOT DEFINED CPPAN_RC_ENABLED");
    ctx.addLine("set_cache_var(CPPAN_RC_ENABLED "s + (settings.rc_enabled ? "1" : "0") + ")");
    ctx.endif();
    ctx.addLine(R"(
if (VISUAL_STUDIO AND CLANG AND NINJA_FOUND AND NOT NINJA)
    set_cache_var(VISUAL_STUDIO_ACCELERATE_CLANG 1)
endif()
)");
    ctx.addLine();
    ctx.addLine("get_configuration_variables()"); // not children
    ctx.addLine();

    ctx.addLine("include(" + cmake_helpers_filename + ")");
    ctx.addLine();

    // deps
    print_references(ctx);
    print_dependencies(ctx, d, settings.use_cache);

    if (d.empty())
    {
        String old_cppan_target = add_target_suffix(cppan_project_name);

        // lib
        config_section_title(ctx, "main library");
        ctx.addLine("add_library                   (" + old_cppan_target + " INTERFACE)");
        for (auto &p : rd[d].dependencies)
        {
            if (p.second.flags[pfExecutable] || p.second.flags[pfIncludeDirectoriesOnly])
                continue;
            ScopedDependencyCondition sdc(ctx, p.second);
            ctx.increaseIndent("target_link_libraries         (" + old_cppan_target);
            ctx.addLine("INTERFACE " + p.second.target_name);
            ctx.decreaseIndent(")");
        }
        ctx.addLine("add_dependencies(" + old_cppan_target + " " + cppan_dummy_target(cppan_dummy_copy_target) + ")");
        ctx.addLine();
        ctx.addLine("export(TARGETS " + old_cppan_target + " FILE " + exports_dir + "cppan.cmake)");

        // re-run cppan when root cppan.yml is changed
        if (settings.add_run_cppan_target)
        {
            config_section_title(ctx, "cppan regenerator");
            ctx.addLine(R"(set(file ${CMAKE_CURRENT_BINARY_DIR}/run-cppan.txt)
add_custom_command(OUTPUT ${file}
    COMMAND ${CPPAN_COMMAND} -d ${PROJECT_SOURCE_DIR}
    COMMAND ${CMAKE_COMMAND} -E echo "" > ${file}
    DEPENDS ${SDIR}/cppan.yml
)
add_custom_target(run-cppan
    DEPENDS ${file}
    SOURCES
        ${SDIR}/cppan.yml
        \")" + normalize_path(directories.get_static_files_dir() / cmake_functions_filename) + R"(\"
        ${PROJECT_SOURCE_DIR}/cppan/)" + cmake_helpers_filename + R"(
)
add_dependencies()" + old_cppan_target + R"( run-cppan)
)");
            print_solution_folder(ctx, "run-cppan", service_folder);
        }

        print_build_dependencies(ctx, cppan_dummy_target(cppan_dummy_build_target));
        print_copy_dependencies(ctx, cppan_dummy_target(cppan_dummy_copy_target));
        //ctx.addLine("add_dependencies(" + cppan_dummy_target(cppan_dummy_copy_target) + " " + cppan_dummy_target(cppan_dummy_build_target) + ")");

        // groups for local projects
        config_section_title(ctx, "local project groups");
        Packages out;
        gather_build_deps({ { "", d } }, out, true);
        ctx.if_("CPPAN_HIDE_LOCAL_DEPENDENCIES");
        for (auto &dep : out)
        {
            if (dep.second.flags[pfLocalProject])
                print_solution_folder(ctx, dep.second.target_name_hash, local_dependencies_folder);
        }
        ctx.endif();
        ctx.emptyLines();

        for (auto &dep : rd[d].dependencies)
        {
            if (!dep.second.flags[pfLocalProject])
                continue;
            if (dep.second.flags[pfExecutable])
            {
                ScopedDependencyCondition sdc(ctx, dep.second);
                ctx.addLine("set_target_properties(" + dep.second.target_name_hash + " PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CPPAN_BUILD_OUTPUT_DIR})");
            }
        }
    }

    file_footer(ctx, d);

    write_if_older(fn, ctx.getText());
}

void CMakePrinter::print_helper_file(const path &fn) const
{
    if (!must_update_contents(fn))
        return;

    const auto &p = rd[d].config->getDefaultProject();

    CMakeContext ctx;
    file_header(ctx, d);

    ctx.addLine("#");
    ctx.addLine("# cppan");
    ctx.addLine("# helper routines");
    ctx.addLine("#");
    ctx.addLine();

    config_section_title(ctx, "cmake setup");
    ctx.addLine(R"(# Use solution folders.
set_property(GLOBAL PROPERTY USE_FOLDERS ON))");
    ctx.addLine();

    config_section_title(ctx, "variables");
    if (d.empty())
    {
        ctx.if_("NOT CPPAN_COMMAND");
        ctx.addLine("find_program(CPPAN_COMMAND cppan)");
        ctx.if_("\"${CPPAN_COMMAND}\" STREQUAL \"CPPAN_COMMAND-NOTFOUND\"");
        ctx.addLine("message(WARNING \"'cppan' program was not found. Please, add it to PATH environment variable\")");
        ctx.addLine("set_cache_var(CPPAN_COMMAND 0)");
        ctx.endif();
        ctx.endif();
        ctx.addLine("set_cache_var(CPPAN_COMMAND ${CPPAN_COMMAND} CACHE STRING \"CPPAN program.\" FORCE)");
        ctx.addLine();
    }
    ctx.addLine(R"xxx(
set_cache_var(XCODE 0)
if (CMAKE_GENERATOR STREQUAL Xcode)
    set_cache_var(XCODE 1)
endif()

set_cache_var(NINJA 0)
if (CMAKE_GENERATOR STREQUAL Ninja)
    set_cache_var(NINJA 1)
endif()

find_program(ninja ninja)
if (NOT "${ninja}" STREQUAL "ninja-NOTFOUND")
    set_cache_var(NINJA_FOUND 1)
elseif()
    find_program(ninja ninja-build)
    if (NOT "${ninja}" STREQUAL "ninja-NOTFOUND")
        set_cache_var(NINJA_FOUND 1)
    endif()
endif()

set_cache_var(VISUAL_STUDIO 0)
if (MSVC AND NOT NINJA)
    set_cache_var(VISUAL_STUDIO 1)
endif()

set_cache_var(CLANG 0)
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
    set_cache_var(CLANG 1)
endif()
if (CMAKE_VS_PLATFORM_TOOLSET MATCHES "(v[0-9]+_clang_.*|LLVM-vs[0-9]+.*)")
    set_cache_var(CLANG 1)
endif()

if (VISUAL_STUDIO AND CLANG AND NOT NINJA_FOUND)
    message(STATUS "Warning: Build with MSVC and Clang without ninja will be single threaded - very very slow.")
endif()

if (VISUAL_STUDIO AND CLANG AND NINJA_FOUND AND NOT NINJA)
    set_cache_var(VISUAL_STUDIO_ACCELERATE_CLANG 1)
    #if ("${CMAKE_LINKER}" STREQUAL "CMAKE_LINKER-NOTFOUND")
    #    message(FATAL_ERROR "CMAKE_LINKER must be set in order to accelerate clang build with MSVC!")
    #endif()
endif()
)xxx");

    // after all vars are set
    //ctx.addLine("set(CPPAN_CONFIG_NO_BUILD_TYPE 1)");
    ctx.addLine("get_configuration(config)"); // not children
    ctx.addLine("get_configuration_with_generator(config_dir)");
    //ctx.addLine("set(CPPAN_CONFIG_NO_BUILD_TYPE 0)");
    ctx.addLine("get_configuration_unhashed(config_name)");
    ctx.addLine("get_configuration_with_generator_unhashed(config_gen_name)");
    ctx.addLine("get_number_of_cores(N_CORES)");
    ctx.addLine();

    // after config
    ctx.addLine("file_write_once(${PROJECT_BINARY_DIR}/" CPPAN_CONFIG_FILENAME " \"${config_gen_name}\")");
    ctx.addLine();

    // use response files when available
    ctx.addLine("set_cache_var(CMAKE_C_USE_RESPONSE_FILE_FOR_INCLUDES    1)");
    ctx.addLine("set_cache_var(CMAKE_C_USE_RESPONSE_FILE_FOR_OBJECTS     1)");
    ctx.addLine("set_cache_var(CMAKE_C_USE_RESPONSE_FILE_FOR_LIBRARIES   1)");
    ctx.addLine("set_cache_var(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES  1)");
    ctx.addLine("set_cache_var(CMAKE_CXX_USE_RESPONSE_FILE_FOR_OBJECTS   1)");
    ctx.addLine("set_cache_var(CMAKE_CXX_USE_RESPONSE_FILE_FOR_LIBRARIES 1)");
    // unknown meaning atm
    ctx.addLine("set_cache_var(CMAKE_CXX_RESPONSE_FILE_LINK_FLAG \"@\")");
    ctx.addLine();

    // cmake includes
    config_section_title(ctx, "cmake includes");
    ctx.addLine(cmake_includes);

    // checks
    {
        // common checks
        config_section_title(ctx, "common checks");

        ctx.if_("NOT CPPAN_DISABLE_CHECKS");

        // read vars file
        ctx.addLine("set(vars_dir \"" + normalize_path(directories.storage_dir_cfg) + "\")");
        ctx.addLine("set(vars_file \"${vars_dir}/${config}.cmake\")");
        // helper will show match between config with gen and just config
        ctx.addLine("set(vars_file_helper \"${vars_dir}//${config}.${config_dir}.cmake\")");
        if (!d.flags[pfLocalProject])
            ctx.addLine("read_check_variables_file(${vars_file})");
        ctx.addLine();

        ctx.if_("NOT DEFINED WORDS_BIGENDIAN");
        ctx.addLine("test_big_endian(WORDS_BIGENDIAN)");
        ctx.addLine("add_check_variable(WORDS_BIGENDIAN)");
        ctx.endif();
        // aliases
        ctx.addLine("set_cache_var(BIG_ENDIAN ${WORDS_BIGENDIAN})");
        ctx.addLine("set_cache_var(BIGENDIAN ${WORDS_BIGENDIAN})");
        ctx.addLine("set_cache_var(HOST_BIG_ENDIAN ${WORDS_BIGENDIAN})");
        ctx.addLine();

        // parallel checks
        if (d.empty())
        {
            config_section_title(ctx, "parallel checks");

            // parallel cygwin process work really bad, so disable parallel checks for it
            ctx.if_("NOT CYGWIN");
            ctx.addLine("set(tmp_dir \"" + normalize_path(temp_directory_path() / "vars") + "\")");
            ctx.addLine("string(RANDOM LENGTH 8 vars_dir)");
            ctx.addLine("set(tmp_dir \"${tmp_dir}/${vars_dir}\")");
            ctx.addLine();
            ctx.addLine("set(checks_file \"" + normalize_path(cwd / settings.cppan_dir / cppan_checks_yml) + "\")");
            ctx.addLine();
            ctx.addLine("execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_BINARY_DIR}/CMakeFiles ${tmp_dir}/CMakeFiles/ RESULT_VARIABLE ret)");
            auto cmd = R"(COMMAND ${CPPAN_COMMAND}
                            internal-parallel-vars-check
                                \"${CMAKE_COMMAND}\"
                                \"${tmp_dir}\"
                                \"${vars_file}\"
                                \"${checks_file}\"
                                \"${CMAKE_GENERATOR}\"
                                \"${CMAKE_SYSTEM_VERSION}\"
                                \"${CMAKE_GENERATOR_TOOLSET}\"
                                \"${CMAKE_TOOLCHAIN_FILE}\"
                            )"s;
            ctx.if_("CPPAN_COMMAND");
            cmake_debug_message(cmd);
            ctx.addLine("execute_process(" + cmd + " RESULT_VARIABLE ret)");
            ctx.addLine("check_result_variable(${ret} \"" + cmd + "\")");
            ctx.endif();
            // this file is created by parallel checks dispatcher
            ctx.addLine("read_check_variables_file(${tmp_dir}/" + parallel_checks_file + ")");
            ctx.addLine("set(CPPAN_NEW_VARIABLE_ADDED 1)");
            ctx.addLine();
            ctx.addLine("file(REMOVE_RECURSE ${tmp_dir})");
            ctx.endif();
            ctx.addLine();
        }

        // checks
        config_section_title(ctx, "checks");
        p.checks.write_checks(ctx, p.checks_prefixes);

        // write vars file
        if (!d.flags[pfLocalProject])
        {
            ctx.if_("CPPAN_NEW_VARIABLE_ADDED");
            ctx.addLine("write_check_variables_file(${vars_file})");
            ctx.addLine("file(WRITE ${vars_file_helper} \"\")");
            ctx.endif();
        }

        ctx.endif();
        ctx.addLine();
    }

    // after all vars are set
    // duplicate this to fix configs if needed
    ctx.addLine("get_configuration(config)"); // not children
    ctx.addLine("get_configuration_with_generator(config_dir)");
    ctx.addLine("get_configuration_unhashed(config_name)");
    ctx.addLine("get_configuration_with_generator_unhashed(config_gen_name)");
    ctx.addLine("get_number_of_cores(N_CORES)");
    ctx.addLine();

    // fixups
    // put bug workarounds here
    //config_section_title(ctx, "fixups");
    ctx.emptyLines();

    // dummy (compiled?) target
    if (d.empty())
    {
        declare_dummy_target(ctx, cppan_dummy_build_target);
        set_target_properties(ctx, cppan_dummy_target(cppan_dummy_build_target), "PROJECT_LABEL", "build-dependencies");

        declare_dummy_target(ctx, cppan_dummy_copy_target);
        set_target_properties(ctx, cppan_dummy_target(cppan_dummy_copy_target), "PROJECT_LABEL", "copy-dependencies");

        ctx.addLine("add_dependencies(" + cppan_dummy_target(cppan_dummy_copy_target) + " " + cppan_dummy_target(cppan_dummy_build_target) + ")");
    }

    file_footer(ctx, d);

    write_if_older(fn, ctx.getText());
}

void CMakePrinter::parallel_vars_check(const ParallelCheckOptions &o) const
{
    static const String cppan_variable_result_filename = "result.cppan";

    LOG_DEBUG(logger, "-- Preparing parallel checker");

    const auto &us = Settings::get_user_settings();

    int N = std::thread::hardware_concurrency();
    if (us.var_check_jobs > 0)
        N = std::min<int>(N, us.var_check_jobs);

    if (N <= 1)
    {
        LOG_DEBUG(logger, "-- Sequential checks mode selected");
        return;
    }

    Checks checks;
    checks.load(o.checks_file);

    // read known vars
    if (fs::exists(o.vars_file))
    {
        std::set<String> known_vars;
        std::vector<String> lines;
        {
            ScopedShareableFileLock lock(o.vars_file);
            lines = read_lines(o.vars_file);
        }
        for (auto &l : lines)
        {
            std::vector<String> v;
            boost::split(v, l, boost::is_any_of(";"));
            if (v.size() == 3)
                known_vars.insert(v[1]);
        }
        checks.remove_known_vars(known_vars);
    }

    auto workers = checks.scatter(N);
    size_t n_checks = 0;
    for (auto &w : workers)
        n_checks += w.checks.size();

    // There are few checks only. Won't go in parallel mode.
    if (n_checks <= 8)
    {
        LOG_DEBUG(logger, "-- There are few checks (" << n_checks << ") only. Won't go in parallel mode.");
        return;
    }

    // disable boost logger as it seems broken here for some reason
#undef LOG_INFO
#define LOG_INFO(l, m) \
    std::cout << m << std::endl

    LOG_INFO(logger, "-- Performing " << n_checks << " checks using " << N << " thread(s)");
#ifndef _WIN32
    LOG_INFO(logger, "-- This process may take up to 5 minutes depending on your hardware");
#else
    LOG_INFO(logger, "-- This process may take up to 10-20 minutes depending on your hardware");
#endif
    //LOG_FLUSH();

    auto work = [&o](auto &w, int i)
    {
        if (w.checks.empty())
            return;

        auto d = o.dir / std::to_string(i);
        fs::create_directories(d);

        CMakeContext ctx;
        ctx.addLine(cmake_minimum_required);
        ctx.addLine("project(" + std::to_string(i) + " LANGUAGES C CXX)");
        ctx.addLine(cmake_includes);
        ctx.addLine("include(" + normalize_path(directories.get_static_files_dir() / cmake_functions_filename) + ")");
        w.write_parallel_checks_for_workers(ctx);
        write_file(d / cmake_config_filename, ctx.getText());

        // copy cached cmake dir
        copy_dir(o.dir / "CMakeFiles", d / "CMakeFiles");
        // since cmake 3.8
        write_file(d / "CMakeCache.txt", "CMAKE_PLATFORM_INFO_INITIALIZED:INTERNAL=1\n");

        // run cmake
        primitives::Command c;
        c.args.push_back(o.cmake_binary.string());
        c.args.push_back("-H" + normalize_path(d));
        c.args.push_back("-B" + normalize_path(d));
        c.args.push_back("-G");
        c.args.push_back(o.generator);
        if (!o.system_version.empty())
            c.args.push_back("-DCMAKE_SYSTEM_VERSION=" + o.system_version);
        if (!o.toolset.empty())
        {
            c.args.push_back("-T");
            c.args.push_back(o.toolset);
        }
        if (!o.toolchain.empty())
            c.args.push_back("-DCMAKE_TOOLCHAIN_FILE=" + o.toolchain);

        //
        auto print = [](const String &str, bool eof, String &out_line)
        {
            if (eof)
            {
                out_line += str;
                LOG_INFO(logger, out_line);
                return;
            }

            size_t p = 0;
            while (1)
            {
#ifdef _WIN32
                size_t p1 = str.find_first_of("\r\n", p);
#else
                size_t p1 = str.find_first_of("\n", p);
#endif
                if (p1 == str.npos)
                {
                    out_line += str.substr(p);
                    break;
                }
                out_line += str.substr(p, p1 - p);
                LOG_INFO(logger, out_line);
                out_line.clear();

                p = ++p1;
#ifdef _WIN32
                if (str[p - 1] == '\r' && str.size() > p && str[p] == '\n')
                    p++;
#endif
            }
        };
        String out, err;
        c.out.action = [&out, &print](const String &str, bool eof) { print(str, eof, out); };
        c.err.action = [&err, &print](const String &str, bool eof) { print(str, eof, err); };


#ifndef _WIN32
        // hide output for *nix as it very fast there
        /*if (N >= 4)
            ret = command::execute(args);
        else*/
#endif
            //ret = command::execute_and_capture(args, o);
        //ret = command::execute(args);
        std::error_code ec;
        c.execute(ec);

        // do not fail (throw), try to read already found variables
        // commited as it occurs always check cmake error or cmake normal exit has this value
        if ((c.exit_code && c.exit_code.value()) || !c.exit_code || ec)
        {
            w.valid = false;
            String s;
            s += "-- Thread #" + std::to_string(i) + ": error during evaluating variables";
            if (ec)
            {
                s += ": " + ec.message() + "\n";
                s += ": out =\n" + c.out.text + "\n";
                s += ": err =\n" + c.err.text + "\n";
            }
            LOG_ERROR(logger, s << "\ncppan: swallowing this error");
            return;
            //throw std::runtime_error(s);
            //throw_with_trace(std::runtime_error(s));
        }

        w.read_parallel_checks_for_workers(d);
    };

    Executor e(N);
    std::vector<Future<void>> fs;

    int i = 0;
    for (auto &w : workers)
        fs.push_back(e.push([&work, &w, n = i++]() { work(w, n); }));

    auto t = get_time<std::chrono::seconds>([&fs]
    {
        for (auto &f : fs)
            f.wait();
        for (auto &f : fs)
            f.get();
    });

    checks.checks.clear();
    for (auto &w : workers)
        if (w.valid)
            checks += w;

    checks.print_values();
    //LOG_FLUSH();

    CMakeContext ctx;
    checks.print_values(ctx);
    write_file(o.dir / parallel_checks_file, ctx.getText());

    LOG_INFO(logger, "-- This operation took " + std::to_string(t) + " seconds to complete");
}

bool CMakePrinter::must_update_contents(const path &fn) const
{
    if (access_table->updates_disabled())
        return false;

    if (d.flags[pfLocalProject])
        return true;

    return access_table->must_update_contents(fn);
}

void CMakePrinter::write_if_older(const path &fn, const String &s) const
{
    if (d.ppath.is_loc())
        return write_file_if_different(fn, s);
    access_table->write_if_older(fn, s);
}

void CMakePrinter::print_source_groups(CMakeContext &ctx) const
{
    // disabled as very very slow!
    return;

    // check own data
    if (sgs.empty())
    {
        // check db data
        auto &sdb = getServiceDatabaseReadOnly();
        sgs = sdb.getSourceGroups(d);
        if (sgs.empty())
        {
            if (d.flags[pfLocalProject])
            {
                const auto &p = rd[d].config->getDefaultProject();
                for (auto &f : p.files)
                {
                    auto r = fs::relative(f, p.root_directory);
                    auto s2 = boost::replace_all_copy(r.parent_path().string(), "\\", "\\\\");
                    boost::replace_all(s2, "/", "\\\\");
                    sgs[s2].insert(normalize_path(f));
                }
            }
            else
            {
                const auto dir = d.getDirSrc();
                for (auto &f : boost::make_iterator_range(fs::recursive_directory_iterator(dir), {}))
                {
                    if (!fs::is_directory(f))
                        continue;

                    auto s = fs::relative(f.path(), dir).string();
                    auto s2 = boost::replace_all_copy(s, "\\", "\\\\");
                    boost::replace_all(s2, "/", "\\\\");

                    for (auto &f2 : boost::make_iterator_range(fs::directory_iterator(f), {}))
                    {
                        if (!fs::is_regular_file(f2))
                            continue;
                        auto s3 = normalize_path(f2.path());
                        sgs[s2].insert(s3);
                    }
                }
            }
            // add empty sgs to prevent directory lookup on the next run
            if (sgs.empty())
                sgs["__cppan_empty"];
            getServiceDatabase().setSourceGroups(d, sgs);
        }
    }

    // print, there's always generated group
    config_section_title(ctx, "source groups");
    ctx.addLine("source_group(\"generated\" REGULAR_EXPRESSION \"" + normalize_path(d.getDirObj()) + "/*\")");
    for (auto &sg : sgs)
    {
        ctx.increaseIndent("source_group(\"" + sg.first + "\" FILES");
        for (auto &f : sg.second)
            ctx.addLine("\"" + f + "\"");
        ctx.decreaseIndent(")");
    }
    ctx.emptyLines();
}
