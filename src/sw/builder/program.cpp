// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "program.h"

#include "command.h"
#include "file_storage.h"
//#include "program_version_storage.h"
#include "sw_context.h"

#include <sw/manager/storage.h>

#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/lock_types.hpp>

#include <fstream>
#include <regex>

namespace sw
{

Program::Program(const SwBuilderContext &swctx)
    : swctx(swctx)
{
}

Program::Program(const Program &rhs)
    : swctx(rhs.swctx), file(rhs.file)
{
}

Program &Program::operator=(const Program &rhs)
{
    file = rhs.file;
    return *this;
}

}
