// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#ifdef BOOST_USE_WINDOWS_H
#undef BOOST_USE_WINDOWS_H
#define REDEFINE_BOOST_USE_WINDOWS_H
#endif

#include <boost/stacktrace.hpp>
#include <boost/exception/errinfo_errno.hpp>

#ifdef REDEFINE_BOOST_USE_WINDOWS_H
#define BOOST_USE_WINDOWS_H
#endif

#include <exception>

#define TYPED_EXCEPTION_WITH_STD_PARENT(x, p) \
    struct x : public std::p                  \
    {                                         \
        using std::p::p;                      \
        x() : p("") {}                        \
    }

#define TYPED_EXCEPTION(x) TYPED_EXCEPTION_WITH_STD_PARENT(x, runtime_error)

using traced_exception = boost::error_info<struct tag_stacktrace, boost::stacktrace::stacktrace>;

template <class E>
void throw_with_trace(const E &e)
{
    boost::stacktrace::stacktrace t(2, -1);
    throw boost::enable_error_info(e) << traced_exception(t);
}

struct SW_SUPPORT_API ExceptionVector : std::exception
{
    ExceptionVector(const std::vector<std::exception_ptr> &v);
    ExceptionVector(const ExceptionVector &v) = default;
    virtual ~ExceptionVector() = default;

    const char *what() const noexcept override;

private:
    std::vector<std::exception_ptr> v;
    mutable std::string s;
};
