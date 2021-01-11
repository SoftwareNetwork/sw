// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

namespace sw
{

struct PackageSettings;

// move to builder probably
SW_CORE_API
PackageSettings toPackageSettings(const struct OS &);

}
