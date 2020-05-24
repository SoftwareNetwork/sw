// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2016-2018 Egor Pugin

#include "stamp.h"

const std::string cppan_stamp =
#ifdef CPPAN_INCLUDE_ASSEMBLED
#include <stamp.h.in>
#else
""
#endif
;
