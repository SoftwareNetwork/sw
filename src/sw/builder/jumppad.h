// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <primitives/exceptions.h>
#include <primitives/filesystem.h>

#include <gsl/span>

#include <functional>

#ifdef SW_PACKAGE_API
#define SW_JUMPPAD_API SW_PACKAGE_API
#else
#define SW_JUMPPAD_API SW_BUILDER_API
#endif

#define SW_JUMPPAD_PREFIX _sw_fn_jumppad_
#define SW_JUMPPAD_DEFAULT_FUNCTION_VERSION 0

// strict macro
// M(visible name, function name in code, ...)
// M2(function name in code, ...) // visible name will be the same as function name
// M3(function name in code, ...) // macro to use in different calls that accept builtin functions
#ifdef _MSC_VER
#define SW_DEFINE_VISIBLE_FUNCTION_JUMPPAD(n, f, ...)                                 \
    extern "C" SW_JUMPPAD_API int CONCATENATE(SW_JUMPPAD_PREFIX, n)(const Strings &s) \
    {                                                                                 \
        ::sw::VisibleFunctionJumppad j(&f, #n, __VA_ARGS__);                          \
        return j.call(s);                                                             \
    }
#define SW_DEFINE_VISIBLE_FUNCTION_JUMPPAD2(n, ...) SW_DEFINE_VISIBLE_FUNCTION_JUMPPAD(n, n, __VA_ARGS__)
#define SW_VISIBLE_FUNCTION(f, ...) #f, f, __VA_ARGS__
#else
#define SW_DEFINE_VISIBLE_FUNCTION_JUMPPAD(n, f, ...)                                 \
    extern "C" SW_JUMPPAD_API int CONCATENATE(SW_JUMPPAD_PREFIX, n)(const Strings &s) \
    {                                                                                 \
        ::sw::VisibleFunctionJumppad j(&f, #n, ##__VA_ARGS__);                        \
        return j.call(s);                                                             \
    }
#define SW_DEFINE_VISIBLE_FUNCTION_JUMPPAD2(n, ...) SW_DEFINE_VISIBLE_FUNCTION_JUMPPAD(n, n, ##__VA_ARGS__)
#define SW_VISIBLE_FUNCTION(f, ...) #f, f, ##__VA_ARGS__
#endif

namespace sw
{

namespace detail
{

template <class T>
inline T from_string(gsl::span<const String> &s)
{
    auto v = s[0];
    s = s.subspan(1);
    return v;
}

template <>
inline int from_string(gsl::span<const String> &s)
{
    auto v = s[0];
    s = s.subspan(1);
    return std::stoi(v);
}

template <>
inline int64_t from_string(gsl::span<const String> &s)
{
    auto v = s[0];
    s = s.subspan(1);
    return std::stoll(v);
}

template <>
inline Strings from_string(gsl::span<const String> &s)
{
    auto b = s.begin();
    Strings f;
    auto n = std::stoi(*b++);
    while (n--)
        f.push_back(*b++);
    s = s.subspan(b - s.begin());
    return f;
}

template <>
inline Files from_string(gsl::span<const String> &s)
{
    auto b = s.begin();
    Files f;
    auto n = std::stoi(*b++);
    while (n--)
        f.insert(*b++);
    s = s.subspan(b - s.begin());
    return f;
}

template <typename Tuple, std::size_t... I>
inline auto strings2tuple(gsl::span<const String> &s, std::index_sequence<I...>)
{
    return std::tuple{ from_string<std::tuple_element_t<I, Tuple>>(s)... };
}

template <class T>
inline size_t get_n_arg(gsl::span<const String> &s)
{
    s = s.subspan(1);
    return 1;
}

template <>
inline size_t get_n_arg<Strings>(gsl::span<const String> &s)
{
    try
    {
        auto n = std::stoi(*s.begin());
        s = s.subspan(n + 1);
    }
    catch (...)
    {
        // on invalid number, we consider this as zero
        return 0;
    }
    return 1;
}

template <>
inline size_t get_n_arg<Files>(gsl::span<const String> &s)
{
    try
    {
        auto n = std::stoi(*s.begin());
        s = s.subspan(n + 1);
    }
    catch (...)
    {
        // on invalid number, we consider this as zero
        return 0;
    }
    return 1;
}

template <class T, class ... ArgTypes>
inline size_t get_n_args2(gsl::span<const String> &s)
{
    if (s.empty())
        return 0;
    auto n = get_n_arg<T>(s);
    if constexpr (sizeof...(ArgTypes) > 0)
        n += get_n_args2<ArgTypes...>(s);
    return n;
}

template <class ... ArgTypes>
inline size_t get_n_args(gsl::span<const String> &s)
{
    if constexpr (sizeof...(ArgTypes) == 0)
        return 0;
    else
        return get_n_args2<ArgTypes...>(s);
}

}

template <class>
struct VisibleFunctionJumppad;

template <class R, class ... ArgTypes>
struct VisibleFunctionJumppad<R(ArgTypes...)>
{
    std::function<R(ArgTypes...)> f;
    String name;
    int version;

    VisibleFunctionJumppad(std::function<R(ArgTypes...)> f, const String &n, int v = SW_JUMPPAD_DEFAULT_FUNCTION_VERSION)
        : f(f), name(n), version(v)
    {}

    R call(const Strings &s = {})
    {
        auto sp = gsl::make_span(s);
        auto sp2 = sp; // need a copy!
        auto nargs = detail::get_n_args<ArgTypes...>(sp2);
        if (sizeof...(ArgTypes) != nargs)
        {
            throw SW_RUNTIME_ERROR("pf call: " + name + ", version: " + std::to_string(version) + ": incorrect number of arguments " +
                std::to_string(nargs) + ", expected " + std::to_string(sizeof...(ArgTypes)));
        }

        return std::apply(f,
            detail::strings2tuple<std::tuple<ArgTypes...>>(sp,
                std::make_index_sequence<sizeof...(ArgTypes)>{}));
    }
};

template <class R, class ... ArgTypes>
VisibleFunctionJumppad(R(*)(ArgTypes...), const String &, int = SW_JUMPPAD_DEFAULT_FUNCTION_VERSION)->VisibleFunctionJumppad<R(ArgTypes...)>;

SW_BUILDER_API
int jumppad_call(const path &module, const String &name, int version, const Strings &s = {});

SW_BUILDER_API
int jumppad_call(const Strings &s);

}
