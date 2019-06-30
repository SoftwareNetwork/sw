/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2019 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "build.h"

//#include <sw/driver/build.h>
#include <sw/support/filesystem.h>

#include <primitives/exceptions.h>

#include <optional>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "build");

namespace sw
{

std::unique_ptr<Build> load(const SwContext &swctx, const path &file_or_dir)
{
    SW_UNIMPLEMENTED;

    /*auto f = resolveConfig(file_or_dir);
    if (!f || !Build::isFrontendConfigFilename(f.value()))
    {
        if (f && !Build::isFrontendConfigFilename(f.value()))
            LOG_INFO(logger, "Unknown config, trying in configless mode. Default mode is native (ASM/C/C++)");

        path p = file_or_dir;
        p = fs::absolute(p);

        auto b = std::make_unique<Build>(swctx);
        b->Local = true;
        b->setSourceDirectory(fs::is_directory(p) ? p : p.parent_path());
        b->load(p, true);
        return b;
    }

    auto b = std::make_unique<Build>(swctx);
    b->Local = true;
    b->setSourceDirectory(f.value().parent_path());
    b->load(f.value());

    return b;*/
}

void run(const SwContext &swctx, const PackageId &package)
{
    SW_UNIMPLEMENTED;
    //auto b = std::make_unique<Build>(swctx);
    //b->run_package(package.toString());
}

std::optional<String> read_config(const path &file_or_dir)
{
    SW_UNIMPLEMENTED;
    /*auto f = findConfig(file_or_dir);
    if (!f)
        return {};
    return read_file(f.value());*/
}

}
