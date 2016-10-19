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

#include <boost/algorithm/string.hpp>

#include <access_table.h>
#include <config.h>
#include <hash.h>
#include <http.h>
#include <response.h>

struct Parameters
{
    String config;
    bool silent = true;
    bool rebuild = false;
    bool prepare = true;
    bool download = true;
};

std::vector<std::string> extract_comments(const std::string &s);

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

Config generate_config(path p, const Parameters &params)
{
    if (params.download)
        download_file(p);

    auto conf = Config::get_user_config();
    conf.type = ConfigType::Local;
    conf.defaults_allowed = false;

    if (!fs::exists(p))
        throw std::runtime_error("File or directory does not exist: " + p.string());

    auto read_from_cpp = [&conf, &params](const path &fn)
    {
        auto s = read_file(fn);
        auto comments = extract_comments(s);

        int loaded = -1;
        int i = -1;
        for (auto &comment : comments)
        {
            try
            {
                i++;
                boost::trim(comment);
                auto root = YAML::Load(comment);
                auto sz = root.size();
                if (sz == 0)
                    continue;
                if (!params.config.empty())
                    root["local_settings"]["current_build"] = params.config;
                conf.load(root);
                loaded = i;
                break;
            }
            catch (...)
            {
            }
        }

        conf.local_settings.build_settings.silent = params.silent;
        conf.local_settings.build_settings.rebuild = params.rebuild;
        conf.local_settings.build_settings.prepare = params.prepare;
        conf.prepare_build(fn, comments.size() > (size_t)i ? comments[i] : "");
    };

    if (fs::is_regular_file(p))
    {
        read_from_cpp(p);
    }
    else if (fs::is_directory(p))
    {
        auto cppan_fn = p / CPPAN_FILENAME;
        if (fs::exists(cppan_fn))
        {
            conf = Config(p);
            conf.local_settings.build_settings.prepare = params.prepare;
            conf.prepare_build(cppan_fn, read_file(cppan_fn));
        }
        else if (fs::exists(p / "main.cpp"))
        {
            read_from_cpp(p / "main.cpp");
        }
        else
            throw std::runtime_error("No candidates {cppan.yml|main.cpp} for reading in directory " + p.string());
    }

    return conf;
}

int generate(path fn, const String &config)
{
    Parameters params;
    params.config = config;
    params.silent = false;
    auto conf = generate_config(fn, params);
    return conf.generate();
}

int build(path fn, const String &config, bool rebuild)
{
    Parameters params;
    params.config = config;
    params.rebuild = rebuild;
    auto conf = generate_config(fn, params);
    if (conf.generate())
        return 1;
    return conf.build();
}

int build_only(path fn, const String &config)
{
    AccessTable::do_not_update_files(true);

    Parameters params;
    params.config = config;
    params.prepare = false;
    auto conf = generate_config(fn, params);
    return conf.build();
}

int dry_run(path p, const String &config)
{
    Config c(p);
    auto cppan_fn = p / CPPAN_FILENAME;
    c.prepare_build(cppan_fn, read_file(cppan_fn));

    auto &project = c.getDefaultProject();
    auto dst = c.local_settings.build_settings.source_directory / "src";
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
    pkg.ppath = sha256(c.local_settings.build_settings.source_directory.string());
    if (c.version.isValid())
        pkg.version = c.version;
    else
        pkg.version = String("master");

    c2.setPackage(pkg);
    rd.add_config(std::make_unique<Config>(c2), true);

    Parameters params;
    params.config = config;
    params.download = false;
    auto conf = generate_config(p, params);
    return conf.build();
}
