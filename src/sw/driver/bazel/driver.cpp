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

#include "driver.h"

#include <primitives/exceptions.h>

#include <algorithm>
#include <fstream>
#include <sstream>

int ll_bazellex_init(void **scanner);
int ll_bazellex_destroy(void *yyscanner);
struct yy_buffer_state *ll_bazel_scan_string(const char *yy_str, void *yyscanner);
yy_bazel::parser::symbol_type ll_bazellex(void *yyscanner, yy_bazel::location &loc);

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
        throw SW_RUNTIME_ERROR("Error during bazel parse: " + ss.str());
}

void BazelParserDriver::error(const std::string& m)
{
    std::ostringstream ss;
    ss << m << "\n";
    if (!can_throw)
        std::cerr << ss.str();
    else
        throw SW_RUNTIME_ERROR("Error during bazel parse: " + ss.str());
}
