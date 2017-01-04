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

#include <map>
#include <set>
#include <string>
#include <vector>

using String = std::string;
using Strings = std::vector<String>;
using StringMap = std::map<String, String>;
using StringSet = std::set<String>;

using namespace std::literals;

Strings split_string(const String &s, const String &delims);
Strings split_lines(const String &s);
String trim_double_quotes(String s);

int get_end_of_string_block(const String &s, int i = 1);

#ifdef _WIN32
void normalize_string(String &s);
String normalize_string_copy(String s);
#else
#define normalize_string(s) (s)
#define normalize_string_copy(s) (s)
#endif
