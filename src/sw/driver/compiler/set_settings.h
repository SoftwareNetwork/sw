// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

namespace sw
{

struct TargetSettings;
struct SwBuild;

void addSettingsAndSetPrograms(const SwBuild &, TargetSettings &);
void addSettingsAndSetHostPrograms(const SwBuild &, TargetSettings &);
void addSettingsAndSetConfigPrograms(const SwBuild &, TargetSettings &);

}
