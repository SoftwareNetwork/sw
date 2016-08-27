// This file is in public domain.

// C++ Archive Network (CPPAN)

#pragma once

/*******************************************************************************
*
* general
*
******************************************************************************/

#ifndef CPPAN_BUILD
#define CPPAN_BUILD
#endif

#ifndef CPPAN_EXPORT
#define CPPAN_EXPORT
#endif

/*******************************************************************************
 *
 * export/import
 *
 ******************************************************************************/

#if __GNUC__ >= 4
#  if (defined(_WIN32) || defined(__WIN32__) || defined(WIN32)) && !defined(__CYGWIN__)
// All Win32 development environments, including 64-bit Windows and MinGW, define
// _WIN32 or one of its variant spellings. Note that Cygwin is a POSIX environment,
// so does not define _WIN32 or its variants.
#    define CPPAN_SYMBOL_EXPORT __attribute__((__dllexport__))
#    define CPPAN_SYMBOL_IMPORT __attribute__((__dllimport__))
#  else
#    define CPPAN_SYMBOL_EXPORT __attribute__((__visibility__("default")))
#    define CPPAN_SYMBOL_IMPORT
#  endif
#elif defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
#define CPPAN_SYMBOL_EXPORT __declspec(dllexport)
#define CPPAN_SYMBOL_IMPORT __declspec(dllimport)
#endif

#if __SUNPRO_CC > 0x500
#undef  CPPAN_SYMBOL_EXPORT
#undef  CPPAN_SYMBOL_IMPORT

#define CPPAN_SYMBOL_EXPORT __global
#define CPPAN_SYMBOL_IMPORT __global
#endif

#ifndef CPPAN_SYMBOL_EXPORT
#error "CPPAN_SYMBOL_EXPORT was not defined"
#endif

#ifndef CPPAN_SYMBOL_IMPORT
#error "CPPAN_SYMBOL_IMPORT was not defined"
#endif

/******************************************************************************/
