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

#include "cppan.h"

#include <fstream>
#include <iostream>
#include <regex>
#include <tuple>

#include <boost/algorithm/string.hpp>

#include <curl/curl.h>
#include <curl/easy.h>

#ifdef _WIN32
#include <windows.h>

#include "shell_link.h"
#endif

#include "access_table.h"
#include "context.h"
#include "inserts.h"

#define CPPAN_LOCAL_DIR "cppan"

#define LOG_NO_NEWLINE(x)   \
    do                      \
    {                       \
        if (!silent)        \
            std::cout << x; \
    } while (0)

#define LOG(x)                      \
    do                              \
    {                               \
        if (!silent)                \
            std::cout << x << "\n"; \
    } while (0)

bool silent = false;

const String cmake_config_filename = "CMakeLists.txt";
const String cmake_object_config_filename = "generate.cmake";
const String cmake_helpers_filename = "helpers.cmake";
const String cmake_functions_filename = "functions.cmake";
const String cppan_dummy_target = "cppan-dummy";
const String cppan_helpers_target = "cppan-helpers";
const String cppan_helpers_private_target = "cppan-helpers-private";
const String exports_dir = "${CMAKE_BINARY_DIR}/exports/";
const String non_local_build_file = "build.cmake";
const String cmake_minimum_required = "cmake_minimum_required(VERSION 3.2.0)";
const String packages_folder = "cppan/packages";
const String include_guard_filename = "include.cmake";
const String include_guard_prefix = "CPPAN_INCLUDE_GUARD_";
const String actions_filename = "actions.cmake";
const String exports_filename = "exports.cmake";
const String cpp_config_filename = "cppan.h";
const String cppan_export = "CPPAN_EXPORT";
const String cppan_export_prefix = "CPPAN_API_";
const String cppan_local_build_prefix = "cppan-build-";

const std::vector<String> cmake_configuration_types = { "DEBUG", "MINSIZEREL", "RELEASE", "RELWITHDEBINFO" };
const std::vector<String> cmake_configuration_types_no_rel = { "DEBUG", "MINSIZEREL", "RELWITHDEBINFO" };

using ConfigPtr = std::shared_ptr<Config>;
std::map<Dependency, ConfigPtr> config_store;

String repeat(const String &e, int n)
{
    String s;
    s.reserve(e.size() * n);
    for (int i = 0; i < n; i++)
        s += e;
    return s;
}

const String config_delimeter_short = repeat("#", 40);
const String config_delimeter = config_delimeter_short + config_delimeter_short;

path get_home_directory()
{
#ifdef WIN32
    auto home = getenv("USERPROFILE");
    if (!home)
        throw std::runtime_error("Cannot get user's home directory (%USERPROFILE%)");
#else
    auto home = getenv("HOME");
    if (!home)
        throw std::runtime_error("Cannot get user's home directory ($HOME)");
#endif
    return home;
}

path get_config_filename()
{
    return get_root_directory() / ".cppan";
}

path get_root_directory()
{
    return get_home_directory() / ".cppan";
}

void config_section_title(Context &ctx, const String &t)
{
    ctx.emptyLines(1);
    ctx.addLine(config_delimeter);
    ctx.addLine("#");
    ctx.addLine("# " + t);
    ctx.addLine("#");
    ctx.addLine(config_delimeter);
    ctx.emptyLines(1);
}

void get_config_insertion(const yaml &n, const String &key, String &dst)
{
    dst = get_scalar<String>(n, key);
    boost::trim(dst);
}

ptree url_post(const String &url, const ptree &data)
{
    ptree p;
    std::ostringstream oss;
    pt::write_json(oss, data
#if !defined(CPPAN_TEST)
        , false
#endif
        );
    auto response = url_post(url, oss.str());
    std::istringstream iss(response);
    pt::read_json(iss, p);
    return p;
}

String normalize_path(const path &p)
{
    String s = p.string();
    boost::algorithm::replace_all(s, "\\", "/");
    return s;
}

String get_binary_path(const ProjectPath &p, const Version &v)
{
    return "${CMAKE_BINARY_DIR}/cppan/" + sha1(p.toString() + " " + v.toString()).substr(0, 10);
}

String add_subdirectory(String src, String bin = String())
{
    boost::algorithm::replace_all(src, "\\", "/");
    boost::algorithm::replace_all(bin, "\\", "/");
    return "include(\"" + src + "/" + include_guard_filename + "\")";
}

void add_subdirectory(Context &ctx, const String &src, const String &bin = String())
{
    ctx << add_subdirectory(src, bin) << Context::eol;
}

void print_dependencies(Context &ctx, const Config &c, const Dependencies &dd, const Dependencies &id, bool obj_dir = false)
{
    std::vector<String> includes;

    if (dd.empty())
        return;

    auto base_dir = c.get_storage_dir_src();
    if (obj_dir)
        base_dir = c.get_storage_dir_obj();

    auto print_deps = [&](const auto &dd, const String &prefix = "")
    {
        config_section_title(ctx, prefix + "direct dependencies");
        for (auto &p : dd)
        {
            String s;
            auto dir = base_dir;
            if (p.second.flags[pfHeaderOnly] ||
                p.second.flags[pfIncludeDirectories])
            {
                dir = c.get_storage_dir_src();
                s = p.second.getPackageDir(dir).string();
            }
            else if (obj_dir)
            {
                s = p.second.getPackageDirHash(dir).string();
            }
            else
            {
                dir = c.get_storage_dir_src();
                s = p.second.getPackageDir(dir).string();
            }

            if (p.second.flags[pfIncludeDirectories])
                ;// ctx.addLine("include(\"" + normalize_path(s) + "/" + actions_filename + "\")");
            else if (c.local_build || p.second.flags[pfHeaderOnly])
                add_subdirectory(ctx, s, get_binary_path(p.second.package, p.second.version));
            else
            {
                includes.push_back("include(\"" + normalize_path(s) + "/" + cmake_object_config_filename + "\")");
            }
        }
        ctx.addLine();
    };
    print_deps(dd);
    //print_deps(id, "in");

    if (!includes.empty())
    {
        config_section_title(ctx, "include dependencies (they should be placed at the end)");
        for (auto &line : includes)
            ctx.addLine(line);
    }
}

void print_dependencies(Context &ctx, const Config &c, bool obj_dir = false)
{
    print_dependencies(ctx, c, c.getDirectDependencies(), c.getIndirectDependencies(), obj_dir);
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

ConfigPtr getConfig(const Dependency &d, const path &src_dir)
{
    ConfigPtr c;
    if (config_store.find(d) == config_store.end())
        c = config_store[d] = std::make_shared<Config>(src_dir / d.package.toString() / d.version.toString());
    else
        c = config_store[d];
    return c;
}

void prepare_exports(const Files &files, const Dependency &d)
{
    // very stupid algorithm
    PackageInfo pi(d);
    auto api = cppan_export_prefix + pi.variable_name;

    for (auto &f : files)
    {
        auto s = read_file(f, true);
        boost::algorithm::replace_all(s, cppan_export, api);
        write_file(f, s);
    }
}

String get_stamp_filename(const String &prefix)
{
    return prefix + ".md5";
}

void print_copy_deps(Context &ctx, const Dependencies &dd)
{
    for (auto &dp : dd)
    {
        auto &d = dp.second;
        PackageInfo pi(d);

        if (d.flags[pfExecutable] || d.flags[pfHeaderOnly] || d.flags[pfIncludeDirectories])
            continue;

        ctx.addLine("add_custom_command(TARGET " + cppan_dummy_target + " POST_BUILD");
        ctx.increaseIndent();
        ctx.addLine("COMMAND ${CMAKE_COMMAND} -E copy_if_different");
        ctx.increaseIndent();
        ctx.addLine("$<TARGET_FILE:" + pi.target_name + "> ${output_dir}/$<TARGET_FILE_NAME:" + pi.target_name + ">");
        ctx.decreaseIndent();
        ctx.decreaseIndent();
        ctx.addLine(")");
        ctx.addLine();

        auto i = config_store.find(d);
        if (i == config_store.end())
            continue;
        print_copy_deps(ctx, i->second->getDefaultProject().dependencies);
    }
}

void BuildSystemConfigInsertions::get_config_insertions(const yaml &n)
{
#define ADD_CFG_INSERTION(x) get_config_insertion(n, #x, x)
    ADD_CFG_INSERTION(pre_sources);
    ADD_CFG_INSERTION(post_sources);
    ADD_CFG_INSERTION(post_target);
    ADD_CFG_INSERTION(post_alias);
#undef ADD_CFG_INSERTION
}

PackageInfo::PackageInfo(const Dependency &d)
{
    dependency = std::make_unique<Dependency>(d);
    auto v = d.version.toAnyVersion();
    target_name = d.package.toString() + (v == "*" ? "" : ("-" + v));
    variable_name = d.package.toString() + "_" + (v == "*" ? "" : ("_" + v));
    std::replace(variable_name.begin(), variable_name.end(), '.', '_');
}

void BuildSettings::load(const yaml &root)
{
    if (root.IsNull())
        return;

    // extract
    EXTRACT_AUTO(cmake_options);
    EXTRACT_AUTO(c_compiler);
    EXTRACT_AUTO(cxx_compiler);
    EXTRACT_AUTO(compiler);
    EXTRACT_AUTO(c_compiler_flags);
    if (c_compiler_flags.empty())
        EXTRACT_VAR(root, c_compiler_flags, "c_flags", String);
    EXTRACT_AUTO(cxx_compiler_flags);
    if (cxx_compiler_flags.empty())
        EXTRACT_VAR(root, cxx_compiler_flags, "cxx_flags", String);
    EXTRACT_AUTO(compiler_flags);
    EXTRACT_AUTO(link_flags);
    EXTRACT_AUTO(link_libraries);
    EXTRACT_AUTO(configuration);
    EXTRACT_AUTO(generator);
    EXTRACT_AUTO(toolset);
    EXTRACT_AUTO(type);
    EXTRACT_AUTO(library_type);
    EXTRACT_AUTO(executable_type);
    EXTRACT_AUTO(use_shared_libs);
    EXTRACT_AUTO(silent);

    for (int i = 0; i < CMakeConfigurationType::Max; i++)
    {
        auto t = cmake_configuration_types[i];
        boost::to_lower(t);
        EXTRACT_VAR(root, c_compiler_flags_conf[i], "c_compiler_flags_" + t, String);
        EXTRACT_VAR(root, cxx_compiler_flags_conf[i], "cxx_compiler_flags_" + t, String);
        EXTRACT_VAR(root, compiler_flags_conf[i], "compiler_flags_" + t, String);
        EXTRACT_VAR(root, link_flags_conf[i], "link_flags_" + t, String);
    }

    cmake_options = get_sequence<String>(root["cmake_options"]);
    get_string_map(root, "env", env);

    // process
    if (c_compiler.empty())
        c_compiler = cxx_compiler;
    if (c_compiler.empty())
        c_compiler = compiler;
    if (cxx_compiler.empty())
        cxx_compiler = compiler;

    c_compiler_flags += " " + compiler_flags;
    cxx_compiler_flags += " " + compiler_flags;
    for (int i = 0; i < BuildSettings::CMakeConfigurationType::Max; i++)
    {
        c_compiler_flags_conf[i] += " " + compiler_flags_conf[i];
        cxx_compiler_flags_conf[i] += " " + compiler_flags_conf[i];
    }
}

Config::Config()
{
    storage_dir = get_root_directory() / "packages";
    build_dir = temp_directory_path() / "build";

    // add some common types
    check_types.insert("size_t");
    check_types.insert("void *");
}

Config::Config(const path &p)
    : Config()
{
    if (fs::is_directory(p))
    {
        auto old = fs::current_path();
        fs::current_path(p);
        load_current_config();
        fs::current_path(old);
    }
    else
        load(p);
    dir = p;
}

Config Config::load_system_config()
{
    auto fn = CONFIG_ROOT "default";
    Config c;
    if (!fs::exists(fn))
        return c;
    c.load_common(fn);
    return c;
}

Config Config::load_user_config()
{
    auto fn = get_config_filename();
    if (!fs::exists(fn))
    {
        boost::system::error_code ec;
        fs::create_directories(fn.parent_path(), ec);
        if (ec)
            throw std::runtime_error(ec.message());
        Config c = load_system_config();
        c.save(fn);
        return c;
    }
    Config c = load_system_config();
    c.load_common(fn);
    return c;
}

void Config::load_current_config()
{
    load(fs::current_path() / CPPAN_FILENAME);
}

void Config::load_common(const path &p)
{
    auto root = YAML::LoadFile(p.string());
    load_common(root);
}

void Config::load_common(const yaml &root)
{
    EXTRACT_AUTO(host);
    EXTRACT_AUTO(local_build);
    EXTRACT_AUTO(show_ide_projects);
    EXTRACT_AUTO(add_run_cppan_target);
    EXTRACT(storage_dir, String);
    EXTRACT(build_dir, String);

    auto &p = root["proxy"];
    if (p.IsDefined())
    {
        if (!p.IsMap())
            throw std::runtime_error("'proxy' should be a map");
        EXTRACT_VAR(p, proxy.host, "host", String);
        EXTRACT_VAR(p, proxy.user, "user", String);
    }

    storage_dir_type = packages_dir_type_from_string(get_scalar<String>(root, "storage_dir_type", "user"), "storage_dir_type");
    if (root["storage_dir"].IsDefined())
        storage_dir_type = PackagesDirType::None;
    build_dir_type = packages_dir_type_from_string(get_scalar<String>(root, "build_dir_type", "system"), "build_dir_type");
    if (root["build_dir"].IsDefined())
        build_dir_type = PackagesDirType::None;
}

void Config::load(const path &p)
{
    const auto root = YAML::LoadFile(p.string());
    load(root);
}

void Config::load(const yaml &root, const path &p)
{
    auto ls = root["local_settings"];
    if (ls.IsDefined())
    {
        if (!ls.IsMap())
            throw std::runtime_error("'local_settings' should be a map");
        load_common(ls);

        // read build settings
        if (ls["builds"].IsDefined() && ls["current_build"].IsDefined())
            build_settings.load(ls["builds"][ls["current_build"].template as<String>()]);
        else if (ls["build"].IsDefined())
            build_settings.load(ls["build"]);
    }

    // version
    {
        String ver;
        EXTRACT_VAR(root, ver, "version", String);
        if (!ver.empty())
            version = Version(ver);
    }

    source = load_source(root);

    EXTRACT(root_project, String);

    // global checks
    auto check = [&root](auto &a, auto &&str)
    {
        auto s = get_sequence<String>(root, str);
        a.insert(s.begin(), s.end());
    };

    check(check_functions, "check_function_exists");
    check(check_includes, "check_include_exists");
    check(check_types, "check_type_size");
    check(check_libraries, "check_library_exists");

    get_map_and_iterate(root, "check_symbol_exists", [this](const auto &root)
    {
        auto f = root.first.template as<String>();
        auto s = root.second.template as<String>();
        if (root.second.IsSequence())
            check_symbols[f] = get_sequence_set<String>(root.second);
        else if (root.second.IsScalar())
            check_symbols[f].insert(s);
        else
            throw std::runtime_error("Symbol headers should be a scalar or a set");
    });

    // global insertions
    bs_insertions.get_config_insertions(root);

    // project
    auto set_project = [this, &p](auto &&project, auto &&name)
    {
        project.cppan_filename = p.filename().string();
        project.package = relative_name_to_absolute(root_project, name);
        projects.emplace(project.package.toString(), project);
    };

    const auto &prjs = root["projects"];
    if (prjs.IsDefined())
    {
        if (!prjs.IsMap())
            throw std::runtime_error("'projects' should be a map");
        for (auto &prj : prjs)
        {
            Project project(root_project);
            project.load(prj.second);
            set_project(project, prj.first.template as<String>());
        }
    }
    else
    {
        Project project(root_project);
        project.load(root);
        set_project(project, "");
    }
}

void remove_file(const path &p)
{
    boost::system::error_code ec;
    fs::remove(p, ec);
    if (ec)
        std::cerr << "Cannot remove file: " << p.string() << "\n";
}

void Config::clean_cmake_cache(path p) const
{
    if (p.empty())
        p = get_storage_dir_obj();

    // projects
    for (auto &fp : boost::make_iterator_range(fs::directory_iterator(p), {}))
    {
        if (!fs::is_directory(fp))
            continue;

        // versions
        for (auto &fv : boost::make_iterator_range(fs::directory_iterator(fp), {}))
        {
            if (!fs::is_directory(fv) || !fs::exists(fv / "build"))
                continue;

            // configs
            for (auto &fc : boost::make_iterator_range(fs::directory_iterator(fv / "build"), {}))
            {
                if (!fs::is_directory(fc))
                    continue;
                remove_file(fc / "CMakeCache.txt");
            }
        }
    }

    clean_cmake_exports(p);
}

void Config::clean_cmake_exports(path p) const
{
    if (p.empty())
        p = get_storage_dir_obj();

    // projects
    for (auto &fp : boost::make_iterator_range(fs::directory_iterator(p), {}))
    {
        if (!fs::is_directory(fp))
            continue;

        // versions
        for (auto &fv : boost::make_iterator_range(fs::directory_iterator(fp), {}))
        {
            if (!fs::is_directory(fv) || !fs::exists(fv / "build"))
                continue;

            if (!fs::is_directory(fv) || !fs::exists(fv / "build"))
                continue;

            // configs
            for (auto &fc : boost::make_iterator_range(fs::directory_iterator(fv / "build"), {}))
            {
                if (!fs::is_directory(fc))
                    continue;

                boost::system::error_code ec;
                fs::remove_all(fc / "exports", ec);
            }
        }
    }
}

void Config::clean_vars_cache(path p) const
{
    if (p.empty())
        p = get_storage_dir_cfg();

    for (auto &f : boost::make_iterator_range(fs::recursive_directory_iterator(p), {}))
    {
        if (!fs::is_regular_file(f))
            continue;
        remove_file(f);
    }
}

const Project &Config::getProject(const String &pname) const
{
    const Project *p = nullptr;
    if (projects.size() == 1)
        p = &projects.begin()->second;
    else if (!projects.empty())
    {
        auto it = projects.find(pname);
        if (it != projects.end())
            p = &it->second;
    }
    if (!p)
        throw std::runtime_error("No such project '" + pname + "' in dependencies list");
    return *p;
}

Project &Config::getDefaultProject()
{
    if (projects.empty())
        projects.emplace("", Project(root_project));
    return projects.begin()->second;
}

void Config::save(const path &p) const
{
#define EMIT_KV(k, v)          \
    do                         \
    {                          \
        e << YAML::Key << k;   \
        e << YAML::Value << v; \
    } while (0)
#define EMIT_KV_SAME(k) EMIT_KV(#k, k)

    std::ofstream o(p.string());
    if (!o)
        throw std::runtime_error("Cannot open file: " + p.string());
    YAML::Emitter e(o);
    e.SetIndent(4);
    e << YAML::BeginMap;
    EMIT_KV_SAME(host);
    EMIT_KV("storage_dir", storage_dir.string());
    e << YAML::EndMap;
}

void Config::process()
{
    download_dependencies();

    AccessTable at(get_storage_dir_etc());
    access_table = &at;

    print_configs();
    print_meta_config_file();
    print_include_guards_file();
    print_helper_file();

    // print inserted files
    access_table->write_if_older(fs::current_path() / CPPAN_LOCAL_DIR / cmake_functions_filename, cmake_functions);
    access_table->write_if_older(fs::current_path() / CPPAN_LOCAL_DIR / cpp_config_filename, cppan_h);

    fs::create_directories(get_storage_dir_cfg());

    access_table = nullptr;
}

void Config::download_dependencies()
{
    auto deps = getDependencies();
    if (deps.empty())
        return;

    if (!dependency_tree.empty())
        return extractDependencies(dependency_tree);

    // prepare request
    ptree data;
    for (auto &d : deps)
    {
        ptree version;
        version.put("version", d.second.version.toString());
        data.put_child(ptree::path_type(d.second.package.toString(), '|'), version);
    }

    LOG_NO_NEWLINE("Requesting dependency list... ");
    dependency_tree = url_post(host + "/api/find_dependencies", data);
    LOG("Ok");

    // read deps urls, download them, unpack
    int api = 0;
    if (dependency_tree.find("api") != dependency_tree.not_found())
        api = dependency_tree.get<int>("api");

    auto e = dependency_tree.find("error");
    if (e != dependency_tree.not_found())
        throw std::runtime_error(e->second.get_value<String>());

    if (api == 0)
        throw std::runtime_error("Api version is missing in the response");
    if (api != 1)
        throw std::runtime_error("Bad api version");

    String data_url = "data";
    if (dependency_tree.find("data_dir") != dependency_tree.not_found())
        data_url = dependency_tree.get<String>("data_dir");

    extractDependencies(dependency_tree);
    download_and_unpack(data_url);
}

void Config::download_and_unpack(const String &data_url) const
{
    for (auto &dd : dependencies)
    {
        auto &d = dd.second;
        auto version_dir = d.getPackageDir(get_storage_dir_src());
        auto md5_filename = get_stamp_filename(d.version.toString());
        auto md5file = version_dir.parent_path() / md5_filename;

        // store md5 of archive
        bool must_download = false;
        {
            std::ifstream ifile(md5file.string());
            String file_md5;
            if (ifile)
            {
                ifile >> file_md5;
                ifile.close();
            }
            if (file_md5 != d.md5 || d.md5.empty() || file_md5.empty())
                must_download = true;
        }

        if (!fs::exists(version_dir) || must_download)
        {
            if (fs::exists(version_dir))
                fs::remove_all(version_dir);

            auto fs_path = ProjectPath(d.package).toFileSystemPath().string();
            std::replace(fs_path.begin(), fs_path.end(), '\\', '/');
            String package_url = host + "/" + data_url + "/" + fs_path + "/" + d.version.toString() + ".tar.gz";
            path fn = version_dir.string() + ".tar.gz";

            String dl_md5;
            DownloadData dd;
            dd.url = package_url;
            dd.fn = fn;
            dd.dl_md5 = &dl_md5;
            LOG_NO_NEWLINE("Downloading: " << d.package.toString() << "-" << d.version.toString() << "... ");
            download_file(dd);

            if (dl_md5 != d.md5)
            {
                LOG("Fail");
                throw std::runtime_error("md5 does not match for package '" + d.package.toString() + "'");
            }
            LOG("Ok");

            write_file(md5file, d.md5);

            LOG_NO_NEWLINE("Unpacking: " << fn.string() << "... ");
            Files files;
            try
            {
                files = unpack_file(fn, version_dir);
            }
            catch (...)
            {
                fs::remove_all(version_dir);
                throw;
            }
            fs::remove(fn);
            LOG("Ok");

            prepare_exports(files, d);

            // FIXME: clear only related data instead all
            AccessTable at(get_storage_dir_etc());
            at.clear();
        }
    }
}

void Config::extractDependencies(const ptree &dependency_tree)
{
    dependencies.clear();

    auto &remote_packages = dependency_tree.get_child("packages");
    for (auto &v : remote_packages)
    {
        auto id = v.second.get<int>("id");

        DownloadDependency dep;
        dep.package = v.first;
        dep.version = v.second.get<String>("version");
        dep.flags = decltype(dep.flags)(v.second.get<uint64_t>("flags"));
        dep.md5 = v.second.get<String>("md5");

        if (v.second.find(DEPENDENCIES_NODE) != v.second.not_found())
        {
            std::set<int> idx;
            for (auto &tree_dep : v.second.get_child(DEPENDENCIES_NODE))
                idx.insert(tree_dep.second.get_value<int>());
            dep.setDependencyIds(idx);
        }

        dep.map_ptr = &dependencies;
        dependencies[id] = dep;
    }

    if (internal_options.current_package.empty())
        return;

    // remove current package and unneeded deps
    for (auto &dd : dependencies)
    {
        auto &d = dd.second;
        if (internal_options.current_package.package == d.package &&
            internal_options.current_package.version == d.version)
        {
            dependencies = d.getDependencies();
            for (auto &dd : dependencies)
                dd.second.map_ptr = &dependencies;
            break;
        }
    }
}

void Config::prepare_build(path fn, const String &cppan)
{
    fn = fs::canonical(fs::absolute(fn));

    build_settings.filename = fn.filename().string();
    build_settings.filename_without_ext = fn.filename().stem().string();
    if (build_settings.filename == CPPAN_FILENAME)
    {
        build_settings.is_dir = true;
        build_settings.filename = fn.parent_path().filename().string();
        build_settings.filename_without_ext = build_settings.filename;
    }

    build_settings.source_directory = get_build_dir(build_dir_type);
    if (build_dir_type == PackagesDirType::Local || build_dir_type == PackagesDirType::None)
        build_settings.source_directory /= (cppan_local_build_prefix + build_settings.filename);
    else
        build_settings.source_directory /= sha1(normalize_path(fn.string())).substr(0, 10);
    build_settings.binary_directory = build_settings.source_directory / "build";
    if (build_settings.rebuild)
    {
        boost::system::error_code ec;
        fs::remove_all(build_settings.source_directory, ec);
    }
    fs::create_directories(build_settings.source_directory);

    auto &p = getDefaultProject();
    if (!build_settings.is_dir)
        p.sources.insert(build_settings.filename);
    p.findSources(fn.parent_path());
    p.files.erase(CPPAN_FILENAME);

    write_file_if_different(build_settings.source_directory / CPPAN_FILENAME, cppan);
    Config conf(build_settings.source_directory);
    auto old = fs::current_path();
    fs::current_path(build_settings.source_directory);
    conf.process(); // invoke cppan
    fs::current_path(old);

    Context ctx;
    config_section_title(ctx, "cmake settings");
    ctx.addLine(cmake_minimum_required);
    ctx.addLine();

    config_section_title(ctx, "project settings");
    ctx.addLine("project(" + build_settings.filename_without_ext + " C CXX)");
    ctx.addLine();

    config_section_title(ctx, "compiler & linker settings");
    ctx.addLine(R"(# Output directory settings
set(output_dir ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${output_dir})

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if (MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
endif()
)");

    // compiler flags
    ctx.addLine("set(CMAKE_C_FLAGS \"${CMAKE_C_FLAGS} " + build_settings.c_compiler_flags + "\")");
    ctx.addLine("set(CMAKE_CXX_FLAGS \"${CMAKE_CXX_FLAGS} " + build_settings.cxx_compiler_flags + "\")");
    ctx.addLine();

    for (int i = 0; i < BuildSettings::CMakeConfigurationType::Max; i++)
    {
        auto &cfg = cmake_configuration_types[i];
        ctx.addLine("set(CMAKE_C_FLAGS_" + cfg + " \"${CMAKE_C_FLAGS_" + cfg + "} " + build_settings.c_compiler_flags_conf[i] + "\")");
        ctx.addLine("set(CMAKE_CXX_FLAGS_" + cfg + " \"${CMAKE_CXX_FLAGS_" + cfg + "} " + build_settings.cxx_compiler_flags_conf[i] + "\")");
        ctx.addLine();
    }

    // linker flags
    ctx.addLine("set(CMAKE_EXE_LINKER_FLAGS \"${CMAKE_EXE_LINKER_FLAGS} " + build_settings.link_flags + "\")");
    ctx.addLine("set(CMAKE_MODULE_LINKER_FLAGS \"${CMAKE_MODULE_LINKER_FLAGS} " + build_settings.link_flags + "\")");
    ctx.addLine("set(CMAKE_SHARED_LINKER_FLAGS \"${CMAKE_SHARED_LINKER_FLAGS} " + build_settings.link_flags + "\")");
    ctx.addLine("set(CMAKE_STATIC_LINKER_FLAGS \"${CMAKE_STATIC_LINKER_FLAGS} " + build_settings.link_flags + "\")");
    ctx.addLine();

    for (int i = 0; i < BuildSettings::CMakeConfigurationType::Max; i++)
    {
        auto &cfg = cmake_configuration_types[i];
        ctx.addLine("set(CMAKE_EXE_LINKER_FLAGS_" + cfg + " \"${CMAKE_EXE_LINKER_FLAGS_" + cfg + "} " + build_settings.link_flags_conf[i] + "\")");
        ctx.addLine("set(CMAKE_MODULE_LINKER_FLAGS_" + cfg + " \"${CMAKE_MODULE_LINKER_FLAGS_" + cfg + "} " + build_settings.link_flags_conf[i] + "\")");
        ctx.addLine("set(CMAKE_SHARED_LINKER_FLAGS_" + cfg + " \"${CMAKE_SHARED_LINKER_FLAGS_" + cfg + "} " + build_settings.link_flags_conf[i] + "\")");
        ctx.addLine("set(CMAKE_STATIC_LINKER_FLAGS_" + cfg + " \"${CMAKE_STATIC_LINKER_FLAGS_" + cfg + "} " + build_settings.link_flags_conf[i] + "\")");
        ctx.addLine();
    }

    // should be after flags
    config_section_title(ctx, "CPPAN include");
    ctx.addLine("set(CPPAN_BUILD_OUTPUT_DIR \"" + normalize_path(fs::current_path()) + "\")");
    if (build_settings.use_shared_libs)
        ctx.addLine("set(CPPAN_BUILD_SHARED_LIBS 1)");
    ctx.addLine("add_subdirectory(cppan)");
    ctx.addLine();

    // add GLOB later
    config_section_title(ctx, "sources");
    ctx.addLine("set(src");
    ctx.increaseIndent();
    for (auto &s : p.files)
        ctx.addLine("\"" + normalize_path(fn.parent_path() / s) + "\"");
    ctx.decreaseIndent();
    ctx.addLine(")");
    ctx.addLine();

    config_section_title(ctx, "target");
    ctx.addLine("set(this " + build_settings.filename_without_ext + ")");
    if (build_settings.type == "executable")
    {
        ctx.addLine("add_executable(${this} " + boost::to_upper_copy(build_settings.executable_type) + " ${src})");
        ctx.addLine("target_compile_definitions(${this} PRIVATE CPPAN_EXPORT=)");
    }
    else
    {
        if (build_settings.type == "library")
        {
            ctx.addLine("add_library(${this} " + boost::to_upper_copy(build_settings.library_type) + " ${src})");
        }
        else
        {
            ctx.addLine("add_library(${this} " + boost::to_upper_copy(build_settings.type) + " ${src})");
        }
        ctx.addLine("target_compile_definitions(${this} PRIVATE CPPAN_EXPORT=CPPAN_SYMBOL_EXPORT)");
        ctx.addLine(R"(set_target_properties(${this} PROPERTIES
    INSTALL_RPATH .
    BUILD_WITH_INSTALL_RPATH True
))");
    }
    ctx.addLine("target_link_libraries(${this} cppan " + build_settings.link_libraries + ")");
    ctx.addLine();
    ctx.addLine(R"(add_custom_command(TARGET ${this} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${this}> )" + normalize_path(fs::current_path()) + R"(/
))");
    ctx.addLine();

    // eof
    ctx.addLine(config_delimeter);
    ctx.addLine();
    ctx.splitLines();

    write_file_if_different(build_settings.source_directory / cmake_config_filename, ctx.getText());
}

int Config::generate() const
{
    std::vector<String> args;
    args.push_back("cmake");
    args.push_back("-H\"" + normalize_path(build_settings.source_directory) + "\"");
    args.push_back("-B\"" + normalize_path(build_settings.binary_directory) + "\"");
    if (!build_settings.c_compiler.empty())
        args.push_back("-DCMAKE_C_COMPILER=\"" + build_settings.c_compiler + "\"");
    if (!build_settings.cxx_compiler.empty())
        args.push_back("-DCMAKE_CXX_COMPILER=\"" + build_settings.cxx_compiler + "\"");
    if (!build_settings.generator.empty())
        args.push_back("-G \"" + build_settings.generator + "\"");
    if (!build_settings.toolset.empty())
        args.push_back("-T " + build_settings.toolset + "");
    args.push_back("-DCMAKE_BUILD_TYPE=" + build_settings.configuration + "");
    for (auto &o : build_settings.cmake_options)
        args.push_back(o);
    for (auto &o : build_settings.env)
    {
#ifdef _WIN32
        _putenv_s(o.first.c_str(), o.second.c_str());
#else
        setenv(o.first.c_str(), o.second.c_str(), 1);
#endif
    }
    auto ret = system(args);
    if (!build_settings.silent)
    {
        auto bld_dir = get_build_dir(PackagesDirType::Local);
#ifdef _WIN32
        auto sln = build_settings.binary_directory / (build_settings.filename_without_ext + ".sln");
        auto sln_new = bld_dir / (build_settings.filename_without_ext + ".sln.lnk");
        if (fs::exists(sln))
            CreateLink(sln.string().c_str(), sln_new.string().c_str(), "Link to CPPAN Solution");
#else
        bld_dir /= path(cppan_local_build_prefix + build_settings.filename);
        fs::create_directories(bld_dir);
        boost::system::error_code ec;
        fs::create_symlink(build_settings.source_directory / cmake_config_filename, bld_dir / cmake_config_filename, ec);
#endif
    }
    return ret;
}

int Config::build() const
{
    std::vector<String> args;
    args.push_back("cmake");
    args.push_back("--build \"" + normalize_path(build_settings.binary_directory) + "\"");
    args.push_back("--config " + build_settings.configuration);
    return system(args);
}

void Config::print_configs()
{
    LOG_NO_NEWLINE("Generating build configs... ");
    for (auto &dd : dependencies)
    {
        auto &d = dd.second;
        auto version_dir = d.getPackageDir(get_storage_dir_src());

        auto c = getConfig(d, get_storage_dir_src());

        // steps that should be performed even if printed
        String include_guard;
        {
            PackageInfo pi(d);
            include_guard = include_guard_prefix + pi.variable_name;
            include_guards.insert(include_guard);
        }

        if (c->printed)
            continue;
        c->printed = true;

        // gather checks, options etc.
        {
#define GATHER_CHECK(array) array.insert(c->array.begin(), c->array.end())
            GATHER_CHECK(check_functions);
            GATHER_CHECK(check_includes);
            GATHER_CHECK(check_types);
            GATHER_CHECK(check_symbols);
            GATHER_CHECK(check_libraries);
#undef GATHER_CHECK

            const auto &p = getProject(d.package.toString());
            for (auto &ol : p.options)
            {
                if (!ol.second.global_definitions.empty())
                    global_options[ol.first].global_definitions.insert(ol.second.global_definitions.begin(), ol.second.global_definitions.end());
            }
        }

        c->print_package_config_file(version_dir / cmake_config_filename, d, *this);
        c->print_package_actions_file(version_dir, d);
        c->print_package_include_file(version_dir, d, include_guard);

        if (d.flags[pfHeaderOnly] || local_build)
            continue;

        // print object config files for non-local building
        auto obj_dir = d.getPackageDirHash(get_storage_dir_obj());
        auto bld_dir = obj_dir;
        boost::system::error_code ec;
        fs::create_directories(bld_dir, ec);
        c->print_object_config_file(bld_dir / cmake_config_filename, d, *this);
        c->print_object_include_config_file(obj_dir / cmake_object_config_filename, d);
        c->print_object_export_file(obj_dir, d);
        c->print_object_build_file(obj_dir, d);
    }
    LOG("Ok");
}

void Config::print_bs_insertion(Context &ctx, const Project &p, const String &name, const String BuildSystemConfigInsertions::*i) const
{
    config_section_title(ctx, name);
    if (projects.size() > 1)
    {
        ctx.addLine(bs_insertions.*i);
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

void Config::print_package_config_file(const path &config_file, const DownloadDependency &d, const Config &parent) const
{
    auto fn = config_file;
    if (!access_table->must_update_contents(config_file))
        return;

    PackageInfo pi(d);
    bool header_only = pi.dependency->flags[pfHeaderOnly];
    const auto &p = getProject(d.package.toString());
    auto dd = d.getDirectDependenciesFixed(p);
    
    Context ctx;
    ctx.addLine("#");
    ctx.addLine("# cppan");
    ctx.addLine("# package: " + d.package.toString());
    ctx.addLine("# version: " + d.version.toString());
    ctx.addLine("#");
    ctx.addLine();

    // includes
    {
        if (parent.local_build)
            print_dependencies(ctx, parent, dd, d.getIndirectDependencies());
        else
            print_dependencies(ctx, parent, dd, d.getIndirectDependencies(), true);
    }

    // settings
    {
        config_section_title(ctx, "settings");
        ctx.addLine("set(PACKAGE_NAME " + d.package.toString() + ")");
        ctx.addLine("set(PACKAGE_VERSION " + d.version.toString() + ")");
        ctx.addLine();
        ctx.addLine("set(LIBRARY_TYPE STATIC)");
        ctx.addLine();
        ctx.addLine("if (CPPAN_BUILD_SHARED_LIBS)");
        ctx.increaseIndent();
        ctx.addLine("set(LIBRARY_TYPE SHARED)");
        ctx.decreaseIndent();
        ctx.addLine("endif()");
        ctx.addLine();
        ctx.addLine("if (LIBRARY_TYPE_" + pi.variable_name + ")");
        ctx.increaseIndent();
        ctx.addLine("set(LIBRARY_TYPE ${LIBRARY_TYPE_" + pi.variable_name + "})");
        ctx.decreaseIndent();
        ctx.addLine("endif()");

        if (p.static_only)
            ctx.addLine("set(LIBRARY_TYPE STATIC)");
        else if (p.shared_only)
            ctx.addLine("set(LIBRARY_TYPE SHARED)");

        ctx.emptyLines(1);
    }
    
    print_bs_insertion(ctx, p, "pre sources", &BuildSystemConfigInsertions::pre_sources);

    // sources (also used for headers)
    config_section_title(ctx, "sources");
    if (p.build_files.empty())
        ctx.addLine("file(GLOB_RECURSE src \"*\")");
    else
    {
        ctx.addLine("set(src");
        ctx.increaseIndent();
        for (auto &f : p.build_files)
        {
            auto s = f;
            std::replace(s.begin(), s.end(), '\\', '/');
            ctx.addLine("${CMAKE_CURRENT_SOURCE_DIR}/" + s);
        }
        ctx.decreaseIndent();
        ctx.addLine(")");
    }
    ctx.addLine();

    // exclude files
    if (!p.exclude_from_build.empty())
    {
        config_section_title(ctx, "exclude files");
        for (auto &f : p.exclude_from_build)
            ctx << "list(REMOVE_ITEM src \"${CMAKE_CURRENT_SOURCE_DIR}/" << f.string() << "\")" << Context::eol;
        ctx.emptyLines(1);
    }

    print_bs_insertion(ctx, p, "post sources", &BuildSystemConfigInsertions::post_sources);

    for (auto &ol : p.options)
        for (auto &ll : ol.second.link_directories)
            ctx.addLine("link_directories(" + ll + ")");
    ctx.emptyLines(1);

    // target
    config_section_title(ctx, "target: " + pi.target_name);
    if (d.flags[pfExecutable])
    {
        ctx << "add_executable                (" << pi.target_name << " ${src})" << Context::eol;
    }
    else
    {
        if (header_only)
            ctx << "add_library                   (" << pi.target_name << " INTERFACE)" << Context::eol;
        else
            ctx << "add_library                   (" << pi.target_name << " ${LIBRARY_TYPE} ${src})" << Context::eol;
    }
    ctx.addLine();

    // local aliases
    ctx.addLine("set(target " + pi.target_name + ")");
    ctx.addLine("set(this " + pi.target_name + ")");
    ctx.addLine();

    //if (d.flags[pfExecutable])
    //{
        //ctx.addLine("add_dependencies(${this} " + cppan_dummy_target + ")");
    //}

    // include directories
    {
        std::vector<Dependency> include_deps;
        for (auto &d : dd)
        {
            if (d.second.flags[pfIncludeDirectories])
                include_deps.push_back(d.second);
        }
        if (!p.include_directories.empty() || !include_deps.empty())
        {
            auto get_i_dir = [](const String &i)
            {
                return i;
                if (i.find("${CMAKE_CURRENT_SOURCE_DIR}") != i.npos ||
                    i.find("${CMAKE_CURRENT_BINARY_DIR}") != i.npos)
                    return i;
                return "${CMAKE_CURRENT_SOURCE_DIR}/" + i;
            };

            ctx << "target_include_directories    (" << pi.target_name << Context::eol;
            ctx.increaseIndent();
            if (header_only)
            {
                for (auto &idir : p.include_directories.public_)
                    ctx.addLine("INTERFACE " + get_i_dir(idir.string()));
                for (auto &idir : include_deps)
                {
                    auto c = getConfig(idir, get_storage_dir_src());
                    auto &proj = getProject(idir.package.toString());
                    for (auto &i : proj.include_directories.public_)
                    {
                        auto ipath = c->dir / i;
                        boost::system::error_code ec;
                        if (fs::exists(ipath, ec))
                            ctx.addLine("INTERFACE " + normalize_path(ipath));
                    }
                }
            }
            else
            {
                for (auto &idir : p.include_directories.public_)
                    ctx.addLine("PUBLIC " + get_i_dir(idir.string()));
                for (auto &idir : p.include_directories.private_)
                    ctx.addLine("PRIVATE " + get_i_dir(idir.string()));
                for (auto &idir : include_deps)
                {
                    auto c = getConfig(idir, get_storage_dir_src());
                    auto &proj = getProject(idir.package.toString());
                    for (auto &i : proj.include_directories.public_)
                    {
                        auto ipath = c->dir / i;
                        boost::system::error_code ec;
                        if (fs::exists(ipath, ec))
                            ctx.addLine("PUBLIC " + normalize_path(ipath));
                    }
                    // no privates here
                }
            }
            ctx.decreaseIndent();
            ctx.addLine(")");
        }
    }

    // deps (direct)
    ctx.addLine("target_link_libraries         (" + pi.target_name);
    ctx.increaseIndent();
    ctx.addLine((!header_only ? "PUBLIC" : "INTERFACE") + String(" ") + cppan_helpers_target);
    if (!header_only)
        ctx.addLine("PRIVATE" + String(" ") + cppan_helpers_private_target);
    for (auto &d1 : dd)
    {
        if (d1.second.flags[pfExecutable] ||
            d1.second.flags[pfIncludeDirectories])
            continue;
        PackageInfo pi1(d1.second);
        if (header_only)
            ctx.addLine("INTERFACE " + pi1.target_name);
        else
        {
            if (d1.second.flags[pfPrivate])
                ctx.addLine("PRIVATE " + pi1.target_name);
            else
                ctx.addLine("PUBLIC " + pi1.target_name);
        }
    }
    ctx.decreaseIndent();
    ctx.addLine(")");
    ctx.addLine();
    ctx.addLine("if (NOT CPPAN_LOCAL_BUILD AND CMAKE_GENERATOR STREQUAL Ninja)");
    ctx.addLine("target_link_libraries         (" + pi.target_name + " PRIVATE cppan-dummy)");
    ctx.addLine("endif()");
    ctx.addLine();

    // solution folder
    if (!header_only)
    {
        ctx << "set_target_properties         (" << pi.target_name << " PROPERTIES" << Context::eol;
        ctx << "    FOLDER \"" + packages_folder + "/" << d.package.toString() << "/" << d.version.toString() << "\"" << Context::eol;
        ctx << ")" << Context::eol;
        ctx.emptyLines(1);
    }

    // options (defs etc.)
    {
        if (!header_only)
        {
            // pkg
            ctx.addLine("target_compile_definitions    (" + pi.target_name);
            ctx.increaseIndent();
            ctx.addLine("PRIVATE   PACKAGE=\"" + d.package.toString() + "\"");
            ctx.addLine("PRIVATE   PACKAGE_NAME=\"" + d.package.toString() + "\"");
            ctx.addLine("PRIVATE   PACKAGE_VERSION=\"" + d.version.toString() + "\"");
            ctx.addLine("PRIVATE   PACKAGE_STRING=\"" + pi.target_name + "\"");
            ctx.decreaseIndent();
            ctx.addLine(")");
        }

        // export/import
        ctx.addLine("if (LIBRARY_TYPE STREQUAL \"SHARED\")");
        ctx.increaseIndent();
        ctx.addLine("target_compile_definitions    (" + pi.target_name);
        ctx.increaseIndent();
        if (!header_only)
        {
            ctx.addLine("PRIVATE   " + cppan_export_prefix + pi.variable_name + (d.flags[pfExecutable] ? "" : "=CPPAN_SYMBOL_EXPORT"));
            ctx.addLine("INTERFACE " + cppan_export_prefix + pi.variable_name + (d.flags[pfExecutable] ? "" : "=CPPAN_SYMBOL_IMPORT"));
        }
        else
            ctx.addLine("INTERFACE " + cppan_export_prefix + pi.variable_name + (d.flags[pfExecutable] ? "" : "="));
        ctx.decreaseIndent();
        ctx.addLine(")");
        ctx.decreaseIndent();
        ctx.addLine("else()");
        ctx.increaseIndent();
        ctx.addLine("target_compile_definitions    (" + pi.target_name);
        ctx.increaseIndent();
        if (!header_only)
            ctx.addLine("PUBLIC    " + cppan_export_prefix + pi.variable_name + "=");
        else
            ctx.addLine("INTERFACE    " + cppan_export_prefix + pi.variable_name + "=");
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

        /*if (d.flags[pfExecutable])
        {
            ctx.addLine("if (MSVC OR XCODE)");
            ctx.increaseIndent();
            for (auto &c : {"Debug","MinSizeRel","RelWithDebInfo"})
            {
                ctx.addLine("add_custom_command(TARGET ${this} POST_BUILD");
                ctx.increaseIndent();
                ctx.addLine("COMMAND ${CMAKE_COMMAND} -E copy_if_different");
                ctx.increaseIndent();
                ctx.addLine("$<TARGET_FILE:${this}>");
                ctx.addLine("$<TARGET_FILE_DIR:${this}>/../" + String(c) + "/$<TARGET_FILE_NAME:${this}>");
                ctx.decreaseIndent();
                ctx.decreaseIndent();
                ctx.addLine(")");
                ctx.addLine();
            }
            ctx.decreaseIndent();
            ctx.addLine("endif()");
            ctx.addLine();
        }*/

        for (auto &ol : p.options)
        {
            ctx.emptyLines(1);

            auto print_defs = [header_only, &ctx, &pi](const auto &ol)
            {
                if (ol.second.definitions.empty())
                    return;
                ctx << "target_compile_definitions    (" << pi.target_name << Context::eol;
                ctx.increaseIndent();
                for (auto &def : ol.second.definitions)
                {
                    if (header_only)
                        ctx << "INTERFACE " << def.second << Context::eol;
                    else
                        ctx << boost::algorithm::to_upper_copy(def.first) << " " << def.second << Context::eol;
                }
                ctx.decreaseIndent();
                ctx.addLine(")");
            };
            auto print_set = [header_only, &ctx, &pi](const auto &a, const auto &s)
            {
                if (a.empty())
                    return;
                ctx << s << "(" << pi.target_name << Context::eol;
                ctx.increaseIndent();
                for (auto &def : a)
                {
                    if (header_only)
                        ctx << "INTERFACE ";
                    else
                        ctx << "PUBLIC ";
                    ctx << def << Context::eol;
                }
                ctx.decreaseIndent();
                ctx.addLine(")");
                ctx.addLine();
            };
            auto print_options = [&ol, &print_defs, &print_set]
            {
                print_defs(ol);
                print_set(ol.second.include_directories, "target_include_directories");
                print_set(ol.second.link_libraries, "target_link_libraries");
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
    if (!pi.dependency->version.isBranch())
    {
        String tt = d.flags[pfExecutable] ? "add_executable" : "add_library";

        config_section_title(ctx, "aliases");

        {
            Version ver = pi.dependency->version;
            ver.patch = -1;
            ctx << tt << "(" << pi.dependency->package.toString() + "-" + ver.toAnyVersion() << " ALIAS " << pi.target_name << ")" << Context::eol;
            ver.minor = -1;
            ctx << tt << "(" << pi.dependency->package.toString() + "-" + ver.toAnyVersion() << " ALIAS " << pi.target_name << ")" << Context::eol;
            ctx << tt << "(" << pi.dependency->package.toString() << " ALIAS " << pi.target_name << ")" << Context::eol;
            ctx.addLine();
        }

        {
            Version ver = pi.dependency->version;
            ctx << tt << "(" << pi.dependency->package.toString("::") + "-" + ver.toAnyVersion() << " ALIAS " << pi.target_name << ")" << Context::eol;
            ver.patch = -1;
            ctx << tt << "(" << pi.dependency->package.toString("::") + "-" + ver.toAnyVersion() << " ALIAS " << pi.target_name << ")" << Context::eol;
            ver.minor = -1;
            ctx << tt << "(" << pi.dependency->package.toString("::") + "-" + ver.toAnyVersion() << " ALIAS " << pi.target_name << ")" << Context::eol;
            ctx << tt << "(" << pi.dependency->package.toString("::") << " ALIAS " << pi.target_name << ")" << Context::eol;
            ctx.addLine();
        }

        if (!p.aliases.empty())
        {
            ctx.addLine("# user-defined");
            for (auto &a : p.aliases)
                ctx << tt << "(" << a << " ALIAS " << pi.target_name << ")" << Context::eol;
            ctx.addLine();
        }
    }

    // export
    config_section_title(ctx, "export");
    ctx.addLine("export(TARGETS " + pi.target_name + " FILE " + exports_dir + pi.variable_name + ".cmake)");

    ctx.emptyLines(1);

    print_bs_insertion(ctx, p, "post alias", &BuildSystemConfigInsertions::post_alias);

    // dummy target for IDEs with headers only
    if (header_only)
    {
        config_section_title(ctx, "IDE dummy target for headers");

        auto tgt = pi.target_name + "-headers";
        ctx.addLine("if (CPPAN_SHOW_IDE_PROJECTS)");
        ctx.addLine("add_custom_target(" + tgt + " SOURCES ${src})");
        ctx.addLine();
        ctx << "set_target_properties         (" << tgt << " PROPERTIES" << Context::eol;
        ctx << "    FOLDER \"" + packages_folder + "/" << d.package.toString() << "/" << d.version.toString() << "\"" << Context::eol;
        ctx << ")" << Context::eol;
        ctx.addLine("endif()");
        ctx.emptyLines(1);
    }

    // source groups
    print_source_groups(ctx, config_file.parent_path());

    // eof
    ctx.addLine(config_delimeter);
    ctx.addLine();
    ctx.splitLines();

    access_table->write_if_older(fn, ctx.getText());
}

void Config::print_package_actions_file(const path &config_dir, const DownloadDependency &d) const
{
    auto fn = config_dir / actions_filename;
    if (!access_table->must_update_contents(fn))
        return;

    const auto &p = getProject(d.package.toString());
    Context ctx;
    ctx.addLine(config_delimeter);
    ctx.addLine();
    ctx.addLine("set(CMAKE_CURRENT_SOURCE_DIR_OLD ${CMAKE_CURRENT_SOURCE_DIR})");
    ctx.addLine("set(CMAKE_CURRENT_SOURCE_DIR \"" + normalize_path(config_dir.string()) + "\")");
    print_bs_insertion(ctx, p, "pre sources", &BuildSystemConfigInsertions::pre_sources);
    ctx.addLine("file(GLOB_RECURSE src \"*\")");
    print_bs_insertion(ctx, p, "post sources", &BuildSystemConfigInsertions::post_sources);
    print_bs_insertion(ctx, p, "post target", &BuildSystemConfigInsertions::post_target);
    print_bs_insertion(ctx, p, "post alias", &BuildSystemConfigInsertions::post_alias);
    ctx.addLine("set(CMAKE_CURRENT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR_OLD})");
    ctx.addLine();
    ctx.addLine(config_delimeter);
    ctx.addLine();
    access_table->write_if_older(fn, ctx.getText());
}

void Config::print_package_include_file(const path &config_dir, const DownloadDependency &d, const String &ig) const
{
    auto fn = config_dir / include_guard_filename;
    if (!access_table->must_update_contents(fn))
        return;

    PackageInfo pi(d);
    Context ctx;

    ctx.addLine("#");
    ctx.addLine("# cppan");
    ctx.addLine("# package: " + d.package.toString());
    ctx.addLine("# version: " + d.version.toString());
    ctx.addLine("#");
    ctx.addLine();

    ctx.addLine("if (" + ig + ")");
    ctx.addLine("    return()");
    ctx.addLine("endif()");
    ctx.addLine();
    ctx.addLine("set(" + ig + " 1 CACHE BOOL \"\" FORCE)");
    ctx.addLine();
    ctx.addLine("add_subdirectory(\"" + normalize_path(config_dir.string()) + "\" \"" + get_binary_path(d.package, d.version) + "\")");
    ctx.addLine();

    access_table->write_if_older(fn, ctx.getText());
}

void Config::print_object_config_file(const path &config_file, const DownloadDependency &d, const Config &parent) const
{
    auto fn = config_file;
    if (!access_table->must_update_contents(config_file))
        return;

    auto src_dir = d.getPackageDir(get_storage_dir_src());
    auto obj_dir = d.getPackageDirHash(get_storage_dir_obj());

    PackageInfo pi(d);

    Context ctx;
    ctx.addLine("#");
    ctx.addLine("# cppan");
    ctx.addLine("# package: " + d.package.toString());
    ctx.addLine("# version: " + d.version.toString());
    ctx.addLine("#");
    ctx.addLine();

    {
        config_section_title(ctx, "cmake settings");
        ctx.addLine(cmake_minimum_required);
        ctx.addLine();
        ctx.addLine("set(CMAKE_RUNTIME_OUTPUT_DIRECTORY " + normalize_path(get_storage_dir_bin()) + "/${OUTPUT_DIR})");
        ctx.addLine("set(CMAKE_LIBRARY_OUTPUT_DIRECTORY " + normalize_path(get_storage_dir_lib()) + "/${OUTPUT_DIR})");
        ctx.addLine("set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY " + normalize_path(get_storage_dir_lib()) + "/${OUTPUT_DIR})");
        ctx.addLine();

        /*if (d.flags[pfExecutable])
        {
            ctx.addLine("if (MSVC OR XCODE)");
            for (auto &c : cmake_configuration_types_no_rel)
                ctx.addLine("set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_" + c + " ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Release)");
            ctx.addLine("endif()");
            ctx.addLine();
        }*/
    }

    config_section_title(ctx, "project settings");
    ctx.addLine("project(" + pi.variable_name + " C CXX)");
    ctx.addLine();

    config_section_title(ctx, "compiler & linker settings");
    ctx.addLine(R"(if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if (MSVC)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")

    string(FIND "${OUTPUT_DIR}" "-mt" mt)
    if (NOT mt EQUAL -1)
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} /MT")
        set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS} /MT")
        set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS} /MT")
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} /MTd")
    endif()

    if (0)# OR CMAKE_GENERATOR STREQUAL Ninja)
        string(TOLOWER "${CMAKE_CXX_COMPILER}" inc)
        string(REGEX MATCH ".*/vc/bin" inc "${inc}")

        include_directories(BEFORE SYSTEM ${inc}/include)
        set(ENV{INCLUDE} ${inc}/include)

        set(lib)
        if (CMAKE_SYSTEM_PROCESSOR STREQUAL amd64)
            set(lib /${CMAKE_SYSTEM_PROCESSOR})
        endif()
        link_directories(${inc}/lib${lib})
        set(ENV{LIB} ${inc}/lib${lib})
    endif()
endif()
)");

    // recursive calls
    {
        config_section_title(ctx, "cppan setup");

        ctx.addLine("add_subdirectory(cppan)");
        fs::copy_file(src_dir / CPPAN_FILENAME, obj_dir / CPPAN_FILENAME, fs::copy_option::overwrite_if_exists);

        if (parent.internal_options.invocations.find(d) != parent.internal_options.invocations.end())
            throw std::runtime_error("Circular dependency detected. Project: " + pi.target_name);

        silent = true;
        auto old_dir = fs::current_path();
        fs::current_path(obj_dir);

        Config c(obj_dir);
        c.dependency_tree = parent.dependency_tree;
        c.internal_options.current_package = d;
        c.internal_options.invocations = parent.internal_options.invocations;
        c.internal_options.invocations.insert(d);
        c.disable_run_cppan_target = true;
        c.process();

        fs::current_path(old_dir);
        if (parent.internal_options.current_package.empty())
            silent = false;
    }

    // main include
    {
        config_section_title(ctx, "main include");
        auto mi = src_dir;
        add_subdirectory(ctx, mi.string(), get_binary_path(d.package, d.version));
        ctx.emptyLines(1);
        auto ig = include_guard_prefix + pi.variable_name;
        ctx.addLine("set(" + ig + " 0 CACHE BOOL \"\" FORCE)");
        ctx.emptyLines(1);
    }

    // eof
    ctx.addLine(config_delimeter);
    ctx.addLine();
    ctx.splitLines();

    access_table->write_if_older(fn, ctx.getText());
}

void Config::print_object_include_config_file(const path &config_file, const DownloadDependency &d) const
{
    auto fn = config_file;
    if (!access_table->must_update_contents(fn))
        return;

    const auto &p = getProject(d.package.toString());
    auto dd = d.getDirectDependenciesFixed(p);
    PackageInfo pi(d);

    Context ctx;
    ctx.addLine("#");
    ctx.addLine("# cppan");
    ctx.addLine("# package: " + d.package.toString());
    ctx.addLine("# version: " + d.version.toString());
    ctx.addLine("#");
    ctx.addLine();

    ctx.addLine("set(target " + pi.target_name + ")");
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
    ctx.addLine("set(current_dir " + normalize_path(config_file.parent_path()) + ")");
    if (!d.flags[pfExecutable])
        ctx.addLine("get_configuration(config)");
    else
        ctx.addLine("get_configuration_exe(config)");
    ctx.addLine("set(build_dir ${current_dir}/build/${config})");
    ctx.addLine("set(export_dir ${build_dir}/exports)");
    ctx.addLine("set(import ${export_dir}/" + pi.variable_name + ".cmake)");
    ctx.addLine("set(import_fixed ${export_dir}/" + pi.variable_name + "-fixed.cmake)");
    ctx.addLine("set(aliases_file ${export_dir}/" + pi.variable_name + "-aliases.cmake)");
    ctx.addLine();
    ctx.addLine(R"(if (NOT EXISTS ${import} OR NOT EXISTS ${import_fixed})
    set(lock ${build_dir}/generate.lock)

    file(LOCK ${lock} TIMEOUT 0 RESULT_VARIABLE lock_result)
    if (NOT ${lock_result} EQUAL 0)
        message(STATUS "WARNING: Target: ${target}")
        message(STATUS "WARNING: Other project is being bootstrapped right now or you hit a circular deadlock.")
        message(STATUS "WARNING: If you aren't building other projects right now feel free to kill this process or it will be stopped in 90 seconds.")

        file(LOCK ${lock} TIMEOUT 90 RESULT_VARIABLE lock_result)

        if (NOT ${lock_result} EQUAL 0)
            message(FATAL_ERROR "Lock error: ${lock_result}")
        endif()
    endif()

    # double check
    if (NOT EXISTS ${import} OR NOT EXISTS ${import_fixed})
        message(STATUS "")
        message(STATUS "Preparing build tree for ${target} with config ${config}")
        message(STATUS "")

        #find_program(ninja ninja)
        #set(generator Ninja)
        set(generator ${CMAKE_GENERATOR})
        if (MSVC
            OR "${ninja}" STREQUAL "ninja-NOTFOUND"
            OR CYGWIN # for me it's not working atm
        )
            set(generator ${CMAKE_GENERATOR})
        endif()
)");
    if (d.flags[pfExecutable])
    {
        ctx.addLine(R"(
            execute_process(
                COMMAND ${CMAKE_COMMAND}
                    -H${current_dir} -B${build_dir}
                    #-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
                    #-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
                    #-G "${generator}"
                    -DOUTPUT_DIR=${config}
                    -DCPPAN_BUILD_SHARED_LIBS=0 # TODO: try to work 0->1
            )
)");
    }
    else
    {
        ctx.addLine(R"(
        if (CMAKE_TOOLCHAIN_FILE)
            execute_process(
                COMMAND ${CMAKE_COMMAND}
                    -H${current_dir} -B${build_dir}
                    -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
                    -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
                    -G "${generator}"
                    -DOUTPUT_DIR=${config}
                    -DCPPAN_BUILD_SHARED_LIBS=${CPPAN_BUILD_SHARED_LIBS}
            )
        else()
            execute_process(
                COMMAND ${CMAKE_COMMAND}
                    -H${current_dir} -B${build_dir}
                    -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
                    -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
                    -G "${generator}"
                    -DOUTPUT_DIR=${config}
                    -DCPPAN_BUILD_SHARED_LIBS=${CPPAN_BUILD_SHARED_LIBS}
            )
        endif()
)");
    }
    ctx.addLine(R"(
        file(WRITE ${aliases_file} "${aliases}")
        execute_process(
            COMMAND cppan internal-fix-imports ${target} ${aliases_file} ${import} ${import_fixed}
        )
    endif()

    file(LOCK ${lock} RELEASE)
endif()
)");

    ctx.addLine("if (NOT TARGET " + pi.target_name + ")");
    ctx.addLine("     include(${import_fixed})");
    ctx.addLine("endif()");
    ctx.addLine();

    config_section_title(ctx, "import direct deps");
    ctx.addLine("include(${current_dir}/exports.cmake)");
    ctx.addLine();

    // src target
    {
        auto target = pi.target_name + "-sources";
        auto dir = d.getPackageDir(get_storage_dir_src());

        ctx.addLine("if (CPPAN_SHOW_IDE_PROJECTS)");
        ctx.addLine();
        config_section_title(ctx, "sources target (for IDE only)");
        ctx.addLine("if (NOT TARGET " + target + ")");
        ctx.increaseIndent();
        ctx.addLine("file(GLOB_RECURSE src \"" + normalize_path(dir) + "/*\")");
        ctx.addLine();
        ctx.addLine("add_custom_target(" + target);
        ctx.addLine("    SOURCES ${src}");
        ctx.addLine(")");
        ctx.addLine();

        // solution folder
        ctx << "set_target_properties         (" << target << " PROPERTIES" << Context::eol;
        ctx << "    FOLDER \"" + packages_folder + "/" << d.package.toString() << "/" << d.version.toString() << "\"" << Context::eol;
        ctx << ")" << Context::eol;
        ctx.decreaseIndent();
        ctx.addLine("endif()");
        ctx.emptyLines(1);

        // source groups
        print_source_groups(ctx, dir);
        ctx.addLine("endif(CPPAN_SHOW_IDE_PROJECTS)");
    }

    // eof
    ctx.emptyLines(1);
    ctx.addLine(config_delimeter);
    ctx.addLine();
    ctx.splitLines();

    access_table->write_if_older(fn, ctx.getText());
}

void Config::print_object_export_file(const path &config_dir, const DownloadDependency &d) const
{
    auto fn = config_dir / exports_filename;
    if (!access_table->must_update_contents(fn))
        return;

    auto dd = d.getDirectDependenciesFixed(getProject(d.package.toString()));
    PackageInfo pi(d);
    Context ctx;

    for (auto &dp : dd)
    {
        auto &dep = dp.second;
        PackageInfo pi(dep);

        if (dep.flags[pfIncludeDirectories])
            continue;

        auto b = dep.getPackageDirHash(get_storage_dir_obj());
        auto p = b / "build" / "${config}" / "exports" / (pi.variable_name + "-fixed.cmake");

        if (!dep.flags[pfHeaderOnly])
            ctx.addLine("include(\"" + normalize_path(b / exports_filename) + "\")");
        ctx.addLine("if (NOT TARGET " + pi.target_name + ")");
        ctx.increaseIndent();
        if (dep.flags[pfHeaderOnly])
            add_subdirectory(ctx, dep.getPackageDir(get_storage_dir_src()).string()); // was ctx
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

    access_table->write_if_older(fn, ctx.getText());
}

void Config::print_object_build_file(const path &config_dir, const DownloadDependency &d) const
{
    auto fn = config_dir / non_local_build_file;
    if (!access_table->must_update_contents(fn))
        return;

    auto dd = d.getDirectDependenciesFixed(getProject(d.package.toString()));
    PackageInfo pi(d);
    Context ctx;

    auto fn1 = normalize_path(get_storage_dir_src() / d.package.toString() / get_stamp_filename(d.version.toString()));
    ctx.addLine(R"(set(REBUILD 1)

set(fn1 ")" + fn1 + R"(")
set(fn2 "${BUILD_DIR}/cppan_sources.stamp")

file(READ ${fn1} f1)
if (EXISTS ${fn2})
    file(READ ${fn2} f2)
    if (f1 STREQUAL f2)
        set(REBUILD 0)
    endif()
endif()

if (NOT REBUILD AND EXISTS ${TARGET_FILE})
    return()
endif()

set(lock ${BUILD_DIR}/build.lock)

file(LOCK ${lock} RESULT_VARIABLE lock_result)
if (NOT ${lock_result} EQUAL 0)
    message(FATAL_ERROR "Lock error: ${lock_result}")
endif()

# double check
if (NOT REBUILD AND EXISTS ${TARGET_FILE})
    # release before exit
    file(LOCK ${lock} RELEASE)

    return()
endif()

execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${fn1} ${fn2})

if (CONFIG)
)");
    if (d.flags[pfExecutable])
    {
        ctx.addLine(R"(
    execute_process(
        COMMAND ${CMAKE_COMMAND}
            --build ${BUILD_DIR}
            --config ${CONFIG}#Release # FIXME: always build exe with Release conf
    ))");
    }
    else
    {
        ctx.addLine(R"(
    execute_process(
        COMMAND ${CMAKE_COMMAND}
            --build ${BUILD_DIR}
            --config ${CONFIG}
    ))");
    }
    ctx.addLine(R"(
else()
    find_program(make make)
    if (${make} STREQUAL "make-NOTFOUND")
        execute_process(
            COMMAND ${CMAKE_COMMAND}
                --build ${BUILD_DIR}
        )
    else()
        get_number_of_cores(N)
        execute_process(
            COMMAND make -j${N} -C ${BUILD_DIR}
        )
    endif()
endif()

file(LOCK ${lock} RELEASE)
)");

    access_table->write_if_older(fn, ctx.getText());
}

void Config::print_meta_config_file() const
{
    auto fn = fs::current_path() / CPPAN_LOCAL_DIR / cmake_config_filename;
    if (!access_table->must_update_contents(fn))
        return;

    Context ctx;
    ctx.addLine("#");
    ctx.addLine("# cppan");
    ctx.addLine("# meta config file");
    ctx.addLine("#");
    ctx.addLine();
    ctx.addLine(cmake_minimum_required);
    ctx.addLine();

    config_section_title(ctx, "variables");
    ctx.addLine("set(CPPAN_BUILD 1 CACHE STRING \"CPPAN is turned on\")");
    ctx.addLine();
    ctx.addLine("set(CPPAN_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})");
    ctx.addLine("set(CPPAN_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})");
    ctx.addLine();
    ctx.addLine("set(CMAKE_POSITION_INDEPENDENT_CODE ON)");
    ctx.addLine();
    ctx.addLine("set(${CMAKE_CXX_COMPILER_ID} 1)");
    ctx.addLine();
    ctx.addLine(String("set(CPPAN_LOCAL_BUILD ") + (local_build ? "1" : "0") + ")");
    ctx.addLine(String("set(CPPAN_SHOW_IDE_PROJECTS ") + (show_ide_projects ? "1" : "0") + ")");
    ctx.addLine();

    ctx.addLine("include(" + cmake_helpers_filename + ")");
    ctx.addLine();

    // deps
    if (local_build)
        print_dependencies(ctx, *this);
    else
        print_dependencies(ctx, *this, true);
    ctx.addLine("include(" + include_guard_filename + ")");

    // lib
    const String cppan_project_name = "cppan";
    config_section_title(ctx, "main library");
    ctx.addLine("add_library                   (" + cppan_project_name + " INTERFACE)");
    ctx.addLine("target_link_libraries         (" + cppan_project_name);
    ctx.increaseIndent();
    ctx.addLine("INTERFACE " + cppan_helpers_target);
    for (auto &p : getDirectDependencies())
    {
        if (p.second.flags[pfExecutable])
            continue;
        PackageInfo pi(p.second);
        ctx.addLine("INTERFACE " + pi.target_name);
    }
    ctx.decreaseIndent();
    ctx.addLine(")");
    ctx.addLine();
    ctx.addLine("export(TARGETS " + cppan_project_name + " FILE " + exports_dir + "cppan.cmake)");

    // exe deps
    if (!local_build)
    {
        config_section_title(ctx, "exe deps");

        auto dd = getDirectDependencies();
        if (!internal_options.current_package.empty())
            dd = internal_options.current_package.getDirectDependencies();

        for (auto &dp : dd)
        {
            auto &d = dp.second;
            PackageInfo pi(d);
            if (!d.flags[pfExecutable])
                continue;
            ctx.addLine("add_dependencies(" + pi.target_name + " " + cppan_project_name + ")");
        }
    }

    ctx.emptyLines(1);
    ctx.addLine(config_delimeter);
    ctx.addLine();

    access_table->write_if_older(fn, ctx.getText());
}

void Config::print_include_guards_file() const
{
    auto fn = fs::current_path() / CPPAN_LOCAL_DIR / include_guard_filename;
    if (!access_table->must_update_contents(fn))
        return;

    // turn off header guards
    Context ctx;
    for (auto &ig : include_guards)
        ctx.addLine("set(" + ig + " 0 CACHE BOOL \"\" FORCE)");
    access_table->write_if_older(fn, ctx.getText());
}

void Config::print_helper_file() const
{
    auto fn = fs::current_path() / CPPAN_LOCAL_DIR / cmake_helpers_filename;
    if (!access_table->must_update_contents(fn))
        return;

    Context ctx;
    ctx.addLine("#");
    ctx.addLine("# cppan");
    ctx.addLine("# helper routines");
    ctx.addLine("#");
    ctx.addLine();

    config_section_title(ctx, "cmake setup");
    ctx.addLine(R"(# Use solution folders.
set_property(GLOBAL PROPERTY USE_FOLDERS ON))");
    ctx.addLine();

    config_section_title(ctx, "macros & functions");
    ctx.addLine("include(" + cmake_functions_filename + ")");

    config_section_title(ctx, "variables");
    ctx.addLine("get_configuration(config)");
    ctx.addLine("#message(STATUS \"CPPAN config - ${config}\")");
    ctx.addLine();

    config_section_title(ctx, "export/import");
    ctx.addLine(R"str(if (MSVC)
    set(CPPAN_EXPORT "__declspec(dllexport)")
    set(CPPAN_IMPORT "__declspec(dllimport)")
endif()

if (MINGW)
    set(CPPAN_EXPORT "__attribute__((__dllexport__))")
    set(CPPAN_IMPORT "__attribute__((__dllimport__))")
elseif(GNU)
    set(CPPAN_EXPORT "__attribute__((__visibility__(\"default\")))")
    set(CPPAN_IMPORT)
endif()

if (SUN) # TODO: check it in real environment
    set(CPPAN_EXPORT "__global")
    set(CPPAN_IMPORT "__global")
endif())str");

    // cmake includes
    config_section_title(ctx, "cmake includes");
    ctx.addLine(R"(include(CheckCXXSymbolExists)
include(CheckFunctionExists)
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CheckTypeSize)
include(TestBigEndian))");
    ctx.addLine();

    config_section_title(ctx, "common checks");

    // read vars file
    ctx.addLine("set(vars_file \"" + normalize_path(get_storage_dir_cfg()) + "/${config}.cmake\")");
    ctx.addLine("read_variables_file(${vars_file})");
    ctx.addLine();

    ctx.addLine("if (NOT DEFINED WORDS_BIGENDIAN)");
    ctx.increaseIndent();
    ctx.addLine("test_big_endian(WORDS_BIGENDIAN)");
    ctx.addLine("add_variable(WORDS_BIGENDIAN)");
    ctx.decreaseIndent();
    ctx.addLine("endif()");
    // aliases
    ctx.addLine("set(BIG_ENDIAN ${WORDS_BIGENDIAN} CACHE STRING \"endianness alias\")");
    ctx.addLine("set(BIGENDIAN ${WORDS_BIGENDIAN} CACHE STRING \"endianness alias\")");
    ctx.addLine("set(HOST_BIG_ENDIAN ${WORDS_BIGENDIAN} CACHE STRING \"endianness alias\")");
    ctx.addLine();

    // checks
    config_section_title(ctx, "checks");

    auto convert_function = [](const auto &s)
    {
        return "HAVE_" + boost::algorithm::to_upper_copy(s);
    };
    auto convert_include = [](const auto &s)
    {
        auto v_def = "HAVE_" + boost::algorithm::to_upper_copy(s);
        for (auto &c : v_def)
        {
            if (!isalnum(c))
                c = '_';
        }
        return v_def;
    };
    auto convert_type = [](const auto &s, const std::string &prefix = "HAVE_")
    {
        String v_def = prefix;
        v_def += boost::algorithm::to_upper_copy(s);
        for (auto &c : v_def)
        {
            if (c == '*')
                c = 'P';
            else if (!isalnum(c))
                c = '_';
        }
        return v_def;
    };

    auto add_checks = [&ctx](const auto &a, const String &s, auto &&f)
    {
        for (auto &v : a)
        {
            auto val = f(v);
            ctx.addLine("if (NOT DEFINED " + val + ")");
            ctx.increaseIndent();
            ctx.addLine(s + "(\"" + v + "\" " + val + ")");
            ctx.addLine("add_variable(" + val + ")");
            ctx.decreaseIndent();
            ctx.addLine("endif()");
        }
        ctx.emptyLines(1);
    };
    auto add_symbol_checks = [&ctx](const auto &a, const String &s, auto &&f)
    {
        for (auto &v : a)
        {
            auto val = f(v.first);
            ctx.addLine("if (NOT DEFINED " + val + ")");
            ctx.increaseIndent();
            ctx << s + "(\"" + v.first + "\" \"";
            for (auto &h : v.second)
                ctx << h << ";";
            ctx << "\" " << val << ")" << Context::eol;
            ctx.addLine("add_variable(" + val + ")");
            ctx.decreaseIndent();
            ctx.addLine("endif()");
        }
        ctx.emptyLines(1);
    };
    auto add_if_definition = [&ctx](const String &s, auto &&... defs)
    {
        auto print_def = [&ctx](auto &&s)
        {
            ctx << "INTERFACE " << s << Context::eol;
            return 0;
        };
        ctx.addLine("if (" + s + ")");
        ctx.increaseIndent();
        ctx << "target_compile_definitions(" << cppan_helpers_target << Context::eol;
        ctx.increaseIndent();
        print_def(s);
        using expand_type = int[];
        expand_type{ 0, print_def(std::forward<decltype(defs)>(defs))... };
        ctx.decreaseIndent();
        ctx.addLine(")");
        ctx.decreaseIndent();
        ctx.addLine("endif()");
        ctx.addLine();
    };
    auto add_check_definitions = [&ctx, &add_if_definition](const auto &a, auto &&f)
    {
        for (auto &v : a)
            add_if_definition(f(v));
    };
    auto add_check_symbol_definitions = [&ctx, &add_if_definition](const auto &a, auto &&f)
    {
        for (auto &v : a)
            add_if_definition(f(v.first));
    };

    add_checks(check_functions, "check_function_exists", convert_function);
    add_symbol_checks(check_symbols, "check_cxx_symbol_exists", convert_function);
    add_checks(check_includes, "check_include_files", convert_include);
    add_checks(check_types, "check_type_size", convert_type);

    for (auto &v : check_types)
    {
        ctx.addLine("if (" + convert_type(v) + ")");
        ctx.increaseIndent();
        ctx.addLine("set(" + convert_type(v, "SIZE_OF_") + " ${" + convert_type(v) + "} CACHE STRING \"\")");
        ctx.addLine("set(" + convert_type(v, "SIZEOF_")  + " ${" + convert_type(v) + "} CACHE STRING \"\")");
        ctx.decreaseIndent();
        ctx.addLine("endif()");
        ctx.addLine();
    }

    // write vars file
    ctx.addLine("if (CPPAN_NEW_VARIABLE_ADDED)");
    ctx.addLine("    write_variables_file(${vars_file})");
    ctx.addLine("endif()");

    // fixups
    // put bug workarounds here
    //config_section_title(ctx, "fixups");
    ctx.emptyLines(1);

    // dummy (compiled?) target
    {
        config_section_title(ctx, "dummy compiled target");
        ctx.addLine("# this target will be always built before any other");
        ctx.addLine("if (CMAKE_GENERATOR STREQUAL Ninja)");
        ctx.addLine("    set(f ${CMAKE_CURRENT_BINARY_DIR}/cppan_dummy.cpp)");
        ctx.addLine("    file_write_once(${f} \"void __cppan_dummy() {}\")");
        ctx.addLine("    add_library(" + cppan_dummy_target + " ${f})");
        //ctx.addLine("    export(TARGETS " + cppan_dummy_target + " FILE " + exports_dir + cppan_dummy_target + ".cmake)");
        ctx.addLine("elseif(MSVC)");
        ctx.addLine("    add_custom_target(" + cppan_dummy_target + " ALL DEPENDS cppan_intentionally_missing_file.txt)");
        ctx.addLine("else()");
        ctx.addLine("    add_custom_target(" + cppan_dummy_target + " ALL)");
        ctx.addLine("endif()");
        ctx.addLine();
        ctx.addLine("set_target_properties(" + cppan_dummy_target + " PROPERTIES\n    FOLDER \"cppan/service\"\n)");
        ctx.emptyLines(1);
    }

    // public library
    {
        config_section_title(ctx, "helper interface library");

        ctx.addLine("add_library(" + cppan_helpers_target + " INTERFACE)");
        ctx.addLine("add_dependencies(" + cppan_helpers_target + " " + cppan_dummy_target + ")");
        ctx.addLine();

        // common include directories
        ctx.addLine("target_include_directories(" + cppan_helpers_target);
        ctx.increaseIndent();
        ctx.addLine("INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}");
        ctx.decreaseIndent();
        ctx.addLine(")");
        ctx.addLine();

        // common definitions
        ctx.addLine("target_compile_definitions(" + cppan_helpers_target);
        ctx.increaseIndent();
        ctx.addLine("INTERFACE CPPAN"); // build is performed under CPPAN
        ctx.addLine("INTERFACE CPPAN_BUILD"); // build is performed under CPPAN
        ctx.addLine("INTERFACE CPPAN_CONFIG=\"${config}\"");
        ctx.addLine("INTERFACE CPPAN_SYMBOL_EXPORT=${CPPAN_EXPORT}");
        ctx.addLine("INTERFACE CPPAN_SYMBOL_IMPORT=${CPPAN_IMPORT}");
        ctx.decreaseIndent();
        ctx.addLine(")");
        ctx.addLine();

        // common link libraries
        ctx.addLine(R"(if (WIN32)
target_link_libraries()" + cppan_helpers_target + R"(
    INTERFACE Ws2_32
)
else()
    find_library(pthread pthread)
    if (NOT ${pthread} STREQUAL "pthread-NOTFOUND")
        target_link_libraries()" + cppan_helpers_target + R"(
            INTERFACE pthread
        )
    endif()
    find_library(rt rt)
    if (NOT ${rt} STREQUAL "rt-NOTFOUND")
        target_link_libraries()" + cppan_helpers_target + R"(
            INTERFACE rt
        )
    endif()
endif()
)");
        ctx.addLine();

        // Do not use APPEND here. It's the first file that will clear cppan.cmake.
        ctx.addLine("export(TARGETS " + cppan_helpers_target + " FILE " + exports_dir + cppan_helpers_target + ".cmake)");
        ctx.emptyLines(1);
    }

    // private library
    {
        config_section_title(ctx, "private helper interface library");

        ctx.addLine("add_library(" + cppan_helpers_private_target + " INTERFACE)");
        ctx.addLine("add_dependencies(" + cppan_helpers_private_target + " " + cppan_dummy_target + ")");
        ctx.addLine();

        // msvc
        ctx.addLine(R"(if (MSVC)
target_compile_definitions()" + cppan_helpers_private_target + R"(
    INTERFACE _CRT_SECURE_NO_WARNINGS # disable warning about non-standard functions
)
target_compile_options()" + cppan_helpers_private_target + R"(
    INTERFACE /wd4005 # macro redefinition
    INTERFACE /wd4996 # The POSIX name for this item is deprecated.
)
endif()
)");

        // Do not use APPEND here. It's the first file that will clear cppan.cmake.
        ctx.addLine("export(TARGETS " + cppan_helpers_private_target + " FILE " + exports_dir + cppan_helpers_private_target + ".cmake)");
        ctx.emptyLines(1);
    }

    // global definitions
    config_section_title(ctx, "global definitions");

    Context local;
    bool has_defs = false;
    local << "target_compile_definitions(" << cppan_helpers_target << Context::eol;
    local.increaseIndent();
    for (auto &o : global_options)
    {
        for (auto &opt : o.second.global_definitions)
        {
            local.addLine("INTERFACE " + opt);
            has_defs = true;
        }
    }
    local.decreaseIndent();
    local.addLine(")");
    local.addLine();
    if (has_defs)
        ctx += local;

    // definitions
    config_section_title(ctx, "definitions");

    add_if_definition("WORDS_BIGENDIAN", "BIGENDIAN", "BIG_ENDIAN", "HOST_BIG_ENDIAN");

    add_check_definitions(check_functions, convert_function);
    add_check_symbol_definitions(check_symbols, convert_function);
    add_check_definitions(check_includes, convert_include);
    add_check_definitions(check_types, convert_type);

    if (add_run_cppan_target && !disable_run_cppan_target)
    {
        // re-run cppan when root cppan.yml is changed
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
        ${PROJECT_SOURCE_DIR}/cppan/)" + cmake_functions_filename + R"(
        ${PROJECT_SOURCE_DIR}/cppan/)" + cmake_helpers_filename + R"(
)
add_dependencies()" + cppan_helpers_target + R"( run-cppan)
set_target_properties(run-cppan PROPERTIES
    FOLDER "cppan/service"
))");
    }

    // direct deps' build actions for non local build
    if (!local_build)
    {
        config_section_title(ctx, "custom actions for dummy target");

        auto dd = getDirectDependencies();
        if (!internal_options.current_package.empty())
        {
            auto &p = projects.begin()->second;
            dd = internal_options.current_package.getDirectDependenciesFixed(p);
        }

        // pre
        for (auto &dp : dd)
        {
            auto &d = dp.second;
            PackageInfo pi(d);

            if (d.flags[pfHeaderOnly] || d.flags[pfIncludeDirectories])
                continue;

            if (!d.flags[pfExecutable])
                ctx.addLine("get_configuration(config)");
            else
                ctx.addLine("get_configuration_exe(config)");
            ctx.addLine("set(current_dir " + normalize_path(d.getPackageDirHash(get_storage_dir_obj())) + ")");
            ctx.addLine("set(build_dir ${current_dir}/build/${config})");
            ctx.addLine("add_custom_command(TARGET " + cppan_dummy_target + " PRE_BUILD");
            ctx.increaseIndent();
            ctx.addLine("COMMAND ${CMAKE_COMMAND}");
            ctx.increaseIndent();
            ctx.addLine("-DTARGET_FILE=$<TARGET_FILE:" + pi.target_name + ">");
            ctx.addLine("-DCONFIG=$<CONFIG>");
            ctx.addLine("-DBUILD_DIR=${build_dir}");
            ctx.addLine("-P " + normalize_path(d.getPackageDirHash(get_storage_dir_obj())) + "/" + non_local_build_file);
            ctx.decreaseIndent();
            ctx.decreaseIndent();
            ctx.addLine(")");
            //if (d.flags[pfExecutable])
            //    ctx.addLine("add_dependencies(" + pi.target_name + " " + cppan_helpers_target + ")");
            ctx.addLine();
        }

        // post (copy)
        // no copy for non local builds
        if (internal_options.current_package.empty())
        {
            ctx.addLine("if (NOT CPPAN_LOCAL_BUILD AND CPPAN_BUILD_SHARED_LIBS)");
            ctx.addLine();
            ctx.addLine("set(output_dir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})");
            ctx.addLine("if (MSVC OR XCODE)");
            ctx.addLine("    set(output_dir ${output_dir}/$<CONFIG>)");
            ctx.addLine("endif()");
            ctx.addLine("if (CPPAN_BUILD_OUTPUT_DIR)");
            ctx.addLine("    set(output_dir ${CPPAN_BUILD_OUTPUT_DIR})");
            ctx.addLine("endif()");
            ctx.addLine();

            print_copy_deps(ctx, dd);

            ctx.addLine("endif()");
            ctx.addLine();
        }
    }

    ctx.addLine(config_delimeter);
    ctx.addLine();

    access_table->write_if_older(fn, ctx.getText());
}

path Config::get_storage_dir(PackagesDirType type) const
{
    static auto system_cfg = load_system_config();
    static auto user_cfg = load_user_config();

    switch (type)
    {
    case PackagesDirType::Local:
        return path(CPPAN_LOCAL_DIR) / "packages";
    case PackagesDirType::User:
        return user_cfg.storage_dir;
    case PackagesDirType::System:
        return system_cfg.storage_dir;
    default:
        return storage_dir;
    }
}

PackagesDirType packages_dir_type_from_string(const String &s, const String &key)
{
    if (s == "local")
        return PackagesDirType::Local;
    if (s == "user")
        return PackagesDirType::User;
    if (s == "system")
        return PackagesDirType::System;
    throw std::runtime_error("Unknown '" + key + "'. Should be one of [local, user, system]");
}

path Config::get_storage_dir_bin() const
{
    return get_storage_dir(storage_dir_type) / "bin";
}

path Config::get_storage_dir_cfg() const
{
    return get_storage_dir(storage_dir_type) / "cfg";
}

path Config::get_storage_dir_etc() const
{
    return get_storage_dir(storage_dir_type) / "etc";
}

path Config::get_storage_dir_lib() const
{
    return get_storage_dir(storage_dir_type) / "lib";
}

path Config::get_storage_dir_obj() const
{
    return get_storage_dir(storage_dir_type) / "obj";
}

path Config::get_storage_dir_src() const
{
    return get_storage_dir(storage_dir_type) / "src";
}

path Config::get_storage_dir_user_obj() const
{
    return get_storage_dir(storage_dir_type) / "usr" / "obj";
}

path Config::get_build_dir(PackagesDirType type) const
{
    switch (type)
    {
    case PackagesDirType::Local:
        return fs::current_path();
    case PackagesDirType::User:
        return get_storage_dir_user_obj();
    case PackagesDirType::System:
        return temp_directory_path() / "build";
    default:
        return build_dir;
    }
}

Dependencies Config::getDirectDependencies() const
{
    Dependencies deps;
    for (auto d : dependencies)
    {
        // TODO: manually find direct deps! remove pfDirectDependency flag
        if (d.second.flags[pfDirectDependency])
            deps[d.second.package.toString()] = d.second;
    }
    return deps;
}

Dependencies Config::getIndirectDependencies() const
{
    Dependencies deps;
    for (auto d : dependencies)
    {
        // TODO: manually find direct deps! remove pfDirectDependency flag
        if (!d.second.flags[pfDirectDependency])
            deps[d.second.package.toString()] = d.second;
    }
    return deps;
}

Dependencies Config::getDependencies() const
{
    Dependencies dependencies;
    for (auto &p : projects)
    {
        for (auto &d : p.second.dependencies)
        {
            // FIXME: why skip is_relative deps???
            if (d.second.package.is_relative())
                continue;
            dependencies.insert({ d.second.package.toString(), { d.second.package, d.second.version} });
        }
    }
    return dependencies;
}
