// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>

#include "stamp.h"

const std::string cppan_stamp =
#ifdef CPPAN_INCLUDE_ASSEMBLED
#include <stamp.h.in>
#else
""
#endif
;
