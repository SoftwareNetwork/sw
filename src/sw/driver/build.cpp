/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
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

#include "entry_point.h"
#include "frontend/cppan/yaml.h"
#include "functions.h"
#include "module.h"
#include "suffix.h"
#include "sw_abi_version.h"
#include "target/native.h"

#include <sw/builder/file_storage.h>
#include <sw/builder/program.h>
#include <sw/core/input.h>
#include <sw/core/sw_context.h>
#include <sw/manager/database.h>
#include <sw/manager/settings.h>
#include <sw/manager/storage.h>

#include <boost/algorithm/string.hpp>
#include <boost/range/combine.hpp>
#include <boost/thread/lock_types.hpp>
#include <primitives/emitter.h>
#include <primitives/date_time.h>
#include <primitives/executor.h>
#include <primitives/pack.h>
#include <primitives/symbol.h>
#include <primitives/templates.h>
#include <primitives/sw/settings_program_name.h>

#include <boost/dll.hpp>
#include <nlohmann/json.hpp>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "build");

namespace sw
{

static void sw_check_abi_version(int v)
{
    if (v > SW_MODULE_ABI_VERSION)
        throw SW_RUNTIME_ERROR("Module ABI (" + std::to_string(v) + ") is greater than binary ABI (" + std::to_string(SW_MODULE_ABI_VERSION) + "). Update your sw binary.");
    if (v < SW_MODULE_ABI_VERSION)
        throw SW_RUNTIME_ERROR("Module ABI (" + std::to_string(v) + ") is less than binary ABI (" + std::to_string(SW_MODULE_ABI_VERSION) + "). Update sw driver headers (or ask driver maintainer).");
}

Build::Build(SwBuild &mb)
    : checker(mb)
{
    main_build_ = &mb;
}

// can be used in configs to load subdir configs
// s.build->loadModule("client/sw.cpp").call<void(Solution &)>("build", s);
/*Module Build::loadModule(const path &p) const
{
    auto fn2 = p;
    if (!fn2.is_absolute())
        fn2 = SourceDir / fn2;
    // driver->build_cpp_spec(swctx, p);
    //return getContext().getModuleStorage().get(dll);
}*/

bool Build::isKnownTarget(const LocalPackage &p) const
{
    return module_data.known_targets.empty() ||
        p.getPath().is_loc() || // used by cfg targets and checks
        module_data.known_targets.find(p) != module_data.known_targets.end();
}

path Build::getSourceDir(const LocalPackage &p) const
{
    return p.getDirSrc2();
}

std::optional<path> Build::getSourceDir(const Source &s, const Version &v) const
{
    auto s2 = s.clone();
    s2->applyVersion(v);
    auto i = source_dirs_by_source.find(s2->getHash());
    if (i != source_dirs_by_source.end())
        return i->second.getRequestedDirectory();
    return {};
}

const TargetSettings &Build::getExternalVariables() const
{
    return getMainBuild().getExternalVariables();
}

}
