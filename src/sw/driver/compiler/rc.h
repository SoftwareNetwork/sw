// SPDX-License-Identifier: GPL-3.0-or-later
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
    FilesOrdered idirs;

    using CompilerBaseProgram::CompilerBaseProgram;

    SW_COMMON_COMPILER_API;

    void setOutputFile(const path &output_file);
    void setSourceFile(const path &input_file);
};

}
