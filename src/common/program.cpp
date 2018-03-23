/*
 * Copyright (C) 2016-2017, Egor Pugin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "program.h"

#include "stamp.h"

#include <primitives/command.h>

#include <iomanip>
#include <regex>
#include <sstream>

#ifdef WIN32
#include <windows.h>
#endif

#ifdef __APPLE__
#include <libproc.h>
#include <unistd.h>
#endif

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

path get_program()
{
#ifdef _WIN32
    WCHAR fn[8192] = { 0 };
    GetModuleFileNameW(NULL, fn, sizeof(fn) * sizeof(WCHAR));
    return fn;
#elif __APPLE__
    auto pid = getpid();
    char dest[PROC_PIDPATHINFO_MAXSIZE] = { 0 };
    auto ret = proc_pidpath(pid, dest, sizeof(dest));
    if (ret <= 0)
        throw std::runtime_error("Cannot get program path");
    return dest;
#else
    char dest[PATH_MAX] = { 0 };
    if (readlink("/proc/self/exe", dest, PATH_MAX) == -1)
    {
        perror("readlink");
        throw std::runtime_error("Cannot get program path");
    }
    return dest;
#endif
}

String get_cmake_version()
{
    static const auto err = "Cannot get cmake version. Do you have cmake added to PATH?";
    static const std::regex r("cmake version (\\S+)");

    primitives::Command c;
    c.program = "cmake";
    c.args = { "--version" };
    std::error_code ec;
    c.execute(ec);
    if (ec)
        throw std::runtime_error(err);

    std::smatch m;
    if (std::regex_search(c.out.text, m, r))
    {
        if (m[0].first != c.out.text.begin())
            throw std::runtime_error(err);
        return m[1].str();
    }

    throw std::runtime_error(err);
}
