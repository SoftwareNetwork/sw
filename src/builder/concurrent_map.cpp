// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
