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

#pragma once

#include "cppan_string.h"
#include "filesystem.h"

namespace command
{

using Args = Strings;

struct Options
{
    struct Stream
    {
        bool capture = false;
        bool inherit = false;
    };

    Stream out;
    Stream err;
};

struct Result
{
    int rc;
    String out;
    String err;

    void write(path p) const;
};

Result execute(const Args &args, const Options &options = Options());
Result execute_with_output(const Args &args, const Options &options = Options());
Result execute_and_capture(const Args &args, const Options &options = Options());

} // namespace command

bool has_executable_in_path(String &exe, bool silent = false);
