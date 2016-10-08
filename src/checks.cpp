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

void Checks::load(const yaml &root)
{
    auto load_string_set = [&root](auto &a, auto &&str)
    {
        auto s = get_sequence<String>(root, str);
        a.insert(s.begin(), s.end());
    };

    auto load_string_map = [&root](auto &a, auto &&str)
    {
        get_map_and_iterate(root, str, [&a](const auto &v)
        {
            auto f = v.first.template as<String>();
            auto s = v.second.template as<String>();
            a[f] = s;
        });
    };

    load_string_set(functions, "check_function_exists");
    load_string_set(includes, "check_include_exists");
    load_string_set(types, "check_type_size");
    load_string_set(libraries, "check_library_exists");

    // add some common types
    types.insert("size_t");
    types.insert("void *");

    get_map_and_iterate(root, "check_symbol_exists", [this](const auto &root)
    {
        auto f = root.first.template as<String>();
        auto s = root.second.template as<String>();
        if (root.second.IsSequence())
            symbols[f] = get_sequence_set<String>(root.second);
        else if (root.second.IsScalar())
            symbols[f].insert(s);
        else
            throw std::runtime_error("Symbol headers should be a scalar or a set");
    });

#define LOAD_MAP(x) load_string_map(x, "check_" ## #x)
    LOAD_MAP(c_source_compiles);
    LOAD_MAP(c_source_runs);
    LOAD_MAP(cxx_source_compiles);
    LOAD_MAP(cxx_source_runs);
#undef LOAD_MAP
    load_string_map(custom, "checks");
}

bool Checks::empty() const
{
#define CHECK_ACTION(array) && array.empty()
    return 1
#include "checks.inl"
        ;
#undef CHECK_ACTION
}

void Checks::merge(const Checks &rhs)
{
#define CHECK_ACTION(array) array.insert(rhs.array.begin(), rhs.array.end());
#include "checks.inl"
#undef CHECK_ACTION
}
