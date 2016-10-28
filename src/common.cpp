/*
 * Copyright (c) 2016, Egor Pugin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *     3. Neither the name of the copyright holder nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "common.h"

#include "command.h"
#include "stamp.h"

#include <boost/algorithm/string.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <fstream>
#include <random>
#include <regex>

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
    boost::posix_time::ptime t(boost::gregorian::date(1970, 1, 1));
    t += boost::posix_time::seconds(static_cast<long>(std::stoi(cppan_stamp)));
    return prog_name + " version " + get_program_version() + "\n" +
        "assembled " + boost::posix_time::to_simple_string(t);
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
    char dest[PATH_MAX];
    if (readlink("/proc/self/exe", dest, PATH_MAX) == -1)
    {
        perror("readlink");
        throw std::runtime_error("Cannot get program path");
    }
    return dest;
#endif
}

std::vector<String> split_lines(const String &s)
{
    std::vector<String> v, lines;
    boost::split(v, s, boost::is_any_of("\r\n"));
    for (auto &l : v)
    {
        boost::trim(l);
        if (!l.empty())
            lines.push_back(l);
    }
    return lines;
}

String get_cmake_version()
{
    static const auto err = "Cannot get cmake version";
    static const std::regex r("cmake version (\\S+)");

    auto ret = command::execute_and_capture({ "cmake", "--version" });
    if (ret.rc != 0)
        throw std::runtime_error(err);

    std::smatch m;
    if (std::regex_search(ret.out, m, r))
    {
        if (m[0].first != ret.out.begin())
            throw std::runtime_error(err);
        return m[1].str();
    }

    throw std::runtime_error(err);
}
