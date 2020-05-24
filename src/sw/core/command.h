// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "settings.h"

#include <primitives/command.h>

namespace sw
{

TargetSettings commandToTargetSettings(const primitives::Command &);
// builder::command?
//TargetSettings targetSettingsToCommand(const primitives::Command &);

} // namespace sw
