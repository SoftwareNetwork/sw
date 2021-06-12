// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2017-2020 Egor Pugin

#include "concurrent_map.h"

namespace sw
{

SW_DEFINE_GLOBAL_STATIC_FUNCTION(ConcurrentContext, getConcurrentContext)

ConcurrentContext createConcurrentContext()
{
    return junction::DefaultQSBR.createContext();
}

void destroyConcurrentContext(ConcurrentContext ctx)
{
    junction::DefaultQSBR.destroyContext(ctx);
}

void updateConcurrentContext()
{
    // Update the QSBR context for this thread.
    // In a larger application, this should be called periodically, for each thread, at a moment
    // when the thread is quiescent – that is, not in the middle of any operation that uses a
    // Junction data structure.
    junction::DefaultQSBR.update(getConcurrentContext());
}

}
