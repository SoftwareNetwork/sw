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
