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

#include "verifier.h"

#include "config.h"
#include "http.h"
#include "package.h"
#include "property_tree.h"
#include "resolver.h"
#include "spec.h"

#include <primitives/command.h>
#include <primitives/pack.h>
#include <primitives/templates.h>

#include <primitives/log.h>
//DECLARE_STATIC_LOGGER(logger, "verifier");

void verify(const String &target_name)
{
    auto pkg = extractFromString(target_name);
    verify(pkg);
}

void verify(const Package &pkg, path fn)
{
    LOG_INFO(logger, "Verifying  : " << pkg.target_name << "...");

    auto dir = get_temp_filename("verifier");
    auto dir_original_unprepared = dir / "original_unprepared";
    auto dir_original = dir / "original";
    auto dir_cppan = dir / "cppan";

    fs::create_directories(dir_original_unprepared);
    fs::create_directories(dir_original);
    fs::create_directories(dir_cppan);

    SCOPE_EXIT
    {
        boost::system::error_code ec;
        fs::remove_all(dir, ec);
    };

    // download & prepare cppan sources
    // we also resolve dependency here
    {
        bool rm = fn.empty();
        if (fn.empty())
        {
            LOG_DEBUG(logger, "Resolving  : " << pkg.target_name << "...");
            LOG_DEBUG(logger, "Downloading: " << pkg.target_name << "...");

            fn = dir_cppan / make_archive_name();
            resolve_and_download(pkg, fn);
        }

        LOG_DEBUG(logger, "Unpacking  : " << pkg.target_name << "...");
        unpack_file(fn, dir_cppan);
        if (rm)
            fs::remove(fn);
    }

    // only after cppan resolve step
    LOG_DEBUG(logger, "Downloading package specification...");
    auto spec = download_specification(pkg);
    if (spec.package != pkg)
        throw std::runtime_error("Packages do not match (" + pkg.target_name + " vs. " + spec.package.target_name + ")");

    // download & prepare original sources
    {
        LOG_DEBUG(logger, "Downloading original package from source...");
        LOG_DEBUG(logger, print_source(spec.source));

        ScopedCurrentPath cp(dir_original_unprepared, CurrentPathScope::All);

        applyVersionToUrl(spec.source, spec.package.version);
        download(spec.source);
        write_file(CPPAN_FILENAME, spec.cppan);

        Config c(CPPAN_FILENAME);
        auto &project = c.getDefaultProject();
        project.findSources();
        String archive_name = make_archive_name("original");
        if (!project.writeArchive(fs::absolute(archive_name)))
            throw std::runtime_error("Archive write failed");

        unpack_file(archive_name, dir_original);
    }
    fs::remove_all(dir_original_unprepared); // after current scope leave

    // remove spec files, maybe check them too later
    fs::remove(dir_cppan / CPPAN_FILENAME);
    fs::remove(dir_original / CPPAN_FILENAME);

    LOG_DEBUG(logger, "Comparing packages...");
    if (!compare_dirs(dir_cppan, dir_original))
        throw std::runtime_error("Error! Packages are different.");
}
