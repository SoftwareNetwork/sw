// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "build.h"

#include "entry_point.h"
#include "frontend/cppan/yaml.h"
#include "functions.h"
#include "module.h"
#include "suffix.h"
#include "target/native.h"

#include <sw/builder/file_storage.h>
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

void ModuleSwappableData::addTarget(ITargetPtr p)
{
    added_targets.push_back(p);
}

ModuleSwappableData::AddedTargets &ModuleSwappableData::getTargets()
{
    if (post_actions_performed)
        return added_targets;

    for (auto &t : added_targets)
    {
        auto usual_target = t->as<Target *>();
        if (!usual_target)
            continue;
        usual_target->postConfigureActions();
    }
    post_actions_performed = true;
    return added_targets;
}

Build::Build(SwBuild &mb)
    : checker(std::make_shared<Checker>(mb))
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
        module_data.known_targets.contains(p);
}

path Build::getSourceDir(const LocalPackage &p) const
{
    return p.getDirSrc2();
}

std::optional<path> Build::getSourceDir(const Source &s, const Version &v) const
{
    auto s2 = s.clone();
    s2->applyVersion(v);
    if (dd)
    {
        auto i = dd->source_dirs_by_source.find(s2->getHash());
        if (i != dd->source_dirs_by_source.end())
            return i->second.getRequestedDirectory();
    }
    return {};
}

const TargetSettings &Build::getExternalVariables() const
{
    return getMainBuild().getExternalVariables();
}

}
