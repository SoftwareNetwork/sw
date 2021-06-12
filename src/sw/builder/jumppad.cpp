// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2017-2020 Egor Pugin

#include "jumppad.h"

#include <primitives/exceptions.h>
#include <primitives/preprocessor.h>

#include <boost/dll.hpp>

namespace sw
{

int jumppad_call(const path &module, const String &name, int version, const Strings &s)
{
    auto n = STRINGIFY(SW_JUMPPAD_PREFIX) + name;
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
