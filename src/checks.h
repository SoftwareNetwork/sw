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

#include "common.h"
#include "yaml.h"

#include <boost/variant.hpp>

struct CheckData
{
    int value = 0;
};

struct CheckFunction : public CheckData {};
struct CheckInclude : public CheckData {};
struct CheckType : public CheckData {};
struct CheckSymbol : public CheckData {};
struct CheckLibrary : public CheckData {};
struct CheckCSourceCompiles : public CheckData {};
struct CheckCSourceRuns : public CheckData {};
struct CheckCXXSourceCompiles : public CheckData {};
struct CheckCXXSourceRuns : public CheckData {};
struct CheckCustom : public CheckData {};

using Check = boost::variant<
    CheckFunction,
    CheckInclude,
    CheckType,
    CheckSymbol,
    CheckLibrary,
    CheckCSourceCompiles,
    CheckCSourceRuns,
    CheckCXXSourceCompiles,
    CheckCXXSourceRuns,
    CheckCustom
>;

struct Checks
{
    StringSet functions;
    StringSet includes;
    StringSet types;
    StringSet libraries;
    Symbols symbols;
    StringMap c_source_compiles;
    StringMap c_source_runs;
    StringMap cxx_source_compiles;
    StringMap cxx_source_runs;
    StringMap custom;

    bool empty() const;
    void load(const yaml &root);
    void merge(const Checks &rhs);
};
