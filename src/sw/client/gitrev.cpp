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
    auto t = SW_BUILD_TIME_T;
    std::ostringstream ss2;
    ss2 << std::put_time(localtime(&t), "%d.%m.%Y %H:%M:%S %Z");
    return ss2.str();
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
