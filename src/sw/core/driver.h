// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <primitives/filesystem.h>

namespace sw
{

struct Input;
struct SwContext;
enum class InputType : uint8_t;

struct SW_CORE_API IDriver
{
    virtual ~IDriver();

    /// Detect available inputs on path.
    virtual std::vector<std::unique_ptr<Input>> detectInputs(const path &abspath, InputType) const = 0;

    /// Optimized input loading in a batch.
    /// Inputs are unique and non null.
    /// Inputs will receive their entry points.
    virtual void loadInputsBatch(const std::set<Input*> &) const = 0;

    // get features()?
};

} // namespace sw
