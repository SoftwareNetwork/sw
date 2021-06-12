// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "compiler.h"

namespace sw
{

// win resources
struct SW_DRIVER_CPP_API RcTool :
    CompilerBaseProgram,
    CommandLineOptions<RcToolOptions>
{
    using CompilerBaseProgram::CompilerBaseProgram;

    SW_COMMON_COMPILER_API;
};

}
