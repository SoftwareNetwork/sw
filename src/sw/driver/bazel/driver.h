/*
 * SW - Build System and Package Manager
 * Copyright (C) 2016-2020 Egor Pugin
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
