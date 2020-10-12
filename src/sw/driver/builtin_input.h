// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#include <sw/core/input.h>
#include <sw/core/specification.h>

namespace sw
{

struct BuiltinInput : Input
{
    size_t h;

    BuiltinInput(SwContext &swctx, const IDriver &d, size_t hash)
        : Input(swctx, d, std::make_unique<Specification>(SpecificationFiles{})), h(hash)
    {}

    bool isParallelLoadable() const { return true; }
    size_t getHash() const override { return h; }
    EntryPointPtr load1(SwContext &) override { SW_UNREACHABLE; }
};

using BuiltinInputs = std::unordered_map<Input*, PackageIdSet>;

} // namespace sw
