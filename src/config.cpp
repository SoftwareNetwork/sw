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

#include "config.h"

#include <fstream>
#include <iostream>
#include <regex>
#include <tuple>

#include <boost/algorithm/string.hpp>

#include <curl/curl.h>
#include <curl/easy.h>

#include "access_table.h"
#include "context.h"
#include "file_lock.h"
#include "hasher.h"
#include "log.h"
#include "response.h"
#include "stamp.h"

Directories directories;

void get_config_insertion(const yaml &n, const String &key, String &dst)
{
    dst = get_scalar<String>(n, key);
    boost::trim(dst);
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

void Directories::set_storage_dir(const path &p)
{
    if (!empty())
        return;

    storage_dir = p;

#define SET(x)                          \
    storage_dir_##x = storage_dir / #x; \
    fs::create_directories(storage_dir_##x)

    SET(bin);
    SET(cfg);
    SET(etc);
    SET(lib);
    SET(obj);
    SET(src);
    SET(usr);
#undef SET

}

void Directories::set_build_dir(const path &p)
{
    build_dir = p;
}

void BuildSettings::load(const yaml &root)
{
    if (root.IsNull())
        return;

    // extract
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
        auto t = configuration_types[i];
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

void BuildSettings::set_build_dirs(const path &fn)
{
    filename = fn.filename().string();
    filename_without_ext = fn.filename().stem().string();
    if (filename == CPPAN_FILENAME)
    {
        is_dir = true;
        filename = fn.parent_path().filename().string();
        filename_without_ext = filename;
    }

    source_directory = directories.build_dir;
    if (directories.build_dir_type == PackagesDirType::Local ||
        directories.build_dir_type == PackagesDirType::None)
        source_directory /= (CPPAN_LOCAL_BUILD_PREFIX + filename);
    else
        source_directory /= sha1(normalize_path(fn.string())).substr(0, 6);
    binary_directory = source_directory / "build";
}

void BuildSettings::append_build_dirs(const path &p)
{
    source_directory /= p;
    binary_directory = source_directory / "build";
}

void BuildSettings::prepare_build(Config *c, const path &fn, String cppan)
{
    auto &p = c->getDefaultProject();
    if (!is_dir)
        p.sources.insert(filename);
    p.findSources(fn.parent_path());
    p.files.erase(CPPAN_FILENAME);

    if (rebuild)
        fs::remove_all(source_directory);
    fs::create_directories(source_directory);

    write_file_if_different(source_directory / CPPAN_FILENAME, cppan);
    Config conf(source_directory);
    conf.process(source_directory); // invoke cppan
}

String BuildSettings::get_hash() const
{
    Hasher h;
    h |= c_compiler;
    h |= cxx_compiler;
    h |= compiler;
    h |= c_compiler_flags;
for (int i = 0; i < CMakeConfigurationType::Max; i++)
    h |= c_compiler_flags_conf[i];
    h |= cxx_compiler_flags;
for (int i = 0; i < CMakeConfigurationType::Max; i++)
    h |= cxx_compiler_flags_conf[i];
    h |= compiler_flags;
for (int i = 0; i < CMakeConfigurationType::Max; i++)
    h |= compiler_flags_conf[i];
    h |= link_flags;
for (int i = 0; i < CMakeConfigurationType::Max; i++)
    h |= link_flags_conf[i];
    h |= link_libraries;
    h |= generator;
    h |= toolset;
    h |= use_shared_libs;
    return h.hash;
}

LocalSettings::LocalSettings()
{
    build_dir = temp_directory_path() / "build";
    storage_dir = get_root_directory() / STORAGE_DIR;
}

void LocalSettings::load(const path &p)
{
    auto root = YAML::LoadFile(p.string());
    load(root);
}

void LocalSettings::load(const yaml &root)
{
    load_main(root);

    auto get_storage_dir = [this](PackagesDirType type)
    {
        auto system_cfg = Config::load_system_config();
        auto user_cfg = Config::load_user_config();

        switch (type)
        {
        case PackagesDirType::Local:
            return path(CPPAN_LOCAL_DIR) / STORAGE_DIR;
        case PackagesDirType::User:
            return user_cfg.local_settings.storage_dir;
        case PackagesDirType::System:
            return system_cfg.local_settings.storage_dir;
        default:
            return storage_dir;
        }
    };

    auto get_build_dir = [this](const path &p, PackagesDirType type)
    {
        switch (type)
        {
        case PackagesDirType::Local:
            return fs::current_path();
        case PackagesDirType::User:
            return directories.storage_dir_usr;
        case PackagesDirType::System:
            return temp_directory_path() / "build";
        default:
            return p;
        }
    };

    if (!directories.empty())
        return;
    directories.storage_dir_type = storage_dir_type;
    directories.build_dir_type = build_dir_type;
    directories.storage_dir = "."; // recursion stopper
    auto sd = get_storage_dir(storage_dir_type);
    directories.storage_dir.clear(); // allow changes again
    directories.set_storage_dir(sd);
    directories.set_build_dir(get_build_dir(build_dir, build_dir_type));
}

void LocalSettings::load_main(const yaml &root)
{
    auto packages_dir_type_from_string = [](const String &s, const String &key)
    {
        if (s == "local")
            return PackagesDirType::Local;
        if (s == "user")
            return PackagesDirType::User;
        if (s == "system")
            return PackagesDirType::System;
        throw std::runtime_error("Unknown '" + key + "'. Should be one of [local, user, system]");
    };

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

    // read build settings
    if (root["builds"].IsDefined() && root["current_build"].IsDefined())
        build_settings.load(root["builds"][root["current_build"].template as<String>()]);
    else if (root["build"].IsDefined())
        build_settings.load(root["build"]);
}

String LocalSettings::get_hash() const
{
    Hasher h;
    h |= build_settings.get_hash();
    return h.hash;
}

Config::Config()
{
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
    c.local_settings.load(fn);
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
    c.local_settings.load(fn);
    return c;
}

void Config::load_current_config()
{
    load(fs::current_path() / CPPAN_FILENAME);
}

void Config::load(const path &p)
{
    auto s = read_file(p);
    const auto root = YAML::Load(s);
    load(root);
}

void Config::load(const yaml &root, const path &p)
{
    auto ls = root["local_settings"];
    if (ls.IsDefined())
    {
        if (!ls.IsMap())
            throw std::runtime_error("'local_settings' should be a map");
        local_settings.load(root["local_settings"]);
    }
    else
    {
        // read user/system settings first
        auto uc = load_user_config();
        local_settings = uc.local_settings;
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

    // add some common types
    check_types.insert("size_t");
    check_types.insert("void *");

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
        project.ppath = relative_name_to_absolute(root_project, name);
        projects.emplace(project.ppath.toString(), project);
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

void Config::clear_vars_cache(path p) const
{
    if (p.empty())
        p = directories.storage_dir_obj;

    for (auto &f : boost::make_iterator_range(fs::recursive_directory_iterator(p), {}))
    {
        if (!fs::is_regular_file(f))
            continue;
        remove_file(f);
    }
}

Project &Config::getProject(const String &pname) const
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
    return (Project &)*p;
}

Project &Config::getDefaultProject()
{
    if (projects.empty())
        throw std::runtime_error("Projects are empty");
    return projects.begin()->second;
}

const Project &Config::getDefaultProject() const
{
    if (projects.empty())
        throw std::runtime_error("Projects are empty");
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
    EMIT_KV("host", local_settings.host);
    EMIT_KV("storage_dir", local_settings.storage_dir.string());
    e << YAML::EndMap;
}

void Config::process(const path &p)
{
    auto old = fs::current_path();
    if (!p.empty())
        fs::current_path(p);

    // main access table holder
    auto access_table = std::make_unique<AccessTable>(directories.storage_dir_etc);

    // do a request
    rd.init(this, local_settings.host, directories.storage_dir_src);
    rd.download_dependencies(getFileDependencies());

    LOG_NO_NEWLINE("Generating build configs... ");

    auto printer = Printer::create(printerType);
    printer->access_table = access_table.get();

    printer->pc = this;
    printer->rc = this;

    if (pkg.empty())
    {
        for (auto &cc : rd)
            printer->include_guards.insert(INCLUDE_GUARD_PREFIX + cc.first.variable_name);
    }

    for (auto &cc : rd)
    {
        auto &d = cc.first;
        auto c = cc.second.config;

        if (c->is_printed)
            continue;
        c->is_printed = true;

        // gather checks, options etc.
        {
#define GATHER_CHECK(array) array.insert(c->array.begin(), c->array.end())
            GATHER_CHECK(check_functions);
            GATHER_CHECK(check_includes);
            GATHER_CHECK(check_types);
            GATHER_CHECK(check_symbols);
            GATHER_CHECK(check_libraries);
#undef GATHER_CHECK

            const auto &p = getProject(d.ppath.toString());
            for (auto &ol : p.options)
            {
                if (!ol.second.global_definitions.empty())
                    c->global_options[ol.first].global_definitions.insert(ol.second.global_definitions.begin(), ol.second.global_definitions.end());
            }
        }

        printer->d = d;
        printer->cc = c;

        printer->print();
    }

    printer->cc = this;
    printer->d = pkg;
    printer->print_meta();

    LOG("Ok");

    if (!p.empty())
        fs::current_path(old);
}

void Config::post_download() const
{
    if (!downloaded)
        return;

    auto &p = getDefaultProject();
    p.prepareExports();

    // remove from table
    AccessTable at(directories.storage_dir_etc);
    at.remove(pkg.getDirSrc());
    at.remove(pkg.getDirObj());

    auto printer = Printer::create(printerType);
    printer->d = pkg;
    printer->prepare_rebuild();
}

Packages Config::getFileDependencies() const
{
    Packages dependencies;
    for (auto &p : projects)
    {
        for (auto &d : p.second.dependencies)
        {
            // skip ill-formed deps
            if (d.second.ppath.is_relative())
                continue;
            dependencies.insert({ d.second.ppath.toString(), { d.second.ppath, d.second.version} });
        }
    }
    return dependencies;
}

void Config::prepare_build(path fn, const String &cppan)
{
    fn = fs::canonical(fs::absolute(fn));

    auto printer = Printer::create(printerType);
    printer->rc = this;

    String cfg;
    {
        auto stamps_dir = directories.storage_dir_etc / STAMPS_DIR / "configs";
        if (!fs::exists(stamps_dir))
            fs::create_directories(stamps_dir);
        auto stamps_file = stamps_dir / cppan_stamp;

        std::map<String, String> hash_configs;
        {
            ScopedShareableFileLock lock(stamps_file);

            String hash;
            String config;
            std::ifstream ifile(stamps_file.string());
            while (ifile >> hash >> config)
                hash_configs[hash] = config;
        }

        auto h = local_settings.get_hash();
        auto i = hash_configs.find(h);
        if (i != hash_configs.end())
        {
            cfg = i->second;
        }
        else
        {
            // do a test build to extract config string
            local_settings.build_settings.set_build_dirs(fn);
            local_settings.build_settings.source_directory = get_temp_filename();
            local_settings.build_settings.binary_directory = local_settings.build_settings.source_directory / "build";
            local_settings.build_settings.prepare_build(this, fn, cppan);
            printer->prepare_build(fn, cppan);

            LOG("--");
            LOG("-- Performing test run");
            LOG("--");

            auto olds = local_settings.build_settings.silent;
            local_settings.build_settings.silent = true;
            auto ret = printer->generate();
            local_settings.build_settings.silent = olds;

            if (ret)
            {
                fs::remove_all(local_settings.build_settings.source_directory);
                throw std::runtime_error("There are errors during test run");
            }

            cfg = read_file(local_settings.build_settings.binary_directory / CPPAN_CONFIG_FILENAME);
            hash_configs[h] = cfg;
            fs::remove_all(local_settings.build_settings.source_directory);
        }
        local_settings.build_settings.config = cfg;

        {
            ScopedFileLock lock(stamps_file);

            String hash;
            String config;
            std::ofstream ofile(stamps_file.string());
            if (ofile)
            {
                for (auto &hc : hash_configs)
                    ofile << hc.first << " " << hc.second << "\n";
            }
        }
    }

    local_settings.build_settings.set_build_dirs(fn);
    local_settings.build_settings.append_build_dirs(cfg);
    local_settings.build_settings.prepare_build(this, fn, cppan);

    printer->prepare_build(fn, cppan);
}

int Config::generate() const
{
    auto printer = Printer::create(printerType);
    printer->rc = (Config *)this;
    return printer->generate();
}

int Config::build() const
{
    auto printer = Printer::create(printerType);
    printer->rc = (Config *)this;
    return printer->build();
}
