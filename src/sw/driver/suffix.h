// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <sw/manager/package.h>

namespace sw
{

inline namespace literals
{

/*
reference:

_api - add api definition
_d, _def - definition
_dep - dependency
_f, _fr? - framework (macos)
_id, _idir - include directory: system, after??, before
_id_s, _idir_s - system include directory: system, after??, before
// _s_id? s_idir?
_ld, _ldir - link directory: system, after??, before
_l, _ll, _lib - link library
_slib - system link library - slibs are not tracked as inputs
// _lib_s?
_pch - precompiled header
_r - regex
_rr - recursive regex
_s?, _sf? - source file

sources:
    _git
    _git_v - means that version is a tag 'v{v}'
    _remote
    ...

*/

// api definition
// _api
inline ApiNameType operator "" _api(const char *s, size_t)
{
    return ApiNameType(String(s));
}

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

// framework (macos)
// _framework (_fr? fw?)
inline Framework operator "" _framework(const char *s, size_t)
{
    return Framework(String(s));
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
inline LinkDirectory operator "" _ld(const char *s, size_t)
{
    return LinkDirectory(String(s));
}
inline LinkDirectory operator "" _ldir(const char *s, size_t)
{
    return LinkDirectory(String(s));
}

// link library
// _l or _lib
/*inline LinkLibrary operator "" _l(const char *s, size_t)
{
    return LinkLibrary(String(s));
}*/
inline LinkLibrary operator "" _lib(const char *s, size_t)
{
    return LinkLibrary(String(s));
}

// precompiled header
// _pch
inline PrecompiledHeader operator "" _pch(const char *s, size_t)
{
    return PrecompiledHeader(String(s));
}

// regex
inline FileRegex operator "" _r(const char *s, size_t)
{
    return FileRegex(s, false);
}

// recursive regex
inline FileRegex operator "" _rr(const char *s, size_t)
{
    return FileRegex(s, true);
}

// system link library
// _slib
inline SystemLinkLibrary operator "" _slib(const char *s, size_t)
{
    return SystemLinkLibrary(String(s));
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

// sources

inline Git operator "" _git(const char *s, size_t)
{
    return Git(s);
}
inline Git operator "" _git_v(const char *s, size_t)
{
    return Git(s, "v{v}");
}

inline RemoteFile operator "" _remote(const char *s, size_t)
{
    return RemoteFile(s);
}

// modules
inline HeaderUnit operator "" _qhu(const char *s, size_t)
{
    return HeaderUnit(s, false);
}
inline HeaderUnit operator "" _ahu(const char *s, size_t)
{
    return HeaderUnit(s, true);
}

// more?

} // inline namespace literals

} // namespace sw
