// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <sw/driver/entry_point.h>

class cmake;

namespace sw::driver::cpp
{

struct CmakeTargetEntryPoint : NativeTargetEntryPoint
{
    CmakeTargetEntryPoint(const path &fn);

private:
    path rootfn;
    std::unique_ptr<cmake> cm;

    void loadPackages1(Build &) const override;
};

}
