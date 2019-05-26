// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <gitrev.h>

#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>

std::string getBuildTime()
{
    std::tm tm = {};
    std::stringstream ss(__DATE__ " " __TIME__);
    ss >> std::get_time(&tm, "%b %d %Y %H:%M:%S");
    auto t = mktime(&tm);
    auto utc = std::chrono::system_clock::from_time_t(t) - std::chrono::hours{ 3 }; // +3 TZ
    auto t2 = std::chrono::system_clock::to_time_t(utc);
    auto tm2 = localtime(&t2);
    std::ostringstream ss2;
    ss2 << std::put_time(tm2, "%d.%m.%Y %H:%M:%S");
    return ss2.str() + " UTC";
}

std::string getGitRev()
{
    std::string gitrev = SW_GIT_REV;
    if (!gitrev.empty())
    {
        gitrev = "git revision " + gitrev;
        if (SW_GIT_CHANGED_FILES)
            gitrev += " plus " + std::to_string(SW_GIT_CHANGED_FILES) + " modified files";
        gitrev += "\n";
    }
    return gitrev;
}
