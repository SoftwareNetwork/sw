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
#include <bazel/lexer.h>
extern yy_bazel::parser::symbol_type ll_bazellex(yyscan_t yyscanner, yy_bazel::location &loc);

BazelParserDriver::BazelParserDriver()
{
}

yy_bazel::parser::symbol_type BazelParserDriver::lex()
{
	auto ret = ll_bazellex(scanner, location);
	return ret;
}

int BazelParserDriver::parse(const std::string &s)
{
    parseMode = Mode::String;

    ll_bazellex_init(&scanner);
    ll_bazel_scan_string(s.c_str(), scanner);
    auto res = parse();
    ll_bazellex_destroy(scanner);

    return res;
}

int BazelParserDriver::parse()
{
    yy_bazel::parser parser(*this);
    parser.set_debug_level(debug);
    int res = parser.parse();
    return res;
}

void BazelParserDriver::error(const yy_bazel::location &l, const std::string &m)
{
    std::ostringstream ss;
    ss << l << " " << m << "\n";
    if (!can_throw)
        std::cerr << ss.str();
    else
        throw std::runtime_error("Error during bazel parse: " + ss.str());
}

void BazelParserDriver::error(const std::string& m)
{
    std::ostringstream ss;
    ss << m << "\n";
    if (!can_throw)
        std::cerr << ss.str();
    else
        throw std::runtime_error("Error during bazel parse: " + ss.str());
}
