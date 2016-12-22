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

#include "build.h"

#include <access_table.h>
#include <config.h>
#include <database.h>
#include <directories.h>
#include <hash.h>
#include <http.h>
#include <log.h>
#include <program.h>
#include <resolver.h>
#include <settings.h>
#include <templates.h>

#include <iostream>

int build_packages(const String &name, const std::set<Package> &pkgs, const path &settings_fn, const String &config);
int build_packages(const String &name, const std::set<Package> &pkgs);
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

    auto printer = Printer::create(Settings::get_local_settings().printerType);
    printer->prepare_build(s);

    LOG("--");
    LOG("-- Performing test run");
    LOG("--");

    auto ret = printer->generate(s);

    if (ret)
        throw std::runtime_error("There are errors during test run");

    // read cfg
    auto c = read_file(bin_dir / CPPAN_CONFIG_FILENAME);
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
    if (!fs::exists(src))
    {
        auto config = test_run();
        if (bs.config != config)
        {
            // the original config was detected incorrectly, re-apply
            set_config(config);

            // do we need to addConfigHash() here? like in get_config()
            // or config must be very unique?
        }
    }

    if (!fs::exists(src))
        throw std::runtime_error("src dir does not exist");

    // move this to printer some time
    // copy cached cmake config to bin dir
    auto dst = bs.binary_directory / "CMakeFiles" / cmake_version;
    if (!fs::exists(dst))
        copy_dir(src, dst);

    // setup printer config
    c.process(bs.source_directory);
    auto printer = Printer::create(Settings::get_local_settings().printerType);
    printer->prepare_build(bs);

    auto ret = printer->generate(bs);
    if (ret)
        return ret;
    return printer->build(bs);
}

int build(path fn, const String &config)
{
    std::set<Package> pkgs;
    Config c;
    String name;
    std::tie(pkgs, c, name) = rd.read_packages_from_file(fn, config, true);
    return build_packages(name, pkgs) == 0;
}

int build_only(path fn, const String &config)
{
    AccessTable::do_not_update_files(true);
    return build(fn, config);
}

int build_packages(const String &name, const std::set<Package> &pkgs)
{
    Config c;
    for (auto &p : pkgs)
        c.getDefaultProject().addDependency(p);
    return build_packages(c, name);
}

int build_packages(const String &name, const std::set<Package> &pkgs, const path &settings_fn, const String &config)
{
    Config c;
    if (!settings_fn.empty())
    {
        auto root = load_yaml_config(settings_fn);
        if (!config.empty())
            root["local_settings"]["current_build"] = config;
        Settings::get_local_settings().load(root, SettingsType::Local);
    }

    for (auto &p : pkgs)
        c.getDefaultProject().addDependency(p);
    return build_packages(c, name);
}

int build_package(const String &target_name, const path &settings_fn, const String &config)
{
    auto p = extractFromString(target_name);
    return build_packages(p.ppath.back(), { p }, settings_fn, config);
}
