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

#pragma once

#include <primitives/exceptions.h>
#include <primitives/filesystem.h>

#include <gsl/span>
#include <gsl/span_ext>

#include <functional>

#ifdef SW_PACKAGE_API
#define SW_JUMPPAD_API SW_PACKAGE_API
#else
#define SW_JUMPPAD_API SW_EXPORT
#endif

#define SW_JUMPPAD_PREFIX _sw_fn_jumppad_
#define SW_JUMPPAD_DEFAULT_FUNCTION_VERSION 0

// strict macro
// M(visible name, function name in code, ...)
// M2(function name in code, ...) // visible name will be the same as function name
// M3(function name in code, ...) // macro to use in different calls that accept builtin functions
#define SW_DEFINE_VISIBLE_FUNCTION_JUMPPAD(n, f, ...)                                 \
    extern "C" SW_JUMPPAD_API int CONCATENATE(SW_JUMPPAD_PREFIX, n)(const Strings &s) \
    {                                                                                 \
        ::sw::VisibleFunctionJumppad j(&f, #n, ##__VA_ARGS__);                        \
        return j.call(s);                                                             \
    }
#define SW_DEFINE_VISIBLE_FUNCTION_JUMPPAD2(n, ...) SW_DEFINE_VISIBLE_FUNCTION_JUMPPAD(n, n, ##__VA_ARGS__)
#define SW_VISIBLE_BUILTIN_FUNCTION(f, ...) "sw_" #f, nullptr, ##__VA_ARGS__
#define SW_VISIBLE_FUNCTION(f, ...) #f, (void*)&f, ##__VA_ARGS__

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

template <>
inline FilesOrdered from_string(gsl::span<const String> &s)
{
    auto b = s.begin();
    FilesOrdered f;
    auto n = std::stoi(*b++);
    while (n--)
        f.push_back(*b++);
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
