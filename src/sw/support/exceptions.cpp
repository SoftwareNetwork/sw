// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "exceptions.h"

using namespace std::string_literals;

ExceptionVector::ExceptionVector(const std::vector<std::exception_ptr> &v)
    : v(v)
{
}

const char *ExceptionVector::what() const noexcept
{
    if (!s.empty())
        return s.c_str();
    for (auto &e : v)
    {
        try { std::rethrow_exception(e); }
        catch (std::exception &e2) { s += e2.what() + "\n"s; }
        catch (...) { s += "Unhandled exception\n"; }
    }
    return s.c_str();
}
