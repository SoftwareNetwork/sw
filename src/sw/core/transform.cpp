// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2021 Egor Pugin <egor.pugin@gmail.com>

#include "transform.h"

#include "driver.h"
#include "package.h"

#include <sw/builder/execution_plan.h>

#include <primitives/executor.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "transform");

namespace sw
{

transform::transform() {}
transform::~transform() {}

void transform::add_driver(IDriver &driver)
{
    auto [_, inserted] = drivers.insert_or_assign(driver.get_package().getName(), &driver);
    if (inserted)
        LOG_TRACE(logger, "Registering driver: " + driver.get_package().getName().toString());
}

/*package_transform *transform::add_transform(package_loader &l, const PackageSettings &s)
{
    l.load(s);
}*/

std::vector<package_loader *> transform::load_packages(const path &p)
{
    std::vector<package_loader *> loaders;
    for (auto &[_, d] : drivers) {
        for (auto p : d->load_packages(p)) {
            package_loaders.emplace(p->get_package_name(), p);
            loaders.push_back(p);
        }
    }
    return loaders;
}

package_loader *transform::load_package(const Package &p)
{
    auto it = package_loaders.find(p.getId().getName());
    if (it != package_loaders.end())
        return it->second;
    auto i = drivers.find(p.getData().driver);
    if (i == drivers.end())
        throw SW_RUNTIME_ERROR("Driver is not registered: " + p.getData().driver.toString());
    auto pl = i->second->load_package(p);
    package_loaders.emplace(pl->get_package_name(), pl);
    return pl;
}

void transform_executor::execute(const std::vector<const package_transform *> &v)
{
    Commands cmds;
    for (auto &t : v)
        cmds.merge(t->get_commands());

    auto ep = ExecutionPlan::create(cmds);
    if (!ep->isValid())
        SW_UNIMPLEMENTED;

    ep->execute(getExecutor());
}

}
