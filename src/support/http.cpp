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

#include "http.h"

bool isValidSourceUrl(const String &url)
{
    if (url.empty())
        return false;
    if (!isUrl(url))
        return false;
    if (url.find_first_of(R"bbb('"`\|;$ @!#^*()<>[],)bbb") != url.npos)
        return false;
    // remove? will fail: ssh://name:pass@web.site
    if (std::count(url.begin(), url.end(), ':') > 1)
        return false;
    if (url.find("&&") != url.npos)
        return false;
#ifndef CPPAN_TEST
    if (url.find("file:") == 0) // prevent loading local files
        return false;
#endif
    for (auto &c : url)
    {
        if (c < 0 || c > 127)
            return false;
    }
    return true;
}

void checkSourceUrl(const String &url)
{
    if (!isValidSourceUrl(url))
        throw std::runtime_error("Bad source url: " + url);
}
