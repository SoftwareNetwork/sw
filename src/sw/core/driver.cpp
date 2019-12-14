// Copyright (C) 2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "driver.h"

#include <sw/support/hash.h>

namespace sw
{

void Specification::addFile(const path &relative_path, const String &contents)
{
    files[relative_path] = contents;
}

int64_t Specification::getHash() const
{
    size_t h = 0;
    for (auto &[f, s] : files)
        hash_combine(h, s);
    return h;
}

int64_t IDriver::getGroupNumber(const RawInput &i) const
{
    return getSpecification(i)->getHash();
}

} // namespace sw
