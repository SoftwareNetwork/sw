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

#include "driver.h"

#include <algorithm>
#include <fstream>
#include <sstream>

// Prevent using <unistd.h> because of bug in flex.
#define YY_NO_UNISTD_H 1
#define YY_DECL 1
#include <lexer.h>
extern yy_comments::parser::symbol_type ll_commentslex(yyscan_t yyscanner, yy_comments::location &loc);

std::vector<std::string> extract_comments(const std::string &s)
{
    CommentsParserDriver driver;
    driver.parse(s);
    return driver.comments;
}

CommentsParserDriver::CommentsParserDriver()
{
}

yy_comments::parser::symbol_type CommentsParserDriver::lex()
{
	auto ret = ll_commentslex(scanner, location);
	return ret;
}

int CommentsParserDriver::parse(const std::string &s)
{
    ll_commentslex_init(&scanner);
    ll_comments_scan_string(s.c_str(), scanner);
    auto res = parse();
    ll_commentslex_destroy(scanner);

    return res;
}

int CommentsParserDriver::parse()
{
    yy_comments::parser parser(*this);
    parser.set_debug_level(debug);
    int res = parser.parse();
    return res;
}

void CommentsParserDriver::error(const yy_comments::location &l, const std::string &m)
{
    if (silent)
        return;
    std::ostringstream ss;
    ss << l << " " << m << "\n";
    if (!can_throw)
        std::cerr << ss.str();
    else
        throw std::runtime_error("Error during parse: " + ss.str());
}

void CommentsParserDriver::error(const std::string& m)
{
    if (silent)
        return;
    std::ostringstream ss;
    ss << m << "\n";
    if (!can_throw)
        std::cerr << ss.str();
    else
        throw std::runtime_error("Error during parse: " + ss.str());
}
