// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "file.h"
#include "node.h"

#include <sw/manager/version.h>

#include <optional>

#define SW_DECLARE_PROGRAM_CLONE \
    std::shared_ptr<Program> clone() const override

#define SW_DEFINE_PROGRAM_CLONE(t)            \
    std::shared_ptr<Program> t::clone() const \
    {                                         \
        return std::make_shared<t>(*this);    \
    }

#define SW_DEFINE_PROGRAM_CLONE_INLINE(t)           \
    std::shared_ptr<Program> clone() const override \
    {                                               \
        return std::make_shared<t>(*this);          \
    }

namespace sw
{

struct SW_BUILDER_API Program : File, detail::Executable
{
    virtual ~Program() = default;

    virtual std::shared_ptr<Program> clone() const = 0;
    virtual Version &getVersion();

    const Version &getVersion() const;

protected:
    virtual Version gatherVersion() const { return gatherVersion(file); }
    virtual Version gatherVersion(const path &program, const String &arg = "--version", const String &in_regex = {}) const;

private:
    std::optional<Version> version;
};

using ProgramPtr = std::shared_ptr<Program>;

}
