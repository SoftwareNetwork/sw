// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#include "../commands.h"

#include <sw/builder_distributed/server.h>

SUBCOMMAND_DECL(server)
{
    if (getOptions().options_server.distributed_builder)
    {
        sw::builder::distributed::Server s;
        s.start(getOptions().options_server.endpoint);
        s.wait();
        // TODO: handle interrupts properly
        s.stop();
        return;
    }

    SW_UNIMPLEMENTED;
}
