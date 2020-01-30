// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "settings.h"

#include <primitives/command.h>

namespace sw
{

TargetSettings commandToTargetSettings(const primitives::Command &);
// builder::command?
//TargetSettings targetSettingsToCommand(const primitives::Command &);

} // namespace sw
