// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <sw/support/specification.h>

#include <primitives/filesystem.h>

#include <optional>

namespace sw
{

struct InputDatabase;
struct SwContext;

// Represents set of specification files for single input.
// It may be set of sw (make, cmake, qmake etc.) files.
//
// must store one of:
//  - set of files (including virtual)
//  - single directory
struct SW_CORE_API Specification
{
    Specification(const SpecificationFiles &);
    Specification(const path &dir);

    // One spec differs from the other by its hash.
    // We only need to test it locally and do not care about portability between systems.
    // Hash is combination of rel paths and contents.
    size_t getHash(const InputDatabase &) const;

    bool isOutdated(const fs::file_time_type &) const;

    //const String &getFileContents(const path &relpath);
    //const String &getFileContents(const path &relpath) const;

    // returns absolute paths of files
    Files getFiles() const;

    String getName() const;

    void read();

//private: // temporarily (TODO: update upload)
    SpecificationFiles files;
    path dir;
};

} // namespace sw
