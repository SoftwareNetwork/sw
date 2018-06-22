// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <string>

#define DECLARE_TEXT_VAR(x) extern const std::string x

DECLARE_TEXT_VAR(cppan_cpp);
DECLARE_TEXT_VAR(sql_db_local);

#undef DECLARE_TEXT_VAR
