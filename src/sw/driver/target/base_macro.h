// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

// move things below into separate header without pragma once

#define ASSIGN_WRAPPER_SIMPLE(f, t) \
    struct f##_files                \
    {                               \
        t &r;                       \
                                    \
        f##_files(t &r) : r(r)      \
        {                           \
        }                           \
                                    \
        template <class U>          \
        void operator()(const U &v) \
        {                           \
            r.f(v);                 \
        }                           \
    }

#define ASSIGN_WRAPPER(f, t)          \
    struct f##_files : Assigner       \
    {                                 \
        t &r;                         \
                                      \
        f##_files(t &r) : r(r)        \
        {                             \
        }                             \
                                      \
        using Assigner::operator();   \
                                      \
        template <class U>            \
        void operator()(const U &v)   \
        {                             \
            if (!canProceed(r))       \
                return;               \
            r.f(v);                   \
        }                             \
    }

#define ASSIGN_OP(op, f, t)                                   \
    stream_list_inserter<f##_files> operator op(const t &v)   \
    {                                                         \
        auto x = make_stream_list_inserter(f##_files(*this)); \
        x(v);                                                 \
        return x;                                             \
    }

#define ASSIGN_OP_ACTION(op, f, t, a)                         \
    stream_list_inserter<f##_files> operator op(const t &v)   \
    {                                                         \
        a;                                                    \
        auto x = make_stream_list_inserter(f##_files(*this)); \
        x(v);                                                 \
        return x;                                             \
    }

#define ASSIGN_TYPES_NO_REMOVE(t)        \
    ASSIGN_OP(+=, add, t)                \
    ASSIGN_OP_ACTION(=, add, t, clear())

// disabled
//ASSIGN_OP(<<, add, t)

#define ASSIGN_TYPES(t)       \
    ASSIGN_TYPES_NO_REMOVE(t) \
    ASSIGN_OP(-=, remove, t)

// disabled
//ASSIGN_OP(>>, remove, t)

#define ASSIGN_TYPES_AND_EXCLUDE(t) \
    ASSIGN_TYPES(t)                 \
    ASSIGN_OP(^=, remove_exclude, t)

#define SW_TARGET_USING_ASSIGN_OPS(t) \
    using t::operator+=;              \
    using t::operator-=;              \
    using t::operator=;               \
    using t::add;                     \
    using t::remove

// disabled
//using t::operator<<;
//using t::operator>>;
