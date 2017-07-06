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
            if (i && s[i - 1] == '\\')
                ;
            else if (n_quotes == 0)
                i = get_end_of_string_block(s, i + 1) - 1;
            else
                n_quotes--;
        }
        else
        {
            switch (c)
            {
            case '(':
            case '[':
                i = get_end_of_string_block(s, i + 1) - 1;
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
