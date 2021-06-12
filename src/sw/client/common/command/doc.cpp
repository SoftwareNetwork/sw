// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#include "../commands.h"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <Objbase.h>
#include <Shlobj.h>
#endif

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "command.doc");

void open_url(const String &);

SUBCOMMAND_DECL(doc)
{
    open_url(SW_DOC_URL);
}
