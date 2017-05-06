/*
 * Copyright (C) 2016-2017, Egor Pugin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "context.h"

void CMakeContext::if_(const String &s)
{
    addLine("if (" + s + ")");
    increaseIndent();
}

void CMakeContext::elseif(const String &s)
{
    decreaseIndent();
    emptyLines(0);
    addLine("elseif(" + s + ")");
    increaseIndent();
}

void CMakeContext::else_()
{
    decreaseIndent();
    emptyLines(0);
    addLine("else()");
    increaseIndent();
}

void CMakeContext::endif()
{
    decreaseIndent();
    emptyLines(0);
    addLine("endif()");
}
