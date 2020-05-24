// SPDX-License-Identifier: MPL-2.0
// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>

#include "enums.h"

namespace sw
{

String toString(StorageFileType t)
{
    switch (t)
    {
    case StorageFileType::SourceArchive:
        return "Source Archive";
    default:
        return "Unknown source type";
    }
}

}
