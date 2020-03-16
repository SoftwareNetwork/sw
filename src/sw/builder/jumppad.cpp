/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
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
