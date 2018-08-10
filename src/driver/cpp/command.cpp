// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "target.h"
#include "command.h"

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "command");

namespace sw::driver::cpp
{

path Command::getProgram() const
{
    path p;
    if (base)
    {
        p = base->file;
        if (p.empty())
            throw std::runtime_error("Empty program from base program");
    }
    else if (dependency)
    {
        if (!dependency->target)
            throw std::runtime_error("Command dependency target was not resolved: " + dependency->getPackage().toString());
        p = dependency->target->getOutputFile();
        if (p.empty())
            throw std::runtime_error("Empty program from package: " + dependency->target->getPackage().target_name);
    }
    else
    {
        p = program;
        if (p.empty())
            throw std::runtime_error("Empty program: was not set");
    }
    return p;
}

void Command::setProgram(const std::shared_ptr<Dependency> &d)
{
    dependency = d;
}

void Command::setProgram(const NativeTarget &t)
{
    setProgram(t.getOutputFile());
}

}
