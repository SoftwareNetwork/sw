// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <primitives/filesystem.h>

namespace sw
{

enum class GeneratorType
{
    UnspecifiedGenerator,

    CMake,
    Ninja,
    Qmake,
    UnixMakefiles,
    VisualStudio,
    VisualStudioNMake,
};

struct Generator
{
    GeneratorType type = GeneratorType::UnspecifiedGenerator;
    //path dir;
    path file;

    virtual ~Generator() = default;

    virtual void generate(const struct Build &b) = 0;
    void generate(const path &file, const struct Build &b);

    static std::unique_ptr<Generator> create(const String &s);
};

struct VSGenerator : Generator
{
    void generate(const struct Build &b) override;
};

struct VSGeneratorNMake : Generator
{
    void generate(const struct Build &b) override;
};

struct NinjaGenerator : Generator
{
    void generate(const struct Build &b) override;
};

String toString(GeneratorType Type);
GeneratorType fromString(const String &ss);

}
