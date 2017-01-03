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

#include "cppan_string.h"

#include <boost/algorithm/string.hpp>

Strings split_string(const String &s, const String &delims)
{
    std::vector<String> v, lines;
    boost::split(v, s, boost::is_any_of(delims));
    for (auto &l : v)
    {
        boost::trim(l);
        if (!l.empty())
            lines.push_back(l);
    }
    return lines;
}

Strings split_lines(const String &s)
{
    return split_string(s, "\r\n");
}

int get_end_of_string_block(const String &s, int i)
{
    auto c = s[i - 1];
    int n_curly = c == '(';
    int n_square = c == '[';
    int n_quotes = c == '\"';
    auto sz = (int)s.size();
    while ((n_curly > 0 || n_square > 0 || n_quotes > 0) && i < sz)
    {
        c = s[i];

        if (c == '\"')
        {
            if (n_quotes == 0)
                i = get_end_of_string_block(s.c_str(), i + 1) - 1;
            else if (s[i - 1] == '\\')
                ;
            else
                n_quotes--;
        }
        else
        {
            switch (c)
            {
            case '(':
            case '[':
                i = get_end_of_string_block(s.c_str(), i + 1) - 1;
                break;
            case ')':
                n_curly--;
                break;
            case ']':
                n_square--;
                break;
            }
        }

        i++;
    }
    return i;
}

#ifdef _WIN32
void normalize_string(String &s)
{
    std::replace(s.begin(), s.end(), '\\', '/');
}

String normalize_string_copy(String s)
{
    normalize_string(s);
    return s;
}
#endif

String trim_double_quotes(String s)
{
    boost::trim(s);
    while (!s.empty())
    {
        if (s.front() == '"')
        {
            s = s.substr(1);
            continue;
        }
        if (s.back() == '"')
        {
            s.resize(s.size() - 1);
            continue;
        }
        break;
    }
    boost::trim(s);
    return s;
}
