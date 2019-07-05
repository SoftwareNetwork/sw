// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "file_storage.h"

#include "sw_context.h"

#include <primitives/debug.h>
#include <primitives/executor.h>
#include <primitives/file_monitor.h>
#include <primitives/sw/cl.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "file_storage");

namespace sw
{

FileStorage::FileStorage(const SwBuilderContext &swctx)
    : swctx(swctx)
{
}

FileStorage::~FileStorage()
{
}

void FileStorage::clear()
{
    files.clear();
}

void FileStorage::reset()
{
    for (auto i = files.getIterator(); i.isValid(); i.next())
    {
        auto &f = *i.getValue();
        f.reset();
    }
}

FileRecord *FileStorage::registerFile(const File &in_f)
{
    // fs path hash on windows differs for lower and upper cases
#ifdef CPPAN_OS_WINDOWS
    // very slow
    //((File*)&in_f)->file = boost::to_lower_copy(normalize_path(in_f.file));
    ((File*)&in_f)->file = normalize_path(in_f.file);
#endif

    auto d = swctx.getFileData().insert(in_f.file);
    auto r = files.insert(in_f.file);
    in_f.r = r.first;
    r.first->data = d.first;
    r.first->fs = this;

    return r.first;
}

FileRecord *FileStorage::registerFile(const path &in_f)
{
    auto p = normalize_path(in_f);
    auto r = files.insert(p);
    r.first->fs = this;
    auto d = swctx.getFileData().insert(p);
    r.first->data = d.first;
    //if (!d.first)
        //throw RUNTIME_EXCEPTION("Cannot create file data for file: " + in_f.u8string());
    return r.first;
}

}
