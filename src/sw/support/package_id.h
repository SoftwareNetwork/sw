// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>

#pragma once

//#include "package_name.h"
//#include "settings.h"

#include <string>

namespace sw
{

// use more bits?
struct settings_hash {
    uint64_t h = 0;

    settings_hash() {}
    settings_hash(uint64_t h) : h(h) {}

    //operator uint64_t() const { return h; }
    operator uint64_t() const {
        //static_assert(sizeof(size_t) >= sizeof(h));
        return h;
    }
    operator std::string() const {
        return std::to_string(h).substr(0, 6);
    }
    auto to_string() const {
        return operator std::string();
    }
};

}
