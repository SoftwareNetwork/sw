// Copyright (C) 2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "driver.h"

namespace sw
{

int64_t IDriver::getGroupNumber(const RawInput &i) const
{
    return std::hash<String>()(getSpecification(i));
}

} // namespace sw
