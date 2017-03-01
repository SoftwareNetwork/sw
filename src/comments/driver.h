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

#pragma once

#include <string>
#include <vector>

#include <comments/grammar.hpp>

class CommentsParserDriver
{
public:
    std::vector<std::string> comments;
    bool debug = false;
    bool can_throw = true;
    bool silent = true;

    CommentsParserDriver();

    yy_comments::parser::symbol_type lex();
    int parse(const std::string &s);

    void error(const yy_comments::location &l, const std::string &m);
    void error(const std::string &m);

    // lex & parse
private:
    void *scanner;
    yy_comments::location location;

    int parse();
};
