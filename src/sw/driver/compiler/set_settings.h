// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

namespace sw
{

struct PackageSettings;
struct SwBuild;

void addSettingsAndSetPrograms(const SwBuild &, PackageSettings &);
void addSettingsAndSetHostPrograms(const SwBuild &, PackageSettings &);
void addSettingsAndSetConfigPrograms(const SwBuild &, PackageSettings &);

}
