// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "extensions.h"

namespace sw
{

const StringSet &getCppHeaderFileExtensions()
{
    static const StringSet header_file_extensions{
        ".h",
        ".hh",
        ".hm",
        ".hpp",
        ".hxx",
        ".tcc",
        ".h++",
        ".H++",
        ".HPP",
        ".H",
    };
    return header_file_extensions;
}

const StringSet &getCppSourceFileExtensions()
{
    static const StringSet cpp_source_file_extensions{
        ".cc",
        ".CC",
        ".cpp",
        ".cp",
        ".cxx",
        //".ixx", // msvc modules?
        // cppm - clang?
        // mxx, mpp - build2?
        ".c++",
        ".C++",
        ".CPP",
        ".CXX",
        ".C", // old ext (Wt)
              // Objective-C
              ".m",
              ".mm",
    };
    return cpp_source_file_extensions;
}

bool isCppHeaderFileExtension(const String &e)
{
    auto &exts = getCppHeaderFileExtensions();
    return exts.find(e) != exts.end();
}

bool isCppSourceFileExtensions(const String &e)
{
    auto &exts = getCppSourceFileExtensions();
    return exts.find(e) != exts.end();
}

StringSet get_cpp_exts(bool apple)
{
    auto cppexts = getCppSourceFileExtensions();
    if (!apple)
    {
        cppexts.erase(".m");
        cppexts.erase(".mm");
    }
    return cppexts;
}

StringSet get_asm_exts(bool windows)
{
    if (windows)
        return { ".asm" };
    else
        return { ".s", ".S", ".sx" };
}

}
