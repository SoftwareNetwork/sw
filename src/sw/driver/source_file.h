// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "rule_file.h"
#include "source_file_storage.h"
#include "types.h"

#include <sw/builder/node.h>

#include <memory>

namespace sw
{

struct SourceFile;
struct Target;

struct SW_DRIVER_CPP_API SourceFile : /*TargetFile, */ICastable
{
    path file;
    bool skip = false;
    path install_dir;
    RuleFile::AdditionalArguments args; // additional args per rule, move to native?
    String fancy_name; // for output
    bool skip_unity_build = false;
    int index; // index of file during addition

    SourceFile(const path &input);
    SourceFile(const SourceFile &) = default;
    virtual ~SourceFile() = default;

    bool isActive() const;

    auto &getAdditionalArguments() { return args; }
    const auto &getAdditionalArguments() const { return args; }
};

/*struct SW_DRIVER_CPP_API NativeSourceFile : SourceFile
{
    enum BuildAsType
    {
        BasedOnExtension,
        ASM,
        C,
        CPP,
    };
    BuildAsType BuildAs = BuildAsType::BasedOnExtension;
    bool skip_linking = false; // produce object file only
    bool skip_pch = false;
};*/

}
