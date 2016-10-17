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

#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <stdint.h>
#include <tuple>
#include <vector>
#include <unordered_set>

#include "enums.h"
#include "filesystem.h"
#include "property_tree.h"

#define CONFIG_ROOT "/etc/cppan/"
#define CPPAN_FILENAME "cppan.yml"

using String = std::string;
using Strings = std::vector<String>;

bool check_branch_name(const String &n, String *error = nullptr);
bool check_filename(const String &n, String *error = nullptr);

path get_program();
String get_program_version();
String get_program_version_string(const String &prog_name);

std::vector<String> split_lines(const String &s);

String repeat(const String &e, int n);

// lambda overloads
template <class... Fs> struct overload_set;

template <class F1, class... Fs>
struct overload_set<F1, Fs...> : F1, overload_set<Fs...>::type
{
    typedef overload_set type;

    overload_set(F1 head, Fs... tail)
        : F1(head), overload_set<Fs...>::type(tail...)
    {}

    using F1::operator();
    using overload_set<Fs...>::type::operator();
};

template <class F>
struct overload_set<F> : F
{
    typedef F type;
    using F::operator();
};

template <class... Fs>
typename overload_set<Fs...>::type overload(Fs... x)
{
    return overload_set<Fs...>(x...);
}

// time of operation
template <typename F, typename ... Args>
auto get_time(F &&f, Args && ... args)
{
    using namespace std::chrono;

    auto t0 = high_resolution_clock::now();
    std::forward<F>(f)(std::forward<Args...>(args)...);
    auto t1 = high_resolution_clock::now();
    return t1 - t0;
}

template <typename F, typename ... Args>
auto get_time_seconds(F &&f, Args && ... args)
{
    using namespace std::chrono;

    auto t = get_time(std::forward<F>(f), std::forward<Args...>(args)...);
    return std::chrono::duration_cast<std::chrono::seconds>(t).count();
}
