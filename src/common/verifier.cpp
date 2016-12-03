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

#include "verifier.h"

#include "command.h"
#include "config.h"
#include "http.h"
#include "package.h"
#include "resolver.h"
#include "property_tree.h"
#include "spec.h"
#include "templates.h"

#include "logger.h"
DECLARE_STATIC_LOGGER(logger, "verifier");

void verify(const String &target_name)
{
    auto pkg = extractFromString(target_name);

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
        LOG_INFO(logger, "Resolving  : " << pkg.target_name << "...");

        auto fn = dir_cppan / make_archive_name();
        resolve_and_download(pkg, fn);

        LOG_INFO(logger, "Unpacking  : " << pkg.target_name << "...");
        unpack_file(fn, dir_cppan);
        fs::remove(fn);
    }

    // only after cppan resolve step
    LOG_INFO(logger, "Downloading package specification...");
    auto spec = download_specification(pkg);
    if (spec.package != pkg)
        throw std::runtime_error("Packages do not match (" + pkg.target_name + " vs. " + spec.package.target_name + ")");

    // download & prepare original sources
    {
        LOG_INFO(logger, "Downloading original package from source...");
        LOG_INFO(logger, print_source(spec.source));

        ScopedCurrentPath cp(dir_original_unprepared);

        DownloadSource ds;
        ds.download(spec.source);
        write_file(CPPAN_FILENAME, spec.cppan);

        Config c(CPPAN_FILENAME);
        auto &project = c.getDefaultProject();
        project.findSources(".");
        String archive_name = make_archive_name("original");
        if (!project.writeArchive(archive_name))
            throw std::runtime_error("Archive write failed");

        unpack_file(archive_name, dir_original);
    }
    fs::remove_all(dir_original_unprepared); // after current scope leave

    // remove spec files, maybe check them too later
    fs::remove(dir_cppan / CPPAN_FILENAME);
    fs::remove(dir_original / CPPAN_FILENAME);

    LOG_INFO(logger, "Comparing packages...");
    if (!compare_dirs(dir_cppan, dir_original))
        throw std::runtime_error("Error! Packages are different.");
    LOG_INFO(logger, "Verified... Ok. Packages are the same.");
}
