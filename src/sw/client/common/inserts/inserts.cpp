/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2019 Egor Pugin
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

#include "../inserts.h"

#define DECLARE_TEXT_VAR_BEGIN(x) const uint8_t _##x[] = {
#define DECLARE_TEXT_VAR_END(x) }; const std::string x = (const char *)&_##x[0];

DECLARE_TEXT_VAR_BEGIN(sw_config_cmake)
#include <src/sw/client/common/inserts/SWConfig.cmake.emb>
DECLARE_TEXT_VAR_END(sw_config_cmake);

DECLARE_TEXT_VAR_BEGIN(project_templates)
#include <src/sw/client/common/inserts/project_templates.yml.emb>
DECLARE_TEXT_VAR_END(project_templates);

#undef DECLARE_TEXT_VAR_BEGIN
#undef DECLARE_TEXT_VAR_END
