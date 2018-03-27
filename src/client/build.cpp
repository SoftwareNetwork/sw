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

#include "build.h"

#include <access_table.h>
#include <config.h>
#include <database.h>
#include <directories.h>
#include <hash.h>
#include <http.h>
#include <program.h>
#include <resolver.h>
#include <settings.h>

#include <primitives/templates.h>

#include <iostream>

#include <primitives/log.h>
//DECLARE_STATIC_LOGGER(logger, "build");

int build_packages(const String &name, const PackagesSet &pkgs, const path &settings_fn, const String &config);
int build_packages(const String &name, const PackagesSet &pkgs);
int build_packages(const Config &c, const String &name);

String test_run()
{
    // do a test build to extract config string
    auto src_dir = temp_directory_path() / "temp" / fs::unique_path();
    auto bin_dir = src_dir / "build";

    fs::create_directories(src_dir);
    write_file(src_dir / CPPAN_FILENAME, "");
    SCOPE_EXIT
    {
        // remove test dir
        boost::system::error_code ec;
        fs::remove_all(src_dir, ec);
    };

    // invoke cppan
    Config conf(src_dir);
    conf.process(src_dir);

    BuildSettings s;
    s.allow_links = false;
    s.disable_checks = true;
    s.source_directory = src_dir;
    s.binary_directory = bin_dir;
    s.test_run = true;

    auto printer = Printer::create(Settings::get_local_settings().printerType);
    printer->prepare_build(s);

    LOG_INFO(logger, "--");
    LOG_INFO(logger, "-- Performing test run");
    LOG_INFO(logger, "--");

    auto ret = printer->generate(s);

    if (ret)
        throw std::runtime_error("There are errors during test run");

    // read cfg
    auto c = read_file(bin_dir / CPPAN_CONFIG_FILENAME);
    if (c.empty())
        throw std::logic_error("Test config is empty");

    auto cmake_version = get_cmake_version();

    // move this to printer some time
    // copy cached cmake config to storage
    copy_dir(
        bin_dir / "CMakeFiles" / cmake_version,
        directories.storage_dir_cfg / hash_config(c) / "CMakeFiles" / cmake_version);

    return c;
}

String get_config()
{
    // add original config to db
    // but return hashed

    auto &db = getServiceDatabase();
    auto h = Settings::get_local_settings().get_hash();
    auto c = db.getConfigByHash(h);

    if (!c.empty())
        return hash_config(c);

    c = test_run();
    auto ch = hash_config(c);
    db.addConfigHash(h, c, ch);

    return ch;
}

int build_packages(const Config &c, const String &name)
{
    auto &sdb = getServiceDatabase();

    // install all pkgs first
    for (auto &p : c.getProjects())
        for (auto &d : p.second.dependencies)
            sdb.addInstalledPackage(d.second);

    BuildSettings bs;

    path src;
    String cmake_version;

    auto set_config = [&](const String &config)
    {
        bs.config = config;
        bs.set_build_dirs(name);
        bs.append_build_dirs(bs.config);

        cmake_version = get_cmake_version();
        src = directories.storage_dir_cfg / bs.config / "CMakeFiles" / cmake_version;
    };

    set_config(get_config());

    // if dir does not exist it means probably we have new cmake version
    // we have config value but there was not a test run with copying cmake prepared files
    // so start unconditional test run
    bool new_config = false;
    if (!fs::exists(src))
    {
        auto config = test_run();
        auto ch = hash_config(config);
        if (bs.config != ch)
        {
            // the original config was detected incorrectly, re-apply
            set_config(ch);
            new_config = true;

            // also register in db
            auto h = Settings::get_local_settings().get_hash();
            sdb.addConfigHash(h, config, ch);

            // do we need to addConfigHash() here? like in get_config()
            // or config must be very unique?
        }

        if (!fs::exists(src))
            throw std::runtime_error("src dir does not exist");
    }

    // move this to printer some time
    // copy cached cmake config to bin dir
    auto dst = bs.binary_directory / "CMakeFiles" / cmake_version;
    if (new_config)
        fs::remove_all(dst);
    if (!fs::exists(dst))
    {
        copy_dir(src, dst);
        // since cmake 3.8
        write_file(bs.binary_directory / "CMakeCache.txt", "CMAKE_PLATFORM_INFO_INITIALIZED:INTERNAL=1\n");
    }

    auto &ls = Settings::get_local_settings();

    // setup printer config
    c.process(bs.source_directory);
    auto printer = Printer::create(ls.printerType);
    printer->prepare_build(bs);

    auto ret = printer->generate(bs);
    if (ret || ls.generate_only)
        return ret;
    return printer->build(bs);
}

int build(path fn, const String &config)
{
    PackagesSet pkgs;
    Config c;
    String name;
    std::tie(pkgs, c, name) = rd.read_packages_from_file(fn, config, true);
    return build_packages(name, pkgs);
}

int build_only(path fn, const String &config)
{
    AccessTable::do_not_update_files(true);
    return build(fn, config);
}

int build_packages(const String &name, const PackagesSet &pkgs)
{
    Config c;
    for (auto &p : pkgs)
        c.getDefaultProject().addDependency(p);
    return build_packages(c, name);
}

int build_packages(const String &name, const PackagesSet &pkgs, const path &settings_fn, const String &config)
{
    Config c;
    if (!config.empty() && (fs::exists(settings_fn) || fs::exists(CPPAN_FILENAME)))
    {
        auto root = load_yaml_config(settings_fn.empty() ? CPPAN_FILENAME : settings_fn);
        root["local_settings"]["current_build"] = config;
        Settings::get_local_settings().load(root["local_settings"], SettingsType::Local);
    }

    for (auto &p : pkgs)
        c.getDefaultProject().addDependency(p);
    return build_packages(c, name);
}

int build_package(const String &target_name, const path &settings_fn, const String &config)
{
    Settings::get_local_settings().copy_all_libraries_to_output = true;

    Package p;
    PackagesSet pkgs;
    std::tie(p, pkgs) = resolve_dependency(target_name);
    if (std::all_of(pkgs.begin(), pkgs.end(), [](const auto &p) { return p.flags[pfHeaderOnly]; }))
        throw std::runtime_error("You are trying to build header only project. This is not supported.");
    return build_packages(p.ppath.back(), pkgs, settings_fn, config);
}
