// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "options_cl_clang.h"

#include "command.h"

#include <program.h>

namespace sw
{

DEFINE_OPTION_SPECIALIZATION_DUMMY(ClangOptions);
DEFINE_OPTION_SPECIALIZATION_DUMMY(ClangClOptions);

DEFINE_OPTION_SPECIALIZATION_DUMMY(GNUOptions);
DEFINE_OPTION_SPECIALIZATION_DUMMY(GNULinkerOptions);
DEFINE_OPTION_SPECIALIZATION_DUMMY(GNULibrarianOptions);

namespace clang
{
}

Strings getCommandLineImplCPPLanguageStandardClang(const CommandLineOption<CPPLanguageStandard> &co, builder::Command *c)
{
    String s = "-std=c++";
    switch (co.value())
    {
    case CPPLanguageStandard::CPP11:
        s += "11";
        break;
    case CPPLanguageStandard::CPP14:
        s += "14";
        break;
    case CPPLanguageStandard::CPP17:
        s += c->base->getVersion() > 5 ? "17" : "1z";
        break;
    case CPPLanguageStandard::CPPLatest:
        s += "2a";
        break;
    default:
        return {};
    }
    return { s };
}

Strings getCommandLineImplCPPLanguageStandardGNU(const CommandLineOption<CPPLanguageStandard> &co, builder::Command *c)
{
    String s = "-std=c++";
    switch (co.value())
    {
    case CPPLanguageStandard::CPP11:
        s += "11";
        break;
    case CPPLanguageStandard::CPP14:
        s += "14";
        break;
    case CPPLanguageStandard::CPP17:
        s += c->base->getVersion() > 6 ? "17" : "1z";
        break;
    case CPPLanguageStandard::CPPLatest:
        s += "2a";
        break;
    default:
        return {};
    }
    return { s };
}

}
