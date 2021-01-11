// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "settings.h"

#include <primitives/command.h>

namespace sw
{

PackageSettings commandToPackageSettings(const primitives::Command &);
// builder::command?
//PackageSettings targetSettingsToCommand(const primitives::Command &);

} // namespace sw
