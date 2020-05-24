// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>

#include "../commands.h"

SUBCOMMAND_DECL(configure)
{
    auto b = createBuildAndPrepare({ getOptions().options_configure.build_arg_configure });
    b->saveExecutionPlan();
}
