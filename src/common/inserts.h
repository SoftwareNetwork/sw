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

#include "cppan_string.h"

#define DECLARE_TEXT_VAR(x) extern const String x

DECLARE_TEXT_VAR(cppan_h);
DECLARE_TEXT_VAR(version_rc_in);
DECLARE_TEXT_VAR(branch_rc_in);
DECLARE_TEXT_VAR(cmake_functions);
DECLARE_TEXT_VAR(cmake_build_file);
DECLARE_TEXT_VAR(cmake_generate_file);
DECLARE_TEXT_VAR(cmake_export_import_file);
DECLARE_TEXT_VAR(cmake_header);
DECLARE_TEXT_VAR(cppan_cmake_config);

#undef DECLARE_TEXT_VAR
