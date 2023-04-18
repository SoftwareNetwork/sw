// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>

#include "../inserts.h"

#include <stdint.h>

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
