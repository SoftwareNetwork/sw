// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <sw/driver/inserts.h>

#define DECLARE_TEXT_VAR_BEGIN(x) const char _##x[] = {
#define DECLARE_TEXT_VAR_END(x) }; const std::string x = _##x;

DECLARE_TEXT_VAR_BEGIN(cppan_cpp)
#include <src/sw/driver/inserts/sw.cpp.emb>
DECLARE_TEXT_VAR_END(cppan_cpp);

#undef DECLARE_TEXT_VAR_BEGIN
#undef DECLARE_TEXT_VAR_END
