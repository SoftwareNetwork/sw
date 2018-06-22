// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "http.h"

#include <algorithm>

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
