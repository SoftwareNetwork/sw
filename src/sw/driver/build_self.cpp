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

#include "entry_point.h"

#include <sw/core/sw_context.h>
#include <sw/core/target.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "build.self");

#define SW_PACKAGE_API
#include "sw.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4005) // warning C4005: 'XXX': macro redefinition
#endif

using TargetEntryPointMap = std::unordered_map<sw::PackageId, std::shared_ptr<sw::NativeBuiltinTargetEntryPoint>>;

#define SW_DRIVER_ADD_SELF
#include <build_self.generated.h>

namespace sw
{

PackageIdSet load_builtin_packages(SwContext &swctx)
{
    static const UnresolvedPackages required_packages
    {
#include <build_self.packages.generated.h>
    };

    // create entry points by package
    //auto epm = build_self_generated();
    //for (auto &[gn, ep] : epm)
        //swctx.setEntryPoint(gn, ep); // still set

    //
    auto m = swctx.install(required_packages);

    PackageIdSet builtin_packages;
    for (auto &p : required_packages)
        builtin_packages.insert(m.find(p)->second);
    return builtin_packages;
}

} // namespace sw
