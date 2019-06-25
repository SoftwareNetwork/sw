/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2019 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

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
