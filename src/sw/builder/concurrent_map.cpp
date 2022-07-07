/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "concurrent_map.h"

namespace sw
{

//SW_DEFINE_GLOBAL_STATIC_FUNCTION(ConcurrentContext, getConcurrentContext)

ConcurrentContext createConcurrentContext()
{
    return junction::DefaultQSBR.createContext();
}

void destroyConcurrentContext(ConcurrentContext ctx)
{
    junction::DefaultQSBR.destroyContext(ctx);
}

void updateConcurrentContext(ConcurrentContext ctx)
{
    // Update the QSBR context for this thread.
    // In a larger application, this should be called periodically, for each thread, at a moment
    // when the thread is quiescent – that is, not in the middle of any operation that uses a
    // Junction data structure.
    junction::DefaultQSBR.update(ctx);
}

}
