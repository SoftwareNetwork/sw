// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>

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
    s += "Total errors: " + std::to_string(v.size()) + "\n";
    return s.c_str();
}
