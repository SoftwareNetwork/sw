// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "jumppad.h"

#include <primitives/exceptions.h>
#include <primitives/preprocessor.h>

#include <boost/dll.hpp>

namespace sw
{

int jumppad_call(const path &module, const String &name, int version, const Strings &s)
{
    auto n = STRINGIFY(SW_JUMPPAD_PREFIX) + name;
    //n += "_" + std::to_string(version);
    boost::dll::shared_library lib(module.u8string(),
        boost::dll::load_mode::rtld_now | boost::dll::load_mode::rtld_global);
    return lib.get<int(const Strings &)>(n.c_str())(s);
}

int jumppad_call(const Strings &s)
{
    int i = 3;
    if (s.size() < i++)
        throw SW_RUNTIME_ERROR("No module name was provided");
    if (s.size() < i++)
        throw SW_RUNTIME_ERROR("No function name was provided");
    if (s.size() < i++)
        throw SW_RUNTIME_ERROR("No function version was provided");
    // converting version to int is doubtful, but might help in removing leading zeroes (0002)
    return jumppad_call(s[2], s[3], std::stoi(s[4]), Strings{s.begin() + 5, s.end()});
}

}
