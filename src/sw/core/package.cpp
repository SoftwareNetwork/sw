// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "package.h"

namespace sw
{

package_loader::~package_loader() = default;
physical_package::~physical_package() = default;
package_transform::~package_transform() = default;

void transform_executor::execute(const std::vector<const package_transform *> &)
{
    SW_UNIMPLEMENTED;
}

}
