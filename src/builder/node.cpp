// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "node.h"

#include <sw/builder/command.h>

namespace sw
{

void Executable::execute() const
{
    if (auto c = getCommand())
        return c->execute();
}

}
