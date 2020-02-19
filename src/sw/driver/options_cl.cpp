// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "options_cl.h"

#include "command.h"

#include <sw/manager/package.h>

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
    if (separate_prefix)
        return { getCommandLineFlag(), value() };
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
    //if (intermediate_file)
        //c->addIntermediate(value());
    if (output_dependency)
        c->addOutput(value());
    if (create_directory)
        c->output_dirs.insert(value().parent_path());
    if (separate_prefix)
        return { getCommandLineFlag(), normalize_path(value()) };
    return { getCommandLineFlag() + normalize_path(value()) };
}

DECLARE_OPTION_SPECIALIZATION(FilesOrdered)
{
    Strings cmds;
    for (auto &v : value())
    {
        if (input_dependency)
            c->addInput(v);
        //if (intermediate_file)
            //c->addIntermediate(v);
        if (output_dependency)
            c->addOutput(v);
        if (cmd_flag_before_each_value)
        {
            if (separate_prefix)
            {
                cmds.push_back(getCommandLineFlag());
                cmds.push_back(normalize_path(v));
            }
            else
                cmds.push_back(getCommandLineFlag() + normalize_path(v));
        }
        else
            cmds.push_back(normalize_path(v));
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
        //if (intermediate_file)
            //c->addIntermediate(v);
        if (output_dependency)
            c->addOutput(v);
        if (cmd_flag_before_each_value)
        {
            if (separate_prefix)
            {
                cmds.push_back(getCommandLineFlag());
                cmds.push_back(normalize_path(v));
            }
            else
                cmds.push_back(getCommandLineFlag() + normalize_path(v));
        }
        else
            cmds.push_back(normalize_path(v));
    }
    return cmds;
}

DECLARE_OPTION_SPECIALIZATION(std::set<int>)
{
    Strings cmds;
    for (auto v : value())
    {
        if (cmd_flag_before_each_value)
        {
            if (separate_prefix)
            {
                cmds.push_back(getCommandLineFlag());
                cmds.push_back(std::to_string(v));
            }
            else
                cmds.push_back(getCommandLineFlag() + std::to_string(v));
        }
        else
            cmds.push_back(std::to_string(v));
    }
    return cmds;
}

}
