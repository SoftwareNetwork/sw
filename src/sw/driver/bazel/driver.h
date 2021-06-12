// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2016-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "bazel.h"

#include <grammar.yy.hpp>

#include <string>
#include <vector>

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
