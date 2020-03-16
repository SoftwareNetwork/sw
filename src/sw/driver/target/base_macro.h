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

#define IMPORT_LIBRARY "sw.dll"

#define SW_MULTIPASS_VARIABLE(n) int n = 1; bool n##_done = false
#define SW_RETURN_MULTIPASS_NEXT_PASS(var) \
    do {var++; return true;} while (0)
#define SW_RETURN_MULTIPASS_END(n) \
    do {n##_done = true; return false;} while (0)

// move things below into separate header without pragma once

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
