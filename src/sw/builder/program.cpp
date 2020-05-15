/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "program.h"

#include "command.h"
#include "file_storage.h"
#include "sw_context.h"

#include <sw/manager/storage.h>

#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/lock_types.hpp>

#include <fstream>
#include <regex>

namespace sw
{

Program::Program()
{
}

Program::Program(const Program &rhs)
    : file(rhs.file)
{
}

Program &Program::operator=(const Program &rhs)
{
    file = rhs.file;
    return *this;
}

Program &PredefinedProgram::getProgram()
{
    if (!program)
        throw SW_RUNTIME_ERROR("Program was not set");
    return *program;
}

const Program &PredefinedProgram::getProgram() const
{
    if (!program)
        throw SW_RUNTIME_ERROR("Program was not set");
    return *program;
}

}
