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
#include <hash.h>
#include <http.h>
#include <response.h>

#include <boost/algorithm/string.hpp>

#include <iostream>

struct Parameters
{
    String config;
    //bool silent = true;
    //bool prepare = true;
    //bool download = true;
};

std::vector<std::string> extract_comments(const std::string &s);
int build_package(const Package &p, const path &settings, const String &config);
int build_package(const Package &p, const Settings &settings);

void download_file(path &fn)
{
    // this function checks if fn is url,
    // tries to download it to current dir and run cppan on it
    auto s = fn.string();
    if (!isUrl(s))
        return;
    fn = fn.filename();

    DownloadData dd;
    dd.url = s;
    dd.file_size_limit = 1'000'000'000;
    dd.fn = fn;
    download_file(dd);
}

std::tuple<std::vector<Package>, Config> extract_packages(path p, const Parameters &params)
{
    //if (params.download)
    download_file(p);
    p = fs::absolute(p);

    auto conf = Config::get_user_config();
    conf.type = ConfigType::Local;
    conf.defaults_allowed = false;

    if (!fs::exists(p))
        throw std::runtime_error("File or directory does not exist: " + p.string());

    auto read_from_cpp = [&conf, &params](const path &fn)
    {
        auto s = read_file(fn);
        auto comments = extract_comments(s);

        std::vector<int> load_ok;
        bool found = false;
        for (size_t i = 0; i < comments.size(); i++)
        {
            try
            {
                boost::trim(comments[i]);
                auto root = YAML::Load(comments[i]);

                auto sz = root.size();
                if (sz == 0)
                    continue;

                bool probably_this = root.IsMap() && (
                    root["local_settings"].IsDefined() ||
                    root["files"].IsDefined() ||
                    root["dependencies"].IsDefined()
                    );

                if (!params.config.empty())
                    root["local_settings"]["current_build"] = params.config;
                conf.load(root);

                if (probably_this)
                {
                    found = true;
                    break;
                }
                load_ok.push_back(i);
            }
            catch (...)
            {
            }
        }

        // fallback to the first comment w/out error
        if (!found && !load_ok.empty())
            conf.load(comments[load_ok.front()]);

        //conf.settings.silent = params.silent;
        //conf.settings.rebuild = params.rebuild;
    };

    auto build_spec_file = [&](const path &fn)
    {
        conf = Config();
        auto s = read_file(fn);
        boost::trim(s);
        conf.load(YAML::Load(s));
    };

    String sname;
    path cpp_fn;
    if (fs::is_regular_file(p))
    {
        if (p.filename() == CPPAN_FILENAME)
        {
            // allow defaults for spec file
            conf.defaults_allowed = true;

            build_spec_file(p);
            sname = p.parent_path().filename().string();
        }
        else
        {
            read_from_cpp(p);
            sname = p.filename().stem().string();
            cpp_fn = p;
        }
    }
    else if (fs::is_directory(p))
    {
        auto cppan_fn = p / CPPAN_FILENAME;
        auto main_fn = p / "main.cpp";
        if (fs::exists(cppan_fn))
        {
            // allow defaults for spec file
            conf.defaults_allowed = true;

            build_spec_file(cppan_fn);
            sname = cppan_fn.parent_path().filename().string();
            p = cppan_fn;
        }
        else if (fs::exists(main_fn))
        {
            read_from_cpp(main_fn);
            p = main_fn;
            sname = p.filename().string();
            cpp_fn = p;
        }
        else
            throw std::runtime_error("No candidates {cppan.yml|main.cpp} for reading in directory " + p.string());
    }
    else
        throw std::runtime_error("Unknown file type " + p.string());

    std::vector<Package> packages;
    auto configs = conf.split();
    // batch resolve of deps first; merge flags?
    for (auto &c : configs)
    {
        auto &project = c.getDefaultProject();

        Package pkg;
        pkg.ppath.push_back("loc");
        pkg.ppath.push_back(sha256_short(normalize_path(p)));
        if (!project.name.empty())
            pkg.ppath.push_back(project.name);
        else
            pkg.ppath.push_back(sname);
        pkg.version = Version("local");
        pkg.flags.set(pfLocalProject);
        pkg.createNames();
        project.applyFlags(pkg.flags);
        c.setPackage(pkg);

        // sources
        if (!cpp_fn.empty())
            project.sources.insert(cpp_fn.filename().string());
        project.findSources(p.parent_path());
        project.files.erase(CPPAN_FILENAME);

        rd.add_local_config(c);

        packages.push_back(pkg);
    }
    rd.write_index();
    return{ packages, conf };
}

int generate(path fn, const String &config)
{
    Parameters params;
    params.config = config;
    //params.silent = false;
    //auto conf = generate_config(fn, params);
    //return conf.settings.generate(&conf);
    return 0;
}

int build(path fn, const String &config)
{
    Parameters params;
    params.config = config;
    std::vector<Package> pkgs;
    Config c;
    std::tie(pkgs, c) = extract_packages(fn, params);
    bool r = true;
    for (auto &pkg : pkgs)
        r &= build_package(pkg, c.settings) == 0;
    return r;
}

int build_only(path fn, const String &config)
{
    AccessTable::do_not_update_files(true);

    Parameters params;
    params.config = config;
    //auto conf = generate_config(fn, params);
    //return conf.settings.build(&conf);
    return 0;
}

int dry_run(path p, const String &config)
{
    Config c(p);
    auto cppan_fn = p / CPPAN_FILENAME;
    //c.settings.prepare_build(&c, cppan_fn, read_file(cppan_fn));

    auto &project = c.getDefaultProject();
    auto dst = c.settings.source_directory / "src";
    for (auto &f : project.files)
    {
        fs::create_directories((dst / f).parent_path());
        fs::copy_file(p / f, dst / f, fs::copy_option::overwrite_if_exists);
    }
    fs::copy_file(p / CPPAN_FILENAME, dst / CPPAN_FILENAME, fs::copy_option::overwrite_if_exists);

    Config c2(dst);
    Package pkg;
    pkg.setLocalSourceDir(dst);

    // TODO: add more robust ppath, version choser
    pkg.flags.set(pfLocalProject);
    pkg.ppath = sha256(c.settings.source_directory.string());
    if (project.version.isValid())
        pkg.version = project.version;
    else
        pkg.version = String("master");

    c2.setPackage(pkg);
    rd.add_config(std::make_unique<Config>(c2), true);

    Parameters params;
    params.config = config;
    //params.download = false;
    //auto conf = generate_config(p, params);
    //return conf.settings.build(&conf);
    return 0;
}

int build_package(const Package &p, const Settings &settings)
{
    Config c;
    c.getDefaultProject().addDependency(p);
    c.settings = settings;
    return c.settings.build_package(c, p);
}

int build_package(const Package &p, const path &settings, const String &config)
{
    Config c;
    if (!settings.empty())
    {
        yaml root;
        auto s = YAML::LoadFile(settings.string());
        merge(s, root);

        if (!config.empty())
            root["local_settings"]["current_build"] = config;
        Config c2;
        c2.load(root);
        c.settings = c2.settings;
    }

    c.getDefaultProject().addDependency(p);
    return c.settings.build_package(c, p);
}

int build_package(const String &target_name, const path &settings, const String &config)
{
    auto p = extractFromString(target_name);
    return build_package(p, settings, config);
}
