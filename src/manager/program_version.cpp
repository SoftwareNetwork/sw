// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "program_version.h"

#include "stamp.h"

#include <iomanip>
#include <sstream>

String get_program_version()
{
    String s;
    s +=
        std::to_string(VERSION_MAJOR) + "." +
        std::to_string(VERSION_MINOR) + "." +
        std::to_string(VERSION_PATCH);
    return s;
}

String get_program_version_string(const String &prog_name)
{
    auto t = static_cast<time_t>(std::stoll(cppan_stamp));
    auto tm = localtime(&t);
    std::ostringstream ss;
    ss << prog_name << " version " << get_program_version() << "\n" <<
        "assembled " << std::put_time(tm, "%F %T");
    return ss.str();
}
