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

#pragma once

#ifdef BOOST_USE_WINDOWS_H
#undef BOOST_USE_WINDOWS_H
#define REDEFINE_BOOST_USE_WINDOWS_H
#endif

#ifdef __linux__
#define BOOST_STACKTRACE_USE_BACKTRACE
#endif
#include <boost/stacktrace.hpp>

#include <boost/exception/all.hpp>

#ifdef REDEFINE_BOOST_USE_WINDOWS_H
#define BOOST_USE_WINDOWS_H
#endif

#include <exception>

#define TYPED_EXCEPTION(x)                       \
    struct x : public std::runtime_error         \
    {                                            \
        using std::runtime_error::runtime_error; \
        x() : runtime_error("") {}               \
    }

using traced_exception = boost::error_info<struct tag_stacktrace, boost::stacktrace::stacktrace>;

template <class E>
void throw_with_trace(const E &e)
{
    boost::stacktrace::stacktrace t
#ifdef _WIN32
        (2, -1)
#else
        (1, -1)
#endif
;
    throw boost::enable_error_info(e) << traced_exception(t);
}
