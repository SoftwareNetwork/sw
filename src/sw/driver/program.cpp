// Copyright (C) 2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "program.h"

namespace sw
{

Program &PredefinedProgram::getProgram()
{
    if (!program)
        throw SW_RUNTIME_ERROR("Program was not set");
    return *program;
}

const Program &PredefinedProgram::getProgram() const
{
    if (!program)
        throw SW_RUNTIME_ERROR("Program was not set");
    return *program;
}

}
