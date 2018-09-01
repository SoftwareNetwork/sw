// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "file.h"
#include "node.h"

#include <optional>

namespace sw
{

struct SW_BUILDER_API Program : File, detail::Executable
{
    virtual ~Program() = default;

    virtual std::shared_ptr<Program> clone() const = 0;

    virtual Version getVersion() const
    {
        if (!version)
            version = gatherVersion();
        return version.value();
    }

protected:
    mutable std::optional<Version> version;

    virtual Version gatherVersion() const = 0;
};

}
