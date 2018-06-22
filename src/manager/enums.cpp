// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "enums.h"

String toString(SettingsType e)
{
    switch (e)
    {
    case SettingsType::Local:
        return "local";
    case SettingsType::User:
        return "user";
    case SettingsType::System:
        return "system";
    }
    return std::to_string(toIndex(e));
}
