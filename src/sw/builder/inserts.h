// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2017-2020 Egor Pugin

#include <string>

#define DECLARE_TEXT_VAR(x) extern const std::string x

DECLARE_TEXT_VAR(cppan_cpp);
DECLARE_TEXT_VAR(sql_db_local);

#undef DECLARE_TEXT_VAR
