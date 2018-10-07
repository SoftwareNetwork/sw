// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "jumppad.h"

#include <boost/dll.hpp>

namespace sw
{

int jumppad_call(const path &module, const String &name, const Strings &s)
{
    auto n = "_sw_fn_jumppad_" + name;
    boost::dll::shared_library lib(module.u8string());
    return lib.get<int(const Strings &)>(n.c_str())(s);
}

int jumppad_call(const Strings &s)
{
    if (s.empty())
        throw std::runtime_error("No function name was provided");
    return jumppad_call(s[0], s[1], Strings{s.begin() + 2, s.end()});
}

}
