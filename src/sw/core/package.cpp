// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2021 Egor Pugin <egor.pugin@gmail.com>

#include "package.h"

#include <sw/builder/execution_plan.h>

#include <primitives/executor.h>

namespace sw
{

package_loader::~package_loader() = default;
physical_package::~physical_package() = default;
package_transform::~package_transform() = default;

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
