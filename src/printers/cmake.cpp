/*
 * Copyright (c) 2016, Egor Pugin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *     3. Neither the name of the copyright holder nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "cmake.h"

#include "../access_table.h"
#include "../command.h"
#include "../date_time.h"
#include "../database.h"
#include "../directories.h"
#include "../executor.h"
#include "../lock.h"
#include "../inserts.h"
#include "../log.h"
#include "../resolver.h"

#ifdef _WIN32
#include "shell_link.h"
#endif

#include <boost/algorithm/string.hpp>

#include "../logger.h"
DECLARE_STATIC_LOGGER(logger, "cmake");

String repeat(const String &e, int n);

// common?
const String exports_dir_name = "exports";
const String exports_dir = "${CMAKE_BINARY_DIR}/" + exports_dir_name + "/";
const String packages_folder = "cppan/packages";

//
const String cmake_config_filename = "CMakeLists.txt";
const String actions_filename = "actions.cmake";
const String cppan_build_dir = "build";
const String non_local_build_file = "build.cmake";
const String exports_filename = "exports.cmake";
const String cmake_functions_filename = "functions.cmake";
const String cmake_header_filename = "header.cmake";
const String cmake_export_filename = "export.cmake";
const String cmake_object_config_filename = "generate.cmake";
const String cmake_helpers_filename = "helpers.cmake";
const String include_guard_filename = "include.cmake";
const String cppan_stamp_filename = "cppan_sources.stamp";
const String cppan_checks_yml = "checks.yml";
const String parallel_checks_file = "vars.txt";

const String cmake_minimum_required = "cmake_minimum_required(VERSION 3.2.0)";
const String cmake_debug_message_fun = "cppan_debug_message";
const String cppan_dummy_build_target = "build";
const String cppan_dummy_copy_target = "copy";

const String config_delimeter_short = repeat("#", 40);
const String config_delimeter = config_delimeter_short + config_delimeter_short;

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
include(TestBigEndian)
)";

String cmake_debug_message(const String &s);

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

void config_section_title(Context &ctx, const String &t)
{
    ctx.emptyLines(1);
    ctx.addLine(config_delimeter);
    ctx.addLine("#");
    ctx.addLine("# " + t);
    ctx.addLine("#");
    ctx.addLine(config_delimeter);
    ctx.addLine();
    ctx.addLine(cmake_debug_message("Section: " + t));
    ctx.emptyLines(1);
}

void file_header(Context &ctx, const Package &d)
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
        ctx.addLine("# package hash short: " + d.getFilesystemHash());
        ctx.addLine("#");
    }
    else
    {
        ctx.addLine("#");
        ctx.addLine("# cppan");
        ctx.addLine("#");
    }
    ctx.addLine();
    ctx.addLine("include(" + normalize_path(directories.get_static_files_dir() / cmake_header_filename) + ")");
    ctx.addLine();
    if (!d.empty())
    {
        ctx.addLine(cmake_debug_message("Entering file: ${CMAKE_CURRENT_LIST_FILE}"));
        ctx.addLine(cmake_debug_message("Package      : " + d.target_name));
    }
    else
        ctx.addLine(cmake_debug_message("Entering file: ${CMAKE_CURRENT_LIST_FILE}"));
    ctx.addLine();
}

void file_footer(Context &ctx, const Package &d)
{
    ctx.emptyLines(1);
    ctx.addLine(cmake_debug_message("Leaving file: ${CMAKE_CURRENT_LIST_FILE}"));
    ctx.addLine();
    ctx.addLine(config_delimeter);
    ctx.addLine();
    ctx.splitLines();
}

void print_storage_dirs(Context &ctx)
{
    config_section_title(ctx, "storage dirs");
    ctx.addLine("set(STORAGE_DIR \"" + normalize_path(directories.storage_dir) + "\")");
    ctx.addLine("set(STORAGE_DIR_ETC \"" + normalize_path(directories.storage_dir_etc) + "\")");
    ctx.addLine("set(STORAGE_DIR_ETC_STATIC \"" + normalize_path(directories.get_static_files_dir()) + "\")");
    ctx.addLine("set(STORAGE_DIR_USR \"" + normalize_path(directories.storage_dir_usr) + "\")");
    ctx.addLine();
}

void print_local_project_files(Context &ctx, const Project &p)
{
    ctx.addLine("set(src");
    ctx.increaseIndent();
    for (auto &f : FilesSorted(p.files.begin(), p.files.end()))
        ctx.addLine("\"${SDIR}/" + normalize_path(f) + "\"");
    ctx.decreaseIndent();
    ctx.addLine(")");
}

String cppan_dummy_target(const String &name)
{
    if (name.empty())
        return "cppan-dummy";
    return "cppan-dummy-" + name;
}

void declare_dummy_target(Context &ctx, const String &name)
{
    config_section_title(ctx, "dummy compiled target " + name);
    ctx.addLine("# this target will be always built before any other");
    ctx.addLine("if(MSVC)");
    ctx.addLine("    add_custom_target(" + cppan_dummy_target(name) + " ALL DEPENDS cppan_intentionally_missing_file.txt)");
    ctx.addLine("else()");
    ctx.addLine("    add_custom_target(" + cppan_dummy_target(name) + " ALL)");
    ctx.addLine("endif()");
    ctx.addLine();
    ctx.addLine("set_target_properties(" + cppan_dummy_target(name) + " PROPERTIES\n    FOLDER \"cppan/service\"\n)");
    ctx.emptyLines(1);
}

String cmake_debug_message(const String &s)
{
    return cmake_debug_message_fun + "(\"" + s + "\")";
}

String add_subdirectory(String src)
{
    boost::algorithm::replace_all(src, "\\", "/");
    return "include(\"" + src + "/" + include_guard_filename + "\")";
}

void add_subdirectory(Context &ctx, const String &src)
{
    ctx.addLine(add_subdirectory(src));
}

String get_binary_path(const Package &d, const String &prefix)
{
    return prefix + "/cppan/" + d.getFilesystemHash();
}

String get_binary_path(const Package &d)
{
    return get_binary_path(d, "${CMAKE_BINARY_DIR}");
}

void print_dependencies(Context &ctx, const Packages &dd, bool use_cache)
{
    std::vector<String> includes;
    Context ctx2;

    if (dd.empty())
        return;

    bool obj_dir = true;
    if (!use_cache)
        obj_dir = false;

    auto base_dir = directories.storage_dir_src;
    if (obj_dir)
        base_dir = directories.storage_dir_obj;

    config_section_title(ctx, "direct dependencies");
    for (auto &p : dd)
    {
        String s;
        auto dir = base_dir;
        // do not "optimize" this condition (whole if..else)
        if (p.second.flags[pfHeaderOnly] || p.second.flags[pfIncludeDirectoriesOnly])
        {
            dir = directories.storage_dir_src;
            s = p.second.getDirSrc().string();
        }
        else if (obj_dir)
        {
            s = p.second.getDirObj().string();
        }
        else
        {
            dir = directories.storage_dir_src;
            s = p.second.getDirSrc().string();
        }

        if (p.second.flags[pfIncludeDirectoriesOnly])
        {
            // MUST be here!
            // actions are executed from include_directories only projects
            ctx.addLine("# " + p.second.target_name);
            ctx.addLine("include(\"" + normalize_path(s) + "/" + actions_filename + "\")");
        }
        else if (!use_cache || p.second.flags[pfHeaderOnly])
        {
            ctx.addLine("# " + p.second.target_name);
            add_subdirectory(ctx, s);
        }
        else if (p.second.flags[pfLocalProject])
        {
            ctx.addLine("if (NOT TARGET " + p.second.target_name + ")");
            ctx.addLine("add_subdirectory(\"" + normalize_path(s) + "\" \"" + normalize_path(p.second.getDirObj() / "build/${config_dir}") + "\")");
            ctx.addLine("endif()");
        }
        else
        {
            // add local build includes
            ctx2.addLine("# " + p.second.target_name);
            add_subdirectory(ctx2, p.second.getDirSrc().string());

            includes.push_back("# " + p.second.target_name + "\n" +
                "include(\"" + normalize_path(s) + "/" + cmake_object_config_filename + "\")");
        }
    }
    ctx.addLine();

    if (!includes.empty())
    {
        config_section_title(ctx, "include dependencies (they should be placed at the end)");
        ctx.addLine("if (CPPAN_USE_CACHE)");
        ctx.increaseIndent();
        for (auto &line : includes)
            ctx.addLine(line);

        ctx.decreaseIndent();
        ctx.addLine("else()");
        ctx.increaseIndent();
        ctx.addLine(boost::trim_copy(ctx2.getText()));
        ctx.decreaseIndent();
        ctx.addLine("endif()");
    }

    ctx.splitLines();
}

void print_source_groups(Context &ctx, const path &dir)
{
    bool once = false;
    for (auto &f : boost::make_iterator_range(fs::recursive_directory_iterator(dir), {}))
    {
        if (!fs::is_directory(f))
            continue;

        if (!once)
            config_section_title(ctx, "source groups");
        once = true;

        auto s = fs::relative(f.path(), dir).string();
        auto s2 = boost::replace_all_copy(s, "\\", "\\\\");
        boost::replace_all(s2, "/", "\\\\");

        ctx.addLine("source_group(\"" + s2 + "\" FILES");
        ctx.increaseIndent();
        for (auto &f2 : boost::make_iterator_range(fs::directory_iterator(f), {}))
        {
            if (!fs::is_regular_file(f2))
                continue;
            ctx.addLine("\"" + normalize_path(f2.path()) + "\"");
        }
        ctx.decreaseIndent();
        ctx.addLine(")");
    }
    ctx.emptyLines(1);
}

void gather_build_deps(Context &ctx, const Packages &dd, Packages &out, bool recursive = false)
{
    for (auto &dp : dd)
    {
        auto &d = dp.second;
        if (d.flags[pfHeaderOnly] || d.flags[pfIncludeDirectoriesOnly])
            continue;
        auto i = out.insert(dp);
        if (i.second && recursive)
            gather_build_deps(ctx, rd[d].dependencies, out, recursive);
    }
}

void gather_copy_deps(Context &ctx, const Packages &dd, Packages &out)
{
    for (auto &dp : dd)
    {
        auto &d = dp.second;
        if (d.flags[pfHeaderOnly] || d.flags[pfIncludeDirectoriesOnly])
            continue;
        if (d.flags[pfExecutable] && !d.flags[pfLocalProject])
            continue;
        if (d.flags[pfExecutable] && d.flags[pfLocalProject] && !d.flags[pfDirectDependency])
            continue; // but maybe copy its deps?
        auto i = out.insert(dp);
        if (i.second)
            gather_copy_deps(ctx, rd[d].dependencies, out);
    }
}

void print_build_dependencies(Context &ctx, const Package &d, const String &target)
{
    // direct deps' build actions for non local build
    {
        config_section_title(ctx, "build dependencies");

        // build deps
        ctx.addLine("if (CPPAN_USE_CACHE)");
        ctx.increaseIndent();

        // run building of direct dependecies before project building
        {
            Packages build_deps;
            // at the moment we re-check all deps to see if we need to build them
            gather_build_deps(ctx, rd[d].dependencies, build_deps, true);

            if (!build_deps.empty())
            {
                Context local;
                local.addLine("get_configuration_with_generator(config)");
                local.addLine("get_configuration_exe(config_exe)");

                local.addLine("add_custom_command(TARGET " + target + " PRE_BUILD");
                local.increaseIndent();
                bool has_build_deps = false;
                for (auto &dp : build_deps)
                {
                    auto &p = dp.second;

                    // local projects are always built inside solution
                    if (p.flags[pfLocalProject])
                        continue;

                    has_build_deps = true;
                    local.addLine("COMMAND ${CMAKE_COMMAND}");
                    local.increaseIndent();
                    local.addLine("-DTARGET_FILE=$<TARGET_FILE:" + p.target_name + ">");
                    local.addLine("-DCONFIG=$<CONFIG>");
                    String cfg = "config";
                    if (p.flags[pfExecutable] && !p.flags[pfLocalProject])
                        cfg = "config_exe";
                    local.addLine("-DBUILD_DIR=" + normalize_path(p.getDirObj()) + "/build/${" + cfg + "}");
                    local.addLine("-DEXECUTABLE=" + String(p.flags[pfExecutable] ? "1" : "0"));
                    local.addLine("-DCPPAN_BUILD_EXECUTABLES_WITH_SAME_CONFIG=${CPPAN_BUILD_EXECUTABLES_WITH_SAME_CONFIG}");
                    local.addLine("-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}");
                    local.addLine("-DN_CORES=${N_CORES}");
                    if (d.empty())
                        local.addLine("-DMULTICORE=1");
                    local.addLine("-DXCODE=${XCODE}");
                    local.addLine("-P " + normalize_path(p.getDirObj()) + "/" + non_local_build_file);
                    local.decreaseIndent();
                    local.addLine();
                }
                local.decreaseIndent();
                local.addLine(")");
                local.addLine();

                if (has_build_deps)
                    ctx += local;
            }
        }

        ctx.decreaseIndent();
        ctx.addLine("endif()");
        ctx.addLine();
    }
}

void CMakePrinter::prepare_rebuild()
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

void CMakePrinter::prepare_build2()
{
    auto &bs = rc->settings;
    auto &p = rc->getDefaultProject();

    Context ctx;
    file_header(ctx, d);

    config_section_title(ctx, "cmake settings");
    ctx.addLine(cmake_minimum_required);
    ctx.addLine();

    config_section_title(ctx, "project settings");
    ctx.addLine("project(" + bs.filename_without_ext + " LANGUAGES C CXX)");
    ctx.addLine();

    config_section_title(ctx, "compiler & linker settings");
    ctx.addLine(R"(# Output directory settings
set(output_dir ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${output_dir})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${output_dir})
#set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${output_dir})

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if (MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
endif()
)");

    // compiler flags
    ctx.addLine("set(CMAKE_C_FLAGS \"${CMAKE_C_FLAGS} " + bs.c_compiler_flags + "\")");
    ctx.addLine("set(CMAKE_CXX_FLAGS \"${CMAKE_CXX_FLAGS} " + bs.cxx_compiler_flags + "\")");
    ctx.addLine();

    for (int i = 0; i < Settings::CMakeConfigurationType::Max; i++)
    {
        auto &cfg = configuration_types[i];
        ctx.addLine("set(CMAKE_C_FLAGS_" + cfg + " \"${CMAKE_C_FLAGS_" + cfg + "} " + bs.c_compiler_flags_conf[i] + "\")");
        ctx.addLine("set(CMAKE_CXX_FLAGS_" + cfg + " \"${CMAKE_CXX_FLAGS_" + cfg + "} " + bs.cxx_compiler_flags_conf[i] + "\")");
        ctx.addLine();
    }

    // linker flags
    ctx.addLine("set(CMAKE_EXE_LINKER_FLAGS \"${CMAKE_EXE_LINKER_FLAGS} " + bs.link_flags + "\")");
    ctx.addLine("set(CMAKE_MODULE_LINKER_FLAGS \"${CMAKE_MODULE_LINKER_FLAGS} " + bs.link_flags + "\")");
    ctx.addLine("set(CMAKE_SHARED_LINKER_FLAGS \"${CMAKE_SHARED_LINKER_FLAGS} " + bs.link_flags + "\")");
    ctx.addLine("set(CMAKE_STATIC_LINKER_FLAGS \"${CMAKE_STATIC_LINKER_FLAGS} " + bs.link_flags + "\")");
    ctx.addLine();

    for (int i = 0; i < Settings::CMakeConfigurationType::Max; i++)
    {
        auto &cfg = configuration_types[i];
        ctx.addLine("set(CMAKE_EXE_LINKER_FLAGS_" + cfg + " \"${CMAKE_EXE_LINKER_FLAGS_" + cfg + "} " + bs.link_flags_conf[i] + "\")");
        ctx.addLine("set(CMAKE_MODULE_LINKER_FLAGS_" + cfg + " \"${CMAKE_MODULE_LINKER_FLAGS_" + cfg + "} " + bs.link_flags_conf[i] + "\")");
        ctx.addLine("set(CMAKE_SHARED_LINKER_FLAGS_" + cfg + " \"${CMAKE_SHARED_LINKER_FLAGS_" + cfg + "} " + bs.link_flags_conf[i] + "\")");
        ctx.addLine("set(CMAKE_STATIC_LINKER_FLAGS_" + cfg + " \"${CMAKE_STATIC_LINKER_FLAGS_" + cfg + "} " + bs.link_flags_conf[i] + "\")");
        ctx.addLine();
    }

    // should be after flags
    config_section_title(ctx, "CPPAN include");
    ctx.addLine("set(CPPAN_BUILD_OUTPUT_DIR \"" + normalize_path(fs::current_path()) + "\")");
    ctx.addLine(String("set(CPPAN_BUILD_SHARED_LIBS ") + (bs.use_shared_libs ? "1" : "0") + ")");
    ctx.addLine("add_subdirectory(" + normalize_path(bs.cppan_dir) + ")");
    ctx.addLine();

    file_footer(ctx, d);

    write_file_if_different(bs.source_directory / cmake_config_filename, ctx.getText());
}

int CMakePrinter::generate() const
{
    auto &bs = rc->settings;

    command::Args args;
    args.push_back("cmake");
    args.push_back("-H" + normalize_path(bs.source_directory));
    args.push_back("-B" + normalize_path(bs.binary_directory));
    if (!bs.c_compiler.empty())
        args.push_back("-DCMAKE_C_COMPILER=" + bs.c_compiler);
    if (!bs.cxx_compiler.empty())
        args.push_back("-DCMAKE_CXX_COMPILER=" + bs.cxx_compiler);
    if (!bs.generator.empty())
    {
        args.push_back("-G");
        args.push_back(bs.generator);
    }
    if (!bs.toolset.empty())
    {
        args.push_back("-T");
        args.push_back(bs.toolset);
    }
    args.push_back("-DCMAKE_BUILD_TYPE=" + bs.configuration);
    args.push_back("-DCPPAN_COMMAND=" + normalize_path(get_program()));
    args.push_back("-DCPPAN_CMAKE_VERBOSE=" + String(bs.cmake_verbose ? "1" : "0"));
    for (auto &o : bs.cmake_options)
        args.push_back(o);
    for (auto &o : bs.env)
    {
#ifdef _WIN32
        _putenv_s(o.first.c_str(), o.second.c_str());
#else
        setenv(o.first.c_str(), o.second.c_str(), 1);
#endif
    }

    auto ret = command::execute_with_output(args);

    if (bs.allow_links)
    {
        if (!bs.silent || bs.is_custom_build_dir())
        {
            auto bld_dir = fs::current_path();
#ifdef _WIN32
            auto name = bs.filename_without_ext + "-" + bs.config + ".sln.lnk";
            if (bs.is_custom_build_dir())
            {
                bld_dir = bs.binary_directory / ".." / "..";
                name = bs.config + ".sln.lnk";
            }
            auto sln = bs.binary_directory / (bs.filename_without_ext + ".sln");
            auto sln_new = bld_dir / name;
            if (fs::exists(sln))
                CreateLink(sln.string().c_str(), sln_new.string().c_str(), "Link to CPPAN Solution");
#else
            if (bs.generator == "Xcode")
            {
                auto name = bs.filename_without_ext + "-" + bs.config + ".xcodeproj";
                if (bs.is_custom_build_dir())
                {
                    bld_dir = bs.binary_directory / ".." / "..";
                    name = bs.config + ".xcodeproj";
                }
                auto sln = bs.binary_directory / (bs.filename_without_ext + ".xcodeproj");
                auto sln_new = bld_dir / name;
                boost::system::error_code ec;
                fs::create_symlink(sln, sln_new, ec);
            }
            else if (!bs.is_custom_build_dir())
            {
                bld_dir /= path(CPPAN_LOCAL_BUILD_PREFIX + bs.filename) / bs.config;
                fs::create_directories(bld_dir);
                boost::system::error_code ec;
                fs::create_symlink(bs.source_directory / cmake_config_filename, bld_dir / cmake_config_filename, ec);
            }
#endif
        }
    }

    return ret.rc;
}

int CMakePrinter::build() const
{
    auto &bs = rc->settings;

    command::Args args;
    args.push_back("cmake");
    args.push_back("--build");
    args.push_back(normalize_path(bs.binary_directory));
    args.push_back("--config");
    args.push_back(bs.configuration);
    return command::execute_with_output(args).rc;
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

void CMakePrinter::print()
{
    print_configs();
}

void CMakePrinter::print_meta()
{
    print_meta_config_file(cwd / cc->settings.cppan_dir / cmake_config_filename);
    print_helper_file(cwd / cc->settings.cppan_dir / cmake_helpers_filename);

    // print inserted files (they'll be printed only once)
    access_table->write_if_older(directories.get_static_files_dir() / cmake_functions_filename, cmake_functions);
    access_table->write_if_older(directories.get_static_files_dir() / cmake_header_filename, cmake_header);
    access_table->write_if_older(directories.get_static_files_dir() / cmake_export_filename, cmake_export_import_file);
    access_table->write_if_older(directories.get_static_files_dir() / "branch.rc.in", branch_rc_in);
    access_table->write_if_older(directories.get_static_files_dir() / "version.rc.in", version_rc_in);
    access_table->write_if_older(directories.get_include_dir() / CPP_HEADER_FILENAME, cppan_h);

    if (d.empty())
    {
        // we write some static files to root project anyway
        access_table->write_if_older(cwd / cc->settings.cppan_dir / CPP_HEADER_FILENAME, cppan_h);

        // checks file
        access_table->write_if_older(cwd / cc->settings.cppan_dir / cppan_checks_yml, cc->checks.save());
    }
}

void CMakePrinter::print_configs()
{
    auto src_dir = d.getDirSrc();
    fs::create_directories(src_dir);

    print_package_config_file(src_dir / cmake_config_filename);
    print_package_actions_file(src_dir / actions_filename);
    print_package_include_file(src_dir / include_guard_filename);

    if (d.flags[pfHeaderOnly])
        return;

    auto obj_dir = d.getDirObj();
    fs::create_directories(obj_dir);

    // print object config files for non-local building
    print_object_config_file(obj_dir / cmake_config_filename);
    print_object_include_config_file(obj_dir / cmake_object_config_filename);
    print_object_export_file(obj_dir / exports_filename);
    print_object_build_file(obj_dir / non_local_build_file);
}

void CMakePrinter::print_bs_insertion(Context &ctx, const Project &p, const String &name, const String BuildSystemConfigInsertions::*i) const
{
    config_section_title(ctx, name);
    if (cc->getProjects().size() > 1)
    {
        ctx.addLine(cc->bs_insertions.*i);
        ctx.emptyLines(1);
    }
    ctx.addLine(p.bs_insertions.*i);
    ctx.emptyLines(1);

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
            ctx.addLine("if (LIBRARY_TYPE STREQUAL \"" + boost::algorithm::to_upper_copy(ol.first) + "\")");
            ctx.increaseIndent();
            ctx.addLine(s);
            ctx.decreaseIndent();
            ctx.addLine("endif()");
            ctx.emptyLines(1);
        }
    }

    ctx.emptyLines(1);
}

void CMakePrinter::print_package_config_file(const path &fn) const
{
    if (!must_update_contents(fn))
        return;

    bool header_only = d.flags[pfHeaderOnly];
    const auto &p = cc->getDefaultProject();

    const String cppan_api = CPPAN_EXPORT_PREFIX + d.variable_name;

    Context ctx;
    file_header(ctx, d);

    // local aliases
    ctx.addLine("set(target " + d.target_name + ")");
    ctx.addLine("set(this " + d.target_name + ")");
    ctx.addLine();

    // prevent errors
    ctx.addLine("if (TARGET ${this})");
    ctx.addLine("    return()");
    ctx.addLine("endif()");

    // deps
    print_dependencies(ctx, rd[d].dependencies, rc->settings.use_cache);

    // settings
    {
        config_section_title(ctx, "settings");
        print_storage_dirs(ctx);
        ctx.addLine("set(PACKAGE ${this})");
        ctx.addLine("set(PACKAGE_NAME " + d.ppath.toString() + ")");
        ctx.addLine("set(PACKAGE_VERSION " + d.version.toString() + ")");
        ctx.addLine();
        if (d.version.isBranch())
        {
            ctx.addLine("set(PACKAGE_VERSION_NUM  \"0\")");
            ctx.addLine("set(PACKAGE_VERSION_NUM2 \"0LL\")");
        }
        else
        {
            auto ver2hex = [this](int n)
            {
                std::ostringstream ss;
                ss << std::hex << std::setfill('0') << std::setw(n) << d.version.major;
                ss << std::hex << std::setfill('0') << std::setw(n) << d.version.minor;
                ss << std::hex << std::setfill('0') << std::setw(n) << d.version.patch;
                return ss.str();
            };
            ctx.addLine("set(PACKAGE_VERSION_NUM  \"0x" + ver2hex(2) + "\")");
            ctx.addLine("set(PACKAGE_VERSION_NUM2 \"0x" + ver2hex(4) + "LL\")");
        }
        ctx.addLine();
        ctx.addLine("set(PACKAGE_VERSION_MAJOR " + std::to_string(d.version.major) + ")");
        ctx.addLine("set(PACKAGE_VERSION_MINOR " + std::to_string(d.version.minor) + ")");
        ctx.addLine("set(PACKAGE_VERSION_PATCH " + std::to_string(d.version.patch) + ")");
        ctx.addLine();
        ctx.addLine("set(PACKAGE_IS_BRANCH " + String(d.version.isBranch() ? "1" : "0") + ")");
        ctx.addLine("set(PACKAGE_IS_VERSION " + String(d.version.isVersion() ? "1" : "0") + ")");
        ctx.addLine();
        ctx.addLine("set(LIBRARY_TYPE STATIC)");
        ctx.addLine();
        ctx.addLine("if (CPPAN_BUILD_SHARED_LIBS)");
        ctx.increaseIndent();
        ctx.addLine("set(LIBRARY_TYPE SHARED)");
        ctx.decreaseIndent();
        ctx.addLine("endif()");
        ctx.addLine();
        ctx.addLine("if (LIBRARY_TYPE_" + d.variable_name + ")");
        ctx.increaseIndent();
        ctx.addLine("set(LIBRARY_TYPE ${LIBRARY_TYPE_" + d.variable_name + "})");
        ctx.decreaseIndent();
        ctx.addLine("endif()");
        ctx.addLine();

        if (p.static_only)
            ctx.addLine("set(LIBRARY_TYPE STATIC)");
        else if (p.shared_only)
            ctx.addLine("set(LIBRARY_TYPE SHARED)");
        else if (d.flags[pfHeaderOnly])
            ctx.addLine("set(LIBRARY_TYPE INTERFACE)");
        ctx.emptyLines(1);
        ctx.addLine("set(EXECUTABLE " + String(d.flags[pfExecutable] ? "1" : "0") + ")");
        ctx.addLine();

        if (d.flags[pfLocalProject])
            ctx.addLine("set(SDIR \"" + normalize_path(p.root_directory) + "\")");
        else
            ctx.addLine("set(SDIR ${CMAKE_CURRENT_SOURCE_DIR})");
        ctx.addLine("set(BDIR ${CMAKE_CURRENT_BINARY_DIR})");
        ctx.addLine();
        ctx.addLine("set(LIBRARY_API " + cppan_api + ")");
        ctx.addLine();

        // configs
        ctx.addLine("get_configuration_variables()");
        ctx.addLine();

        // copy exe cmake settings
        ctx.addLine("if (EXECUTABLE AND CPPAN_USE_CACHE)");
        ctx.increaseIndent();
        ctx.addLine("set(to \"" + normalize_path(directories.storage_dir_cfg) + "/${config}/CMakeFiles/${CMAKE_VERSION}\")");
        ctx.addLine("if (NOT EXISTS ${to})");
        ctx.increaseIndent();
        ctx.addLine("execute_process(");
        ctx.addLine("    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_BINARY_DIR}/CMakeFiles/${CMAKE_VERSION} ${to}");
        ctx.addLine("    RESULT_VARIABLE ret");
        ctx.addLine(")");
        ctx.decreaseIndent();
        ctx.addLine("endif()");
        ctx.decreaseIndent();
        ctx.addLine("endif()");
        ctx.addLine();

        ctx.emptyLines(1);
    }

    config_section_title(ctx, "export/import");
    ctx.addLine("include(\"" + normalize_path(directories.get_static_files_dir() / cmake_export_filename) + "\")");

    print_bs_insertion(ctx, p, "pre sources", &BuildSystemConfigInsertions::pre_sources);

    // sources
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
        ctx.addLine("set(src");
        ctx.increaseIndent();
        for (auto &f : p.build_files)
        {
            auto s = f;
            std::replace(s.begin(), s.end(), '\\', '/');
            ctx.addLine("${SDIR}/" + s);
        }
        ctx.decreaseIndent();
        ctx.addLine(")");
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
                ctx.addLine("remove_src    (\"${SDIR}/" + s + "\")");
                ctx.addLine("remove_src_dir(\"${SDIR}/" + s + "\")");
                ctx.addLine();
            }
            ctx.emptyLines(1);
        }
    };
    exclude_files(p.exclude_from_build);

    print_bs_insertion(ctx, p, "post sources", &BuildSystemConfigInsertions::post_sources);

    for (auto &ol : p.options)
        for (auto &ll : ol.second.link_directories)
            ctx.addLine("link_directories(" + ll + ")");
    ctx.emptyLines(1);

    // do this right before target
    if (!d.empty())
        ctx.addLine("add_win32_version_info(\"" + normalize_path(d.getDirObj()) + "\")");

    // target
    config_section_title(ctx, "target: " + d.target_name);
    if (d.flags[pfExecutable])
    {
        ctx.addLine("add_executable                (${this} " +
            String(p.executable_type == ExecutableType::Win32 ? "WIN32" : "") + " ${src})");
    }
    else
    {
        if (header_only)
            ctx.addLine("add_library                   (${this} INTERFACE)");
        else
            ctx.addLine("add_library                   (${this} ${LIBRARY_TYPE} ${src})");
    }
    ctx.addLine();

    // properties
    {
        // standards
        // always off extensions by default
        // if you need gnuXX extensions, set compiler flags in options or post target
        // TODO: propagate standards to dependent packages
        // (i.e. dependent packages should be built >= this standard value)
        if (!header_only)
        {
            if (p.c_standard != 0)
            {
                ctx.addLine("set_property(TARGET ${this} PROPERTY C_EXTENSIONS OFF)");
                ctx.addLine("set_property(TARGET ${this} PROPERTY C_STANDARD " + std::to_string(p.c_standard) + ")");
            }
            if (p.cxx_standard != 0)
            {
                ctx.addLine("set_property(TARGET ${this} PROPERTY CXX_EXTENSIONS OFF)");
                switch (p.cxx_standard)
                {
                case 17:
                    ctx.addLine("if (UNIX)");
                    ctx.addLine("set(CMAKE_CXX_FLAGS \"${CMAKE_CXX_FLAGS} -std=c++1z\")");
                    ctx.addLine("endif()");
                    break;
                case 20:
                    ctx.addLine("if (UNIX)");
                    ctx.addLine("set(CMAKE_CXX_FLAGS \"${CMAKE_CXX_FLAGS} -std=c++2x\")");
                    ctx.addLine("endif()");
                    break;
                default:
                    ctx.addLine("set_property(TARGET ${this} PROPERTY CXX_STANDARD " + std::to_string(p.cxx_standard) + ")");
                    break;
                }
                if (p.cxx_standard > 14)
                {
                    ctx.addLine("if (MSVC)");
                    ctx.addLine("set(CMAKE_CXX_FLAGS \"${CMAKE_CXX_FLAGS} /std:c++latest\")");
                    ctx.addLine("endif()");
                }
            }
        }

        if (p.export_all_symbols)
        {
            ctx.addLine("set_target_properties(${this} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS True)");
        }
        ctx.addLine();
    }

    // include directories
    {
        std::vector<Package> include_deps;
        for (auto &dep : rd[d].dependencies)
        {
            if (dep.second.flags[pfIncludeDirectoriesOnly])
                include_deps.push_back(dep.second);
        }
        if (!p.include_directories.empty() || !include_deps.empty())
        {
            auto get_i_dir = [this](const String &i)
            {
                if (i.find("${") != i.npos)
                    return i;
                return "${SDIR}/" + i;
            };

            ctx.addLine("target_include_directories    (${this}");
            ctx.increaseIndent();
            if (header_only)
            {
                for (auto &idir : p.include_directories.public_)
                    ctx.addLine("INTERFACE " + get_i_dir(idir.string()));
                for (auto &pkg : include_deps)
                {
                    auto &proj = rd[pkg].config->getDefaultProject();
                    for (auto &i : proj.include_directories.public_)
                    {
                        auto ipath = pkg.getDirSrc() / i;
                        boost::system::error_code ec;
                        if (fs::exists(ipath, ec))
                            ctx.addLine("INTERFACE " + normalize_path(ipath));
                    }
                    // no privates here
                }
            }
            else
            {
                for (auto &idir : p.include_directories.public_)
                    // executable can export include dirs too (e.g. flex - FlexLexer.h)
                    // TODO: but check it ^
                    // export only exe's idirs, not deps' idirs
                    // that's why target_link_libraries always private for exe
                    ctx.addLine("PUBLIC " + get_i_dir(idir.string()));
                for (auto &idir : p.include_directories.private_)
                    ctx.addLine("PRIVATE " + get_i_dir(idir.string()));
                for (auto &pkg : include_deps)
                {
                    auto &proj = rd[pkg].config->getDefaultProject();
                    for (auto &i : proj.include_directories.public_)
                    {
                        auto ipath = pkg.getDirSrc() / i;
                        boost::system::error_code ec;
                        if (fs::exists(ipath, ec))
                            // if 'd' is an executable, do not export foreign idirs (keep them private)
                            ctx.addLine((d.flags[pfExecutable] ? "PRIVATE " : "PUBLIC ") + normalize_path(ipath));
                    }
                    // no privates here
                }
            }
            ctx.decreaseIndent();
            ctx.addLine(")");

            // add BDIRs
            for (auto &pkg : include_deps)
            {
                if (pkg.flags[pfHeaderOnly])
                    continue;

                ctx.addLine("# Binary dir of include_directories_only dependency");
                ctx.addLine("if (CPPAN_USE_CACHE)");

                {
                    auto bdir = pkg.getDirObj() / cppan_build_dir / (pkg.flags[pfExecutable] ? "${config_exe}" : "${config_lib_gen}");
                    auto p = normalize_path(get_binary_path(pkg, bdir.string()));
                    ctx.addLine("if (EXISTS \"" + p + "\")");
                    ctx.addLine("target_include_directories    (${this}");
                    ctx.increaseIndent();
                    if (header_only)
                        ctx.addLine("INTERFACE " + p);
                    else
                        ctx.addLine((d.flags[pfExecutable] ? "PRIVATE " : "PUBLIC ") + p);
                    ctx.decreaseIndent();
                    ctx.addLine(")");
                    ctx.addLine("endif()");
                }

                ctx.addLine("else()");

                {
                    auto p = normalize_path(get_binary_path(pkg));
                    ctx.addLine("if (EXISTS \"" + p + "\")");
                    ctx.addLine("target_include_directories    (${this}");
                    ctx.increaseIndent();
                    if (header_only)
                        ctx.addLine("INTERFACE " + p);
                    else
                        ctx.addLine((d.flags[pfExecutable] ? "PRIVATE " : "PUBLIC ") + p);
                    ctx.decreaseIndent();
                    ctx.addLine(")");
                    ctx.addLine("endif()");
                }

                ctx.addLine("endif()");
                ctx.addLine("");
            }
        }
    }

    // deps (direct)
    {
        config_section_title(ctx, "dependencies");

        Strings deps;
        for (auto &dep : rd[d].dependencies)
        {
            if (dep.second.flags[pfExecutable] ||
                dep.second.flags[pfIncludeDirectoriesOnly])
                continue;

            ctx.addLine("if (NOT TARGET " + dep.second.target_name + ")");
            ctx.addLine("message(FATAL_ERROR \"Target '" + dep.second.target_name + "' is not visible at this place\")");
            ctx.addLine("endif()");
            ctx.addLine();

            if (header_only)
                deps.push_back("INTERFACE " + dep.second.target_name);
            else
            {
                if (dep.second.flags[pfPrivateDependency])
                    deps.push_back("PRIVATE " + dep.second.target_name);
                else
                    deps.push_back("PUBLIC " + dep.second.target_name);
            }
        }

        ctx.addLine("target_link_libraries         (${this}");
        ctx.increaseIndent();
        for (auto &dep : deps)
            ctx.addLine(dep);
        ctx.decreaseIndent();
        ctx.addLine(")");
        ctx.addLine();
    }

    // solution folder
    config_section_title(ctx, "options");
    if (!header_only && !d.flags[pfLocalProject])
    {
        ctx.addLine("set_target_properties         (${this} PROPERTIES");
        ctx.addLine("    FOLDER \"" + packages_folder + "/" + d.ppath.toString() + "/" + d.version.toString() + "\"");
        ctx.addLine(")");
        ctx.emptyLines(1);
    }

    // options (defs, compile options etc.)
    {
        if (!header_only)
        {
            // pkg
            ctx.addLine("target_compile_definitions    (${this}");
            ctx.increaseIndent();
            ctx.addLine("PRIVATE   PACKAGE=\"" + d.ppath.toString() + "\"");
            ctx.addLine("PRIVATE   PACKAGE_NAME=\"" + d.ppath.toString() + "\"");
            ctx.addLine("PRIVATE   PACKAGE_VERSION=\"" + d.version.toString() + "\"");
            ctx.addLine("PRIVATE   PACKAGE_STRING=\"${this}\"");
            ctx.addLine("PRIVATE   PACKAGE_BUILD_CONFIG=\"$<CONFIG>\"");
            ctx.decreaseIndent();
            ctx.addLine(")");
        }

        // export/import
        ctx.addLine("if (LIBRARY_TYPE STREQUAL \"SHARED\")");
        ctx.increaseIndent();
        ctx.addLine("target_compile_definitions    (${this}");
        ctx.increaseIndent();
        if (!header_only)
        {
            ctx.addLine("PRIVATE   " + cppan_api + (d.flags[pfExecutable] ? "" : "=CPPAN_SYMBOL_EXPORT"));
            if (!d.flags[pfExecutable])
                ctx.addLine("INTERFACE " + cppan_api + "=CPPAN_SYMBOL_IMPORT");
        }
        else
        {
            if (d.flags[pfExecutable])
                throw std::runtime_error("Header only target should not be executable: " + d.target_name);
            ctx.addLine("INTERFACE " + cppan_api + "=");
        }
        ctx.decreaseIndent();
        ctx.addLine(")");
        ctx.decreaseIndent();
        ctx.addLine("else()");
        ctx.increaseIndent();
        ctx.addLine("target_compile_definitions    (${this}");
        ctx.increaseIndent();
        if (d.flags[pfExecutable])
            ctx.addLine("PRIVATE    " + cppan_api + "=");
        else
        {
            if (!header_only)
                ctx.addLine("PUBLIC    " + cppan_api + "=");
            else
                ctx.addLine("INTERFACE    " + cppan_api + "=");
        }
        ctx.decreaseIndent();
        ctx.addLine(")");
        ctx.decreaseIndent();
        ctx.addLine("endif()");
        ctx.addLine();

        if (!d.flags[pfExecutable] && !header_only)
        {
            ctx.addLine(R"(set_target_properties(${this} PROPERTIES
    INSTALL_RPATH .
    BUILD_WITH_INSTALL_RPATH True
))");
        }
        ctx.addLine();

        for (auto &ol : p.options)
        {
            ctx.emptyLines(1);

            auto print_target_options = [header_only, &ctx, this](const auto &opts, const String &comment, const String &type)
            {
                if (opts.empty())
                    return;
                ctx.addLine("# " + comment);
                ctx.addLine(type + "(${this}");
                ctx.increaseIndent();
                for (auto &opt : opts)
                {
                    if (header_only)
                        ctx.addLine("INTERFACE " + opt.second);
                    else if (d.flags[pfExecutable])
                        ctx.addLine("PRIVATE " + opt.second);
                    else
                        ctx.addLine(boost::algorithm::to_upper_copy(opt.first) + " " + opt.second);
                }
                ctx.decreaseIndent();
                ctx.addLine(")");
            };

            auto print_defs = [header_only, &ctx, this, &print_target_options](const auto &defs)
            {
                print_target_options(defs, "definitions", "target_compile_definitions");
            };
            auto print_compile_opts = [header_only, &ctx, this, &print_target_options](const auto &copts)
            {
                print_target_options(copts, "compile options", "target_compile_options");
            };
            auto print_linker_opts = [header_only, &ctx, this, &print_target_options](const auto &lopts)
            {
                print_target_options(lopts, "link options", "target_link_libraries");
            };
            auto print_set = [header_only, &ctx, this](const auto &a, const String &s)
            {
                if (a.empty())
                    return;
                ctx.addLine(s + "(${this}");
                ctx.increaseIndent();
                for (auto &def : a)
                {
                    String i;
                    if (header_only)
                        i = "INTERFACE";
                    else if (d.flags[pfExecutable])
                        i = "PRIVATE";
                    else
                        i = "PUBLIC";
                    ctx.addLine(i + " " + def);
                }
                ctx.decreaseIndent();
                ctx.addLine(")");
                ctx.addLine();
            };
            auto print_options = [&ctx, &ol, &print_defs, &print_set, &print_compile_opts, &print_linker_opts]
            {
                print_defs(ol.second.definitions);
                print_compile_opts(ol.second.compile_options);
                print_linker_opts(ol.second.link_options);
                print_linker_opts(ol.second.link_libraries);

                auto print_system = [&ctx](const auto &a, auto f)
                {
                    for (auto &kv : a)
                    {
                        auto k = boost::to_upper_copy(kv.first);
                        ctx.addLine("if (" + k + ")");
                        f(kv.second);
                        ctx.addLine("endif()");
                    }
                };

                print_system(ol.second.system_definitions, print_defs);
                print_system(ol.second.system_compile_options, print_compile_opts);
                print_system(ol.second.system_link_options, print_linker_opts);
                print_system(ol.second.system_link_libraries, print_linker_opts);

                print_set(ol.second.include_directories, "target_include_directories");
            };

            if (ol.first == "any")
            {
                print_options();
            }
            else
            {
                ctx.addLine("if (LIBRARY_TYPE STREQUAL \"" + boost::algorithm::to_upper_copy(ol.first) + "\")");
                print_options();
                ctx.addLine("endif()");
            }
        }
        ctx.emptyLines(1);
    }

    print_bs_insertion(ctx, p, "post target", &BuildSystemConfigInsertions::post_target);

    // aliases
    {
        String tt = d.flags[pfExecutable] ? "add_executable" : "add_library";

        config_section_title(ctx, "aliases");

        if (!d.version.isBranch())
        {

            {
                Version ver = d.version;
                ver.patch = -1;
                ctx.addLine(tt + "(" + d.ppath.toString() + "-" + ver.toAnyVersion() + " ALIAS ${this})");
                ver.minor = -1;
                ctx.addLine(tt + "(" + d.ppath.toString() + "-" + ver.toAnyVersion() + " ALIAS ${this})");
                ctx.addLine(tt + "(" + d.ppath.toString() + " ALIAS ${this})");
                ctx.addLine();
            }

            {
                Version ver = d.version;
                ctx.addLine(tt + "(" + d.ppath.toString("::") + "-" + ver.toAnyVersion() + " ALIAS ${this})");
                ver.patch = -1;
                ctx.addLine(tt + "(" + d.ppath.toString("::") + "-" + ver.toAnyVersion() + " ALIAS ${this})");
                ver.minor = -1;
                ctx.addLine(tt + "(" + d.ppath.toString("::") + "-" + ver.toAnyVersion() + " ALIAS ${this})");
                ctx.addLine(tt + "(" + d.ppath.toString("::") + " ALIAS ${this})");
                ctx.addLine();
            }
        }

        if (!p.aliases.empty())
        {
            ctx.addLine("# user-defined");
            for (auto &a : p.aliases)
                ctx.addLine(tt + "(" + a + " ALIAS ${this})");
            ctx.addLine();
        }
    }

    // private definitions
    if (!header_only)
    {
        config_section_title(ctx, "private definitions");

        // msvc
        ctx.addLine(R"(if (MSVC)
target_compile_definitions(${this}
    PRIVATE _CRT_SECURE_NO_WARNINGS # disable warning about non-standard functions
)
target_compile_options(${this}
    PRIVATE /wd4005 # macro redefinition
    PRIVATE /wd4996 # The POSIX name for this item is deprecated.
)
endif()
)");
    }

    // public definitions
    {
        config_section_title(ctx, "public definitions");

        String visibility;
        if (!d.flags[pfExecutable])
            visibility = !header_only ? "PUBLIC" : "INTERFACE";
        else
            visibility = "PRIVATE";

        // common include directories
        ctx.addLine("target_include_directories(${this}");
        ctx.increaseIndent();
        ctx.addLine(visibility + " ${CMAKE_CURRENT_SOURCE_DIR}");
        ctx.decreaseIndent();
        ctx.addLine(")");
        ctx.addLine();

        // common definitions
        ctx.addLine("target_compile_definitions(${this}");
        ctx.increaseIndent();
        ctx.addLine(visibility + " CPPAN"); // build is performed under CPPAN
        ctx.addLine(visibility + " CPPAN_BUILD"); // build is performed under CPPAN
        ctx.addLine(visibility + " CPPAN_CONFIG=\"${config}\"");
        ctx.addLine(visibility + " CPPAN_SYMBOL_EXPORT=${CPPAN_EXPORT}");
        ctx.addLine(visibility + " CPPAN_SYMBOL_IMPORT=${CPPAN_IMPORT}");
        if (d.flags[pfLocalProject])
            ctx.addLine(visibility + " CPPAN_EXPORT=");
        ctx.decreaseIndent();
        ctx.addLine(")");
        ctx.addLine();

        // common link libraries
        ctx.addLine(R"(if (WIN32)
target_link_libraries(${this}
    )" + visibility + R"( Ws2_32
)
else())");
        ctx.increaseIndent();
        auto add_unix_lib = [this, &ctx, &visibility](const String &s)
        {
            ctx.addLine("find_library(" + s + " " + s + ")");
            ctx.addLine("if (NOT ${" + s + "} STREQUAL \"" + s + "-NOTFOUND\")");
            ctx.increaseIndent();
            ctx.addLine("target_link_libraries(${this}");
            ctx.addLine("    " + visibility + " " + s + "");
            ctx.addLine(")");
            ctx.decreaseIndent();
            ctx.addLine("endif()");
        };
        add_unix_lib("m");
        add_unix_lib("pthread");
        add_unix_lib("rt");
        ctx.decreaseIndent();
        ctx.addLine("endif()");
        ctx.addLine();

        // global definitions
        config_section_title(ctx, "global definitions");

        Context local;
        bool has_defs = false;
        local.addLine("target_compile_definitions(${this}");
        local.increaseIndent();
        for (auto &o : cc->global_options)
        {
            for (auto &opt : o.second.global_definitions)
            {
                local.addLine(visibility + " " + opt);
                has_defs = true;
            }
        }
        local.decreaseIndent();
        local.addLine(")");
        local.addLine();
        if (has_defs)
            ctx += local;
    }

    // definitions
    config_section_title(ctx, "definitions");
    cc->checks.write_definitions(ctx, d);

    // build deps
    print_build_dependencies(ctx, d, "${this}");

    // export
    config_section_title(ctx, "export");
    ctx.addLine("export(TARGETS ${this} FILE " + exports_dir + d.variable_name + ".cmake)");
    ctx.emptyLines(1);

    print_bs_insertion(ctx, p, "post alias", &BuildSystemConfigInsertions::post_alias);

    // dummy target for IDEs with headers only
    if (header_only)
    {
        config_section_title(ctx, "IDE dummy target for headers");

        String tgt = "${this}-headers";
        ctx.addLine("if (CPPAN_SHOW_IDE_PROJECTS)");
        ctx.addLine("add_custom_target(" + tgt + " SOURCES ${src})");
        ctx.addLine();
        ctx.addLine("set_target_properties         (" + tgt + " PROPERTIES");
        ctx.addLine("    FOLDER \"" + packages_folder + "/" + d.ppath.toString() + "/" + d.version.toString() + "\"");
        ctx.addLine(")");
        ctx.addLine("endif()");
        ctx.emptyLines(1);
    }

    // source groups
    print_source_groups(ctx, fn.parent_path());

    file_footer(ctx, d);

    write_if_older(fn, ctx.getText());
}

void CMakePrinter::print_package_actions_file(const path &fn) const
{
    if (!must_update_contents(fn))
        return;

    const auto &p = cc->getDefaultProject();
    const String cppan_api = CPPAN_EXPORT_PREFIX + d.variable_name;
    Context ctx;
    file_header(ctx, d);
    ctx.addLine(config_delimeter);
    ctx.addLine();
    ctx.addLine("set(CMAKE_CURRENT_SOURCE_DIR_OLD ${CMAKE_CURRENT_SOURCE_DIR})");
    ctx.addLine("set(CMAKE_CURRENT_SOURCE_DIR \"" + normalize_path(fn.parent_path().string()) + "\")");
    ctx.addLine("set(CMAKE_CURRENT_BINARY_DIR_OLD ${CMAKE_CURRENT_BINARY_DIR})");
    ctx.addLine("set(CMAKE_CURRENT_BINARY_DIR \"" + normalize_path(get_binary_path(d)) + "\")");
    ctx.addLine();
    ctx.addLine("set(SDIR ${CMAKE_CURRENT_SOURCE_DIR})");
    ctx.addLine("set(BDIR ${CMAKE_CURRENT_BINARY_DIR})");
    ctx.addLine();
    ctx.addLine("set(LIBRARY_API " + cppan_api + ")");
    ctx.addLine();
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

void CMakePrinter::print_package_include_file(const path &fn) const
{
    if (!must_update_contents(fn))
        return;

    Context ctx;
    file_header(ctx, d);

    ctx.addLine("if (TARGET " + d.target_name + ")");
    ctx.addLine("    return()");
    ctx.addLine("endif()");
    ctx.addLine();
    if (d.flags[pfLocalProject])
        ctx.addLine("include(\"" + normalize_path(fn.parent_path().string()) + "/" + cmake_config_filename + "\")");
    else
        ctx.addLine("add_subdirectory(\"" + normalize_path(fn.parent_path().string()) + "\" \"" + get_binary_path(d) + "\")");
    ctx.addLine();

    write_if_older(fn, ctx.getText());
}

void CMakePrinter::print_object_config_file(const path &fn) const
{
    if (!must_update_contents(fn))
        return;

    Context ctx;
    file_header(ctx, d);

    {
        config_section_title(ctx, "cmake settings");
        ctx.addLine(cmake_minimum_required);
        ctx.addLine();
        config_section_title(ctx, "macros & functions");
        ctx.addLine("include(" + normalize_path(directories.get_static_files_dir() / cmake_functions_filename) + ")");
        ctx.addLine();
        if (!d.flags[pfLocalProject])
        {
            config_section_title(ctx, "read passed variables");
            ctx.addLine("read_variables_file(GEN_CHILD_VARS ${VARIABLES_FILE})");
            ctx.addLine();
        }
        else
        {
            config_section_title(ctx, "setup variables");
            ctx.addLine("set(OUTPUT_DIR ${config})");
        }
        ctx.addLine();
        config_section_title(ctx, "output settings");
        ctx.addLine("set(CMAKE_RUNTIME_OUTPUT_DIRECTORY " + normalize_path(directories.storage_dir_bin) + "/${OUTPUT_DIR})");
        ctx.addLine("set(CMAKE_LIBRARY_OUTPUT_DIRECTORY " + normalize_path(directories.storage_dir_lib) + "/${OUTPUT_DIR})");
        ctx.addLine("set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY " + normalize_path(directories.storage_dir_lib) + "/${OUTPUT_DIR})");
        ctx.addLine();

        ctx.addLine("set(CPPAN_USE_CACHE 1)");
    }

    // no need to create a solution for local project
    config_section_title(ctx, "project settings");
    ctx.addLine("project(" + d.getFilesystemHash() + " LANGUAGES C CXX)");
    ctx.addLine();

    config_section_title(ctx, "compiler & linker settings");
    ctx.addLine(R"(if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if (MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

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
    ctx.addLine("add_subdirectory(" + normalize_path(cc->settings.cppan_dir) + ")");

    // main include
    {
        config_section_title(ctx, "main include");
        auto mi = d.getDirSrc();
        add_subdirectory(ctx, mi.string());
        ctx.emptyLines(1);
    }

    file_footer(ctx, d);

    write_if_older(fn, ctx.getText());
}

void CMakePrinter::print_object_include_config_file(const path &fn) const
{
    if (!must_update_contents(fn))
        return;

    const auto &p = cc->getDefaultProject();
    const auto &dd = rd[d].dependencies;

    Context ctx;
    file_header(ctx, d);

    ctx.addLine("set(target " + d.target_name + ")");
    ctx.addLine();
    if (!p.aliases.empty())
    {
        ctx.addLine("set(aliases");
        ctx.increaseIndent();
        for (auto &a : p.aliases)
            ctx.addLine(a);
        ctx.decreaseIndent();
        ctx.addLine(")");
        ctx.addLine();
    }
    ctx.addLine("set(current_dir " + normalize_path(fn.parent_path()) + ")");
    ctx.addLine("set(storage_cfg_dir " + normalize_path(directories.storage_dir_cfg) + ")");
    ctx.addLine();
    ctx.addLine("set(variable_name " + d.variable_name + ")");
    ctx.addLine();
    ctx.addLine("set(EXECUTABLE " + String(d.flags[pfExecutable] ? "1" : "0") + ")");
    ctx.addLine();

    ctx.addLine(cmake_generate_file);

    config_section_title(ctx, "import direct deps");
    ctx.addLine("include(${current_dir}/exports.cmake)");
    ctx.addLine();

    config_section_title(ctx, "include current export file");
    ctx.addLine("if (NOT TARGET " + d.target_name + ")"); // remove cond?
    ctx.addLine("    include(${import_fixed})");
    ctx.addLine("endif()");
    ctx.addLine();

    // src target
    {
        auto target = d.target_name + "-sources";
        auto dir = d.getDirSrc();

        // begin
        if (!d.flags[pfLocalProject])
        {
            ctx.addLine("if (CPPAN_SHOW_IDE_PROJECTS)");
            ctx.addLine();
        }
        config_section_title(ctx, "sources target (for IDE only)");
        ctx.addLine("if (NOT TARGET " + target + ")");
        ctx.increaseIndent();
        if (d.flags[pfLocalProject])
            print_local_project_files(ctx, p);
        else
            ctx.addLine("file(GLOB_RECURSE src \"" + normalize_path(dir) + "/*\")");
        ctx.addLine();
        ctx.addLine("add_custom_target(" + target);
        ctx.addLine("    SOURCES ${src}");
        ctx.addLine(")");
        ctx.addLine();

        // solution folder
        if (!d.flags[pfLocalProject])
        {
            ctx.addLine("set_target_properties         (" + target + " PROPERTIES");
            ctx.addLine("    FOLDER \"" + packages_folder + "/" + d.ppath.toString() + "/" + d.version.toString() + "\"");
            ctx.addLine(")");
        }
        ctx.decreaseIndent();
        ctx.addLine("endif()");
        ctx.emptyLines(1);

        // source groups
        print_source_groups(ctx, dir);

        // end
        if (!d.flags[pfLocalProject])
            ctx.addLine("endif()");
    }

    file_footer(ctx, d);

    write_if_older(fn, ctx.getText());
}

void CMakePrinter::print_object_export_file(const path &fn) const
{
    if (!must_update_contents(fn))
        return;

    const auto &dd = rd[d].dependencies;
    Context ctx;
    file_header(ctx, d);

    for (auto &dp : dd)
    {
        auto &dep = dp.second;

        if (dep.flags[pfIncludeDirectoriesOnly])
            continue;

        auto b = dep.getDirObj();
        auto p = b / cppan_build_dir;
        if (!dep.flags[pfExecutable])
            p /= "${config_lib_gen}";
        else
            p /= "${config_exe}";
        p /= path("exports") / (dep.variable_name + "-fixed.cmake");

        if (!dep.flags[pfHeaderOnly])
            ctx.addLine("include(\"" + normalize_path(b / exports_filename) + "\")");
        ctx.addLine("if (NOT TARGET " + dep.target_name + ")");
        ctx.increaseIndent();
        if (dep.flags[pfHeaderOnly])
            add_subdirectory(ctx, dep.getDirSrc().string());
        else
        {
            ctx.addLine("if (NOT EXISTS \"" + normalize_path(p) + "\")");
            ctx.addLine("    include(\"" + normalize_path(b / cmake_object_config_filename) + "\")");
            ctx.addLine("endif()");
            ctx.addLine("include(\"" + normalize_path(p) + "\")");
        }
        ctx.decreaseIndent();
        ctx.addLine("endif()");
        ctx.addLine();
    }

    write_if_older(fn, ctx.getText());
}

void CMakePrinter::print_object_build_file(const path &fn) const
{
    if (!must_update_contents(fn))
        return;

    const auto &dd = rd[d].dependencies;
    Context ctx;
    file_header(ctx, d);

    ctx.addLine("set(fn1 \"" + normalize_path(d.getStampFilename()) + "\")");
    ctx.addLine("set(fn2 \"${BUILD_DIR}/" + cppan_stamp_filename + "\")");
    ctx.addLine();
    ctx.addLine(cmake_build_file);

    write_if_older(fn, ctx.getText());
}

void CMakePrinter::print_meta_config_file(const path &fn) const
{
    if (!must_update_contents(fn))
        return;

    Context ctx;
    file_header(ctx, d);

    ctx.addLine("#");
    ctx.addLine("# cppan");
    ctx.addLine("# meta config file");
    ctx.addLine("#");
    ctx.addLine();

    config_section_title(ctx, "cmake setup");
    ctx.addLine(cmake_minimum_required);

    config_section_title(ctx, "macros & functions");
    ctx.addLine("include(" + normalize_path(directories.get_static_files_dir() / cmake_functions_filename) + ")");

    config_section_title(ctx, "variables");
    ctx.addLine("set(CPPAN_BUILD 1 CACHE STRING \"CPPAN is turned on\")");
    ctx.addLine();
    print_storage_dirs(ctx);
    //ctx.addLine("set(CPPAN_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})"); // why?
    //ctx.addLine("set(CPPAN_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})"); // why?
    ctx.addLine();
    ctx.addLine("set(CMAKE_POSITION_INDEPENDENT_CODE ON)");
    ctx.addLine();
    ctx.addLine("set(${CMAKE_CXX_COMPILER_ID} 1)");
    ctx.addLine();
    ctx.addLine("if (NOT DEFINED CPPAN_USE_CACHE)");
    ctx.addLine(String("set(CPPAN_USE_CACHE ") + (cc->settings.use_cache ? "1" : "0") + ")");
    ctx.addLine("endif()");
    ctx.addLine();
    ctx.addLine("if (NOT DEFINED CPPAN_SHOW_IDE_PROJECTS)");
    ctx.addLine(String("set(CPPAN_SHOW_IDE_PROJECTS ") + (cc->settings.show_ide_projects ? "1" : "0") + ")");
    ctx.addLine("endif()");
    ctx.addLine();
    ctx.addLine("if (NOT DEFINED CPPAN_BUILD_EXECUTABLES_WITH_SAME_CONFIG)");
    ctx.addLine("set(CPPAN_BUILD_EXECUTABLES_WITH_SAME_CONFIG 0)");
    ctx.addLine("endif()");
    ctx.addLine();
    ctx.addLine("get_configuration_variables()");
    ctx.addLine();

    ctx.addLine("include(" + cmake_helpers_filename + ")");
    ctx.addLine();

    // deps
    print_dependencies(ctx, rd[d].dependencies, cc->settings.use_cache);

    const String cppan_project_name = "cppan";

    if (d.empty())
    {
        // lib
        config_section_title(ctx, "main library");
        ctx.addLine("add_library                   (" + cppan_project_name + " INTERFACE)");
        ctx.addLine("target_link_libraries         (" + cppan_project_name);
        ctx.increaseIndent();
        for (auto &p : rd[d].dependencies)
        {
            if (p.second.flags[pfExecutable] || p.second.flags[pfIncludeDirectoriesOnly])
                continue;
            ctx.addLine("INTERFACE " + p.second.target_name);
        }
        ctx.decreaseIndent();
        ctx.addLine(")");
        ctx.addLine("add_dependencies(" + cppan_project_name + " " + cppan_dummy_target(cppan_dummy_copy_target) + ")");
        ctx.addLine();
        ctx.addLine("export(TARGETS " + cppan_project_name + " FILE " + exports_dir + "cppan.cmake)");

        // exe deps
        /*{
            config_section_title(ctx, "exe deps");

            ctx.addLine("if (CPPAN_USE_CACHE)");
            ctx.increaseIndent();

            for (auto &dp : rd[d].dependencies)
            {
                auto &d = dp.second;
                if (d.flags[pfExecutable])
                    // maybe switch d.target_name and cppan_project_name
                    ctx.addLine("add_dependencies(" + d.target_name + " " + cppan_project_name + ")");
            }

            ctx.decreaseIndent();
            ctx.addLine("endif()");
            ctx.addLine();
        }*/

        // re-run cppan when root cppan.yml is changed
        if (cc->settings.add_run_cppan_target)
        {
            config_section_title(ctx, "cppan regenerator");
            ctx.addLine(R"(set(file ${CMAKE_CURRENT_BINARY_DIR}/run-cppan.txt)
add_custom_command(OUTPUT ${file}
    COMMAND cppan -d ${PROJECT_SOURCE_DIR}
    COMMAND ${CMAKE_COMMAND} -E echo "" > ${file}
    DEPENDS ${PROJECT_SOURCE_DIR}/cppan.yml
)
add_custom_target(run-cppan
    DEPENDS ${file}
    SOURCES
        ${PROJECT_SOURCE_DIR}/cppan.yml
        \")" + normalize_path(directories.get_static_files_dir() / cmake_functions_filename) + R"(\"
        ${PROJECT_SOURCE_DIR}/cppan/)" + cmake_helpers_filename + R"(
)
add_dependencies()" + cppan_project_name + R"( run-cppan)
set_target_properties(run-cppan PROPERTIES
    FOLDER "cppan/service"
))");
        }

        // build deps
        print_build_dependencies(ctx, d, cppan_dummy_target(cppan_dummy_build_target));

        // copy deps
        {
            config_section_title(ctx, "copy dependencies");

            ctx.addLine("if (CPPAN_USE_CACHE)");
            ctx.increaseIndent();

            ctx.addLine("if (NOT COPY_LIBRARIES_TO_OUTPUT)");
            ctx.increaseIndent();
            ctx.addLine("set(output_dir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})");
            ctx.addLine("if (MSVC OR XCODE)");
            ctx.addLine("    set(output_dir ${output_dir}/$<CONFIG>)");
            ctx.addLine("endif()");
            ctx.addLine("if (CPPAN_BUILD_OUTPUT_DIR)");
            ctx.addLine("    set(output_dir ${CPPAN_BUILD_OUTPUT_DIR})");
            ctx.addLine("endif()");
            ctx.addLine();

            Packages copy_deps;
            gather_copy_deps(ctx, rd[d].dependencies, copy_deps);
            for (auto &dp : copy_deps)
            {
                auto &p = dp.second;
                // do not copy static only projects
                if (rd[p].config->getDefaultProject().static_only ||
                    !rd[p].config->getDefaultProject().copy_to_output_dir)
                    continue;

                ctx.addLine("get_target_property(type " + p.target_name + " TYPE)");
                ctx.addLine("if (NOT ${type} STREQUAL STATIC_LIBRARY)");
                ctx.increaseIndent();
                ctx.addLine("add_custom_command(TARGET " + cppan_dummy_target(cppan_dummy_copy_target) + " POST_BUILD");
                ctx.increaseIndent();
                ctx.addLine("COMMAND ${CMAKE_COMMAND} -E copy_if_different");
                ctx.increaseIndent();
                if (p.flags[pfLocalProject])
                {
                    auto &dp = rd[p].config->getDefaultProject();
                    if (dp.type == ProjectType::Executable)
                        ctx.addLine("$<TARGET_FILE:" + p.target_name + "> ${output_dir}/" + p.ppath.back() + "${CMAKE_EXECUTABLE_SUFFIX}");
                    else
                        ctx.addLine("$<TARGET_FILE:" + p.target_name + "> ${output_dir}/" + p.ppath.back() + "${CMAKE_SHARED_LIBRARY_SUFFIX}");
                }
                else
                    ctx.addLine("$<TARGET_FILE:" + p.target_name + "> ${output_dir}/$<TARGET_FILE_NAME:" + p.target_name + ">");
                ctx.decreaseIndent();
                ctx.decreaseIndent();
                ctx.addLine(")");
                ctx.decreaseIndent();
                ctx.addLine("endif()");
                ctx.addLine();
            }

            ctx.decreaseIndent();
            ctx.addLine("endif()");
            ctx.addLine();

            ctx.decreaseIndent();
            ctx.addLine("endif()");
            ctx.addLine();
        }
    }

    file_footer(ctx, d);

    write_if_older(fn, ctx.getText());
}

void CMakePrinter::print_helper_file(const path &fn) const
{
    if (!must_update_contents(fn))
        return;

    Context ctx;
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
        ctx.addLine("if (NOT CPPAN_COMMAND)");
        ctx.increaseIndent();
        ctx.addLine("find_program(CPPAN_COMMAND cppan)");
        ctx.addLine("if (\"${CPPAN_COMMAND}\" STREQUAL \"CPPAN_COMMAND-NOTFOUND\")");
        ctx.increaseIndent();
        ctx.addLine("message(FATAL_ERROR \"'cppan' program was not found. Please, add it to PATH environment variable\")");
        ctx.decreaseIndent();
        ctx.addLine("endif()");
        ctx.decreaseIndent();
        ctx.addLine("endif()");
        ctx.addLine();
    }
    ctx.addLine("get_configuration(config)");
    ctx.addLine("get_configuration_with_generator(config_dir)");
    ctx.addLine("get_number_of_cores(N_CORES)");
    ctx.addLine();
    ctx.addLine("file_write_once(${PROJECT_BINARY_DIR}/" CPPAN_CONFIG_FILENAME " \"${config_dir}\")");
    ctx.addLine();
    ctx.addLine("set(XCODE 0)");
    ctx.addLine("if (CMAKE_GENERATOR STREQUAL \"Xcode\")");
    ctx.addLine("    set(XCODE 1)");
    ctx.addLine("endif()");
    ctx.addLine();

    // use response files when available
    ctx.addLine("set(CMAKE_C_USE_RESPONSE_FILE_FOR_INCLUDES    1 CACHE STRING \"\")");
    ctx.addLine("set(CMAKE_C_USE_RESPONSE_FILE_FOR_OBJECTS     1 CACHE STRING \"\")");
    ctx.addLine("set(CMAKE_C_USE_RESPONSE_FILE_FOR_LIBRARIES   1 CACHE STRING \"\")");
    ctx.addLine("set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_INCLUDES  1 CACHE STRING \"\")");
    ctx.addLine("set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_OBJECTS   1 CACHE STRING \"\")");
    ctx.addLine("set(CMAKE_CXX_USE_RESPONSE_FILE_FOR_LIBRARIES 1 CACHE STRING \"\")");
    // unknown meaning atm, so turned off
    //ctx.addLine("set(CMAKE_CXX_RESPONSE_FILE_LINK_FLAG \"@\" CACHE STRING \"\")");
    ctx.addLine();

    // cmake includes
    config_section_title(ctx, "cmake includes");
    ctx.addLine(cmake_includes);

    // checks
    {
        // common checks
        config_section_title(ctx, "common checks");

        // read vars file
        ctx.addLine("set(vars_file \"" + normalize_path(directories.storage_dir_cfg) + "/${config}.cmake\")");
        if (!d.flags[pfLocalProject])
            ctx.addLine("read_check_variables_file(${vars_file})");
        ctx.addLine();

        ctx.addLine("if (NOT DEFINED WORDS_BIGENDIAN)");
        ctx.increaseIndent();
        ctx.addLine("test_big_endian(WORDS_BIGENDIAN)");
        ctx.addLine("add_check_variable(WORDS_BIGENDIAN)");
        ctx.decreaseIndent();
        ctx.addLine("endif()");
        // aliases
        ctx.addLine("set(BIG_ENDIAN ${WORDS_BIGENDIAN} CACHE STRING \"endianness alias\")");
        ctx.addLine("set(BIGENDIAN ${WORDS_BIGENDIAN} CACHE STRING \"endianness alias\")");
        ctx.addLine("set(HOST_BIG_ENDIAN ${WORDS_BIGENDIAN} CACHE STRING \"endianness alias\")");
        ctx.addLine();

        // parallel checks
        if (d.empty())
        {
            config_section_title(ctx, "parallel checks");

            // parallel cygwin process work really bad, so disable parallel checks for it
            ctx.addLine("if (NOT CYGWIN)");
            ctx.increaseIndent();
            ctx.addLine("set(tmp_dir \"" + normalize_path(temp_directory_path() / "vars") + "\")");
            ctx.addLine("string(RANDOM LENGTH 8 vars_dir)");
            ctx.addLine("set(tmp_dir \"${tmp_dir}/${vars_dir}\")");
            ctx.addLine();
            ctx.addLine("set(checks_file \"" + normalize_path(cwd / cc->settings.cppan_dir / cppan_checks_yml) + "\")");
            ctx.addLine();
            ctx.addLine("execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${PROJECT_BINARY_DIR}/CMakeFiles ${tmp_dir}/CMakeFiles/)");
            ctx.addLine("execute_process(COMMAND ${CPPAN_COMMAND} internal-parallel-vars-check ${tmp_dir} ${vars_file} ${checks_file} ${CMAKE_GENERATOR} ${CMAKE_TOOLCHAIN_FILE})");
            // this file is created by parallel checks dispatcher
            ctx.addLine("read_check_variables_file(${tmp_dir}/" + parallel_checks_file + ")");
            ctx.addLine("set(CPPAN_NEW_VARIABLE_ADDED 1)");
            ctx.addLine();
            ctx.addLine("file(REMOVE_RECURSE ${tmp_dir})");
            ctx.decreaseIndent();
            ctx.addLine("endif()");
            ctx.addLine();
        }

        // checks
        config_section_title(ctx, "checks");
        cc->checks.write_checks(ctx);

        // write vars file
        if (!d.flags[pfLocalProject])
        {
            ctx.addLine("if (CPPAN_NEW_VARIABLE_ADDED)");
            ctx.addLine("    write_check_variables_file(${vars_file})");
            ctx.addLine("endif()");
        }
    }

    // fixups
    // put bug workarounds here
    //config_section_title(ctx, "fixups");
    ctx.emptyLines(1);

    // dummy (compiled?) target
    if (d.empty())
    {
        declare_dummy_target(ctx, cppan_dummy_build_target);
        declare_dummy_target(ctx, cppan_dummy_copy_target);
    }

    file_footer(ctx, d);

    write_if_older(fn, ctx.getText());
}

void CMakePrinter::parallel_vars_check(const path &dir, const path &vars_file, const path &checks_file, const String &generator, const String &toolchain) const
{
    static const String cppan_variable_result_filename = "result.cppan";
    const auto N = std::thread::hardware_concurrency();

    Checks checks;
    checks.load(checks_file);

    // read known vars
    if (fs::exists(vars_file))
    {
        std::set<String> known_vars;
        std::vector<String> lines;
        {
            ScopedShareableFileLock lock(vars_file);
            lines = read_lines(vars_file);
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

    // There are few checks only. Won't go in parallel mode.
    if (checks.checks.size() <= 8)
        return;

    Executor e(N);
    e.throw_exceptions = true;

    auto workers = checks.scatter(N);
    size_t n_checks = 0;
    for (auto &w : workers)
        n_checks += w.checks.size();

    LOG_INFO(logger, "-- Performing " << n_checks << " checks using " << N << " threads");
    LOG_INFO(logger, "-- This process may take up to 5 minutes depending on your hardware");
    LOG_FLUSH();

    auto work = [&dir, &generator, &toolchain](auto &w, int i)
    {
        if (w.checks.empty())
            return;

        auto d = dir / std::to_string(i);
        fs::create_directories(d);

        Context ctx;
        ctx.addLine(cmake_minimum_required);
        ctx.addLine("project(" + std::to_string(i) + " LANGUAGES C CXX)");
        ctx.addLine(cmake_includes);
        w.write_parallel_checks_for_workers(ctx);
        write_file(d / cmake_config_filename, ctx.getText());

        // copy cached cmake dir
        copy_dir(dir / "CMakeFiles", d / "CMakeFiles");

        // run cmake
        command::Args args;
        args.push_back("cmake");
        args.push_back("-H" + normalize_path(d));
        args.push_back("-B" + normalize_path(d));
        args.push_back("-G");
        args.push_back(generator);
        if (!toolchain.empty())
            args.push_back("-DCMAKE_TOOLCHAIN_FILE=" + toolchain);
        auto ret = command::execute(args);

        if (ret.rc)
            throw std::runtime_error("Error during evaluating variables");

        w.read_parallel_checks_for_workers(d);
    };

    int i = 0;
    for (auto &w : workers)
        e.push([&work, &w, n = i++]() { work(w, n); });

    auto t = get_time_seconds([&e] { e.wait(); });

    for (auto &w : workers)
        checks += w;

    checks.print_values();

    Context ctx;
    checks.print_values(ctx);
    write_file(dir / parallel_checks_file, ctx.getText());

    LOG_FLUSH();
    LOG_INFO(logger, "-- This operation took " + std::to_string(t) + " seconds to complete");
}

bool CMakePrinter::must_update_contents(const path &fn) const
{
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
