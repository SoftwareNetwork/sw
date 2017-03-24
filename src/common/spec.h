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
#include "package.h"
#include "source.h"

#include <primitives/date_time.h>

#define SPEC_FILE_EXTENSION ".cppan"

struct Specification
{
    Package package;
    Source source;
    String cppan;
    String hash;
    TimePoint created;
};

Specification download_specification(const Package &pkg);
Specification read_specification(const String &spec);
Specification read_specification(const ptree &spec);
