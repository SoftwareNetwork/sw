// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2018-2020 Egor Pugin <egor.pugin@gmail.com>

#include <sw/core/inserts.h>

#define DECLARE_TEXT_VAR_BEGIN(x) const uint8_t _##x[] = {
#define DECLARE_TEXT_VAR_END(x) }; const std::string x = (const char *)&_##x[0];

DECLARE_TEXT_VAR_BEGIN(inputs_db_schema)
#include <src/sw/core/inserts/input_db_schema.sql.emb>
DECLARE_TEXT_VAR_END(inputs_db_schema);

DECLARE_TEXT_VAR_BEGIN(html_template_build)
#include <src/sw/core/inserts/build.html.emb>
DECLARE_TEXT_VAR_END(html_template_build);

DECLARE_TEXT_VAR_BEGIN(render_py)
#include <src/sw/core/inserts/render.py.emb>
DECLARE_TEXT_VAR_END(render_py);

#undef DECLARE_TEXT_VAR_BEGIN
#undef DECLARE_TEXT_VAR_END
