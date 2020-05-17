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

DECLARE_OPTION_SPECIALIZATION(LinkLibrariesType)
{
    Strings cmds;
    for (auto &v : value())
    {
        if (input_dependency)
            c->addInput(v.l);
        //if (intermediate_file)
            //c->addIntermediate(v);
        if (output_dependency)
            c->addOutput(v.l);
        if (cmd_flag_before_each_value)
        {
            auto static_cond = v.static_ && v.style != v.MSVC;

            if (v.whole_archive && v.style == v.AppleLD)
            {
                // https://www.manpagez.com/man/1/ld/Xcode-5.0.php
                // must provide full path of input archive
                cmds.push_back("-Wl,-force_load," + normalize_path(v.l));
                continue;
            }
            if (v.whole_archive && v.style == v.GNU)
                cmds.push_back("-Wl,--whole-archive");
            if (separate_prefix)
            {
                if (!static_cond)
                    cmds.push_back(getCommandLineFlag());
                cmds.push_back((v.whole_archive && v.style == v.MSVC ? "/WHOLEARCHIVE:" : "") + normalize_path(v.l));
            }
            else
            {
                if (!static_cond)
                    cmds.push_back((v.whole_archive && v.style == v.MSVC ? "/WHOLEARCHIVE:" : "") + getCommandLineFlag() + normalize_path(v.l));
                else
                    cmds.push_back((v.whole_archive && v.style == v.MSVC ? "/WHOLEARCHIVE:" : "") + normalize_path(v.l));
            }
            if (v.whole_archive && v.style == v.GNU)
                cmds.push_back("-Wl,--no-whole-archive");
        }
        else
            cmds.push_back((v.whole_archive && v.style == v.MSVC ? "/WHOLEARCHIVE:" : "") + normalize_path(v.l));
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
