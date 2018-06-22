// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "property_tree.h"

#include <iostream>

std::string ptree2string(const ptree &p)
{
    std::ostringstream oss;
    pt::write_json(oss, p, false);
    return oss.str();
}

ptree string2ptree(const std::string &s)
{
    ptree p;
    if (s.empty())
        return p;
    std::istringstream iss(s);
    pt::read_json(iss, p);
    return p;
}
