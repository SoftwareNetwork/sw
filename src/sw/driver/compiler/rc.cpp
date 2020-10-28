// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "rc.h"

#include "compiler_helpers.h"
#include "../command.h"

namespace sw
{

SW_DEFINE_PROGRAM_CLONE(RcTool)

void RcTool::prepareCommand1(const Target &t)
{
    //
    // https://docs.microsoft.com/en-us/windows/win32/menurc/resource-compiler
    // What we know:
    // - rc can use .rsp files
    // - include dirs with spaces cannot be placed into rsp and must go into INCLUDE env var
    //   ms bug: https://developercommunity.visualstudio.com/content/problem/417189/rcexe-incorrect-behavior-with.html
    // - we do not need to protect args with quotes: "-Dsomevar" - not needed
    // - definition value MUST be escaped: -DKEY="VALUE" because of possible spaces ' ' and braces '(', ')'
    // - include dir without spaces MUST NOT be escaped: -IC:/SOME/DIR
    //

    cmd->protect_args_with_quotes = false;

    // defs
    auto print_def = [&c = *cmd](const auto &a)
    {
        for (auto &[k,v] : a)
        {
            if (v.empty())
                c.arguments.push_back("-D" + k);
            else
            {
                String s = "-D" + k + "=";
                auto v2 = v.toString();
                //if (v2[0] != '\"')
                    //s += "\"";
                s += v2;
                //if (v2[0] != '\"')
                    //s += "\"";
                c.arguments.push_back(s);
            }
        }
    };

    print_def(t.template as<NativeCompiledTarget>().getMergeObject().NativeCompilerOptions::Definitions);
    print_def(t.template as<NativeCompiledTarget>().getMergeObject().NativeCompilerOptions::System.Definitions);

    // idirs
    Strings env_idirs;
    for (auto &d : t.template as<NativeCompiledTarget>().getMergeObject().NativeCompilerOptions::gatherIncludeDirectories())
    {
        auto i = to_string(normalize_path(d));
        if (i.find(' ') != i.npos)
            env_idirs.push_back(i);
        else
            cmd->arguments.push_back("-I" + i);
    }

    // use env
    String s;
    // it is ok when INCLUDE is empty, do not check for it!
    for (auto &i : env_idirs)
        s += i + ";";
    cmd->environment["INCLUDE"] = s;

    getCommandLineOptions<RcToolOptions>(cmd.get(), *this);
}

}
