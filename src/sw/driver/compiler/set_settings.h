// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <sw/core/sw_context.h>

namespace sw
{

void addSettingsAndSetPrograms(const SwCoreContext &, TargetSettings &);
void addSettingsAndSetHostPrograms(const SwCoreContext &, TargetSettings &);
void addSettingsAndSetConfigPrograms(const SwContext &, TargetSettings &);

}
