// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "commands.h"

#include <boost/algorithm/string.hpp>
#include <primitives/emitter.h>

static ::cl::opt<String> create_type(::cl::Positional, ::cl::desc("<type>"), ::cl::sub(subcommand_create), ::cl::Required);
static ::cl::opt<String> create_proj_name(::cl::Positional, ::cl::desc("<project name>"), ::cl::sub(subcommand_create));

static ::cl::opt<String> create_template("template", ::cl::desc("Template project to create"), ::cl::sub(subcommand_create), ::cl::init("exe"));
static ::cl::alias create_template2("t", ::cl::desc("Alias for -template"), ::cl::aliasopt(create_template));
static ::cl::opt<String> create_language("language", ::cl::desc("Template project language to create"), ::cl::sub(subcommand_create), ::cl::init("cpp"));
static ::cl::alias create_language2("l", ::cl::desc("Alias for -language"), ::cl::aliasopt(create_language));
static ::cl::opt<bool> create_clear_dir("clear", ::cl::desc("Clear current directory"), ::cl::sub(subcommand_create));
static ::cl::opt<bool> create_clear_dir_y("y", ::cl::desc("Answer yes"), ::cl::sub(subcommand_create));
static ::cl::opt<bool> create_build("b", ::cl::desc("Build instead of generate"), ::cl::sub(subcommand_create));
static ::cl::alias create_clear_dir2("c", ::cl::desc("Alias for -clear"), ::cl::aliasopt(create_clear_dir));
static ::cl::opt<bool> create_overwrite_files("overwrite", ::cl::desc("Clear current directory"), ::cl::sub(subcommand_create));
static ::cl::alias create_overwrite_files2("ow", ::cl::desc("Alias for -overwrite"), ::cl::aliasopt(create_overwrite_files));
static ::cl::alias create_overwrite_files3("o", ::cl::desc("Alias for -overwrite"), ::cl::aliasopt(create_overwrite_files));

SUBCOMMAND_DECL(create)
{
    auto swctx = createSwContext();
    if (create_type == "project")
    {
        if (create_clear_dir)
        {
            String s;
            if (!create_clear_dir_y)
            {
                std::cout << "Going to clear current directory. Are you sure? [Yes/No]\n";
                std::cin >> s;
            }
            if (create_clear_dir_y || boost::iequals(s, "yes") || boost::iequals(s, "Y"))
            {
                for (auto &p : fs::directory_iterator("."))
                    fs::remove_all(p);
            }
            else
            {
                if (fs::directory_iterator(".") != fs::directory_iterator())
                    return;
            }
        }

        if (!create_overwrite_files && fs::directory_iterator(".") != fs::directory_iterator())
            throw SW_RUNTIME_ERROR("directory is not empty");

        String name = fs::current_path().filename().u8string();
        if (!create_proj_name.empty())
            name = create_proj_name;

        // TODO: add separate extended template with configure
        // common sw.cpp
        primitives::CppEmitter ctx;
        ctx.beginFunction("void build(Solution &s)");
        ctx.addLine("// Uncomment to make a project. Also replace s.addTarget(). with p.addTarget() below.");
        ctx.addLine("// auto &p = s.addProject(\"myproject\");");
        ctx.addLine("// p += Git(\"enter your url here\", \"enter tag here\", \"or branch here\");");
        ctx.addLine();
        ctx.addLine("auto &t = s.addTarget<Executable>(\"" + name + "\");");
        ctx.addLine("t += cpp17;");

        String s;
        if (create_language == "cpp")
        {
            if (create_template == "sw")
            {
                s = R"(#include <primitives/sw/main.h>
#include <primitives/sw/settings.h>
#include <primitives/sw/cl.h>

#include <iostream>

int main(int argc, char *argv[])
{
    ::cl::ParseCommandLineOptions(argc, argv);

    std::cout << "Hello, World!\n";
    return 0;
}
)";
            }
            else
            {
                s = R"(#include <iostream>

int main(int argc, char *argv[])
{
    std::cout << "Hello, World!\n";
    return 0;
}
)";
            }
            write_file("src/main.cpp", s);

            ctx.addLine("t += \"src/main.cpp\";");
            if (create_template == "sw")
                ctx.addLine("t += \"pub.egorpugin.primitives.sw.main-master\"_dep;");
            ctx.endFunction();
            write_file("sw.cpp", ctx.getText());

            if (create_build)
                cli_build(*swctx);
            else
                cli_generate(*swctx);
        }
        else if (create_language == "c")
        {
            s = R"(#include <stdio.h>

int main(int argc, char *argv[])
{
    printf("Hello, World!\n");
    return 0;
}
)";
            write_file("src/main.c", s);

            ctx.addLine("t += \"src/main.c\";");
            ctx.endFunction();
            write_file("sw.cpp", ctx.getText());

            if (create_build)
                cli_build(*swctx);
            else
                cli_generate(*swctx);
        }
        else
            throw SW_RUNTIME_ERROR("unknown language");
    }
    else if (create_type == "config")
    {
        primitives::CppEmitter ctx;
        ctx.beginFunction("void build(Solution &s)");
        ctx.addLine("// Uncomment to make a project. Also replace s.addTarget(). with p.addTarget() below.");
        ctx.addLine("// auto &p = s.addProject(\"myproject\", \"master\");");
        ctx.addLine("// p += Git(\"https://github.com/account/project\");");
        ctx.addLine();
        ctx.addLine("auto &t = s.addTarget<Executable>(\"project\");");
        ctx.addLine("t += cpp17;");
        ctx.addLine("//t += \"src/main.cpp\";");
        ctx.addLine("//t += \"pub.egorpugin.primitives.sw.main-master\"_dep;");
        ctx.endFunction();
        write_file("sw.cpp", ctx.getText());
    }
    else
        throw SW_RUNTIME_ERROR("Unknown create type");
}
