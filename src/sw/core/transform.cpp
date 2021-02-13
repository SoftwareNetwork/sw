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

void transform::add_driver(const PackageName &pkg, std::unique_ptr<IDriver> driver)
{
    auto [_, inserted] = drivers_.insert_or_assign(pkg, std::move(driver));
    if (inserted)
        LOG_TRACE(logger, "Registering driver: " + pkg.toString());
}

/*package_transform *transform::add_transform(package_loader &l, const PackageSettings &s)
{
    l.load(s);
}*/

std::vector<package_loader *> transform::load_packages(const path &p)
{
    std::vector<package_loader *> loaders;
    for (auto &[_, d] : drivers_) {
        for (auto &&p : d->load_packages(p)) {
            package_loaders.emplace_back(std::move(p));
            loaders.push_back(package_loaders.back().get());
        }
    }
    return loaders;
}

package_loader *transform::load_package(const Package &p)
{
    auto i = drivers_.find(p.getData().driver);
    if (i == drivers_.end())
        throw SW_RUNTIME_ERROR("Driver is not registered: " + p.getData().driver.toString());
    package_loaders.emplace_back(i->second->load_package(std::move(p)));
    return package_loaders.back().get();
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
