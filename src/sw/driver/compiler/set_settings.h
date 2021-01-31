// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

namespace sw
{

struct PackageSettings;
struct SwBuild;
struct Target;

void addSettingsAndSetPrograms(Target &t, PackageSettings &);
void addSettingsAndSetHostPrograms(Target &t, PackageSettings &);
void addSettingsAndSetConfigPrograms(/*const SwBuild &, */PackageSettings &);

}
