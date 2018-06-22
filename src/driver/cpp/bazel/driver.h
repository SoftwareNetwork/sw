// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <string>
#include <vector>

#include "bazel.h"

#include <grammar.yy.hpp>

class BazelParserDriver
{
    enum class Mode
    {
        String,
        Tokens,
    };

public:
    bazel::File bazel_file;
    bool debug = false;
    bool can_throw = true;

    BazelParserDriver();

    yy_bazel::parser::symbol_type lex();
    int parse(const std::string &s);

    void error(const yy_bazel::location &l, const std::string &m);
    void error(const std::string &m);

    // lex & parse
private:
    void *scanner;
    yy_bazel::location location;
    Mode parseMode;

    int parse();
};
