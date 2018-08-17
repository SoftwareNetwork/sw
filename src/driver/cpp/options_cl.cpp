// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "options_cl.h"

#include "command.h"

#include <package.h>
#include <target.h>

namespace sw
{

DECLARE_OPTION_SPECIALIZATION(bool)
{
    if (!value())
        return {};
    return { getCommandLineFlag() };
}

DECLARE_OPTION_SPECIALIZATION(String)
{
    return { getCommandLineFlag() + value() };
}

DECLARE_OPTION_SPECIALIZATION(StringMap<String>)
{
    Strings cmds;
    for (auto &[k,v] : value())
        cmds.push_back(getCommandLineFlag() + k + "=" + v);
    return cmds;
}

DECLARE_OPTION_SPECIALIZATION(path)
{
    if (input_dependency)
        c->addInput(value());
    if (intermediate_file)
        c->addIntermediate(value());
    if (output_dependency)
        c->addOutput(value());
    return { getCommandLineFlag() + normalize_path(value()) };
}

DECLARE_OPTION_SPECIALIZATION(FilesOrdered)
{
    Strings cmds;
    for (auto &v : value())
    {
        if (input_dependency)
            c->addInput(v);
        if (intermediate_file)
            c->addIntermediate(v);
        if (output_dependency)
            c->addOutput(v);
        if (cmd_flag_before_each_value)
            cmds.push_back(getCommandLineFlag() + v.string());
        else
            cmds.push_back(v.string());
    }
    return cmds;
}

DECLARE_OPTION_SPECIALIZATION(Files)
{
    Strings cmds;
    for (auto &v : value())
    {
        if (input_dependency)
            c->addInput(v);
        if (intermediate_file)
            c->addIntermediate(v);
        if (output_dependency)
            c->addOutput(v);
        if (cmd_flag_before_each_value)
            cmds.push_back(getCommandLineFlag() + v.string());
        else
            cmds.push_back(v.string());
    }
    return cmds;
}

}
