// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <package.h>

namespace sw
{

/*
reference:

_d, _def - definition
_dep - dependency
_f, _fr? - framework (macos)
_id, _idir - include directory: system, after??, before
_id_s, _idir_s - system include directory: system, after??, before
// _s_id? s_idir?
_ld, _ldir - link directory: system, after??, before
_l, _ll, _lib - link library
// _slib - system link library?
// _lib_s?
_pch - precompiled header
_r - regex
_rr - recursive regex
_s?, _sf? - source file

*/

// definition
// _d or _def
inline Definition operator "" _d(const char *s, size_t)
{
    return Definition(String(s));
}
inline Definition operator "" _def(const char *s, size_t)
{
    return Definition(String(s));
}

// dependency
// _dep
inline DependencyPtr operator "" _dep(const char *s, size_t)
{
    return std::make_shared<Dependency>(extractFromString(s));
}

// regex
inline FileRegex operator "" _r(const char *s, size_t)
{
    return FileRegex(s);
}

// recursive regex
inline FileRegex operator "" _rr(const char *s, size_t)
{
    return FileRegex(s, true);
}

// include directory
// _id or _idir
inline IncludeDirectory operator "" _id(const char *s, size_t)
{
    return IncludeDirectory(String(s));
}
inline IncludeDirectory operator "" _idir(const char *s, size_t)
{
    return IncludeDirectory(String(s));
}

// link directory
// _ld or _ldir

// _link _library
// _l or _lib
inline LinkLibrary operator "" _l(const char *s, size_t)
{
    return LinkLibrary(String(s));
}
inline LinkLibrary operator "" _lib(const char *s, size_t)
{
    return LinkLibrary(String(s));
}

// precompiled header
// _pch
inline PrecompiledHeader operator "" _pch(const char *s, size_t)
{
    PrecompiledHeader pch;
    pch.header = s;
    return pch;
}

// variable
// _v or _var
inline Variable operator "" _v(const char *s, size_t)
{
    return Variable{ s };
}
inline Variable operator "" _var(const char *s, size_t)
{
    return Variable{ s };
}

// more?

}
