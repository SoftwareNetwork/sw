// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2018-2020 Egor Pugin <egor.pugin@gmail.com>

#include <sw/core/inserts.h>

#define DECLARE_TEXT_VAR_BEGIN(x) const uint8_t _##x[] = {
#define DECLARE_TEXT_VAR_END(x) }; const std::string x = (const char *)&_##x[0];

DECLARE_TEXT_VAR_BEGIN(inputs_db_schema)
#include <src/sw/core/inserts/input_db_schema.sql.emb>
DECLARE_TEXT_VAR_END(inputs_db_schema);

#undef DECLARE_TEXT_VAR_BEGIN
#undef DECLARE_TEXT_VAR_END
