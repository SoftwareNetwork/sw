/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2019 Egor Pugin
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

#include "commands.h"

#include "../inserts.h"

#include <sw/manager/storage.h>

#include <boost/algorithm/string.hpp>
#include <primitives/emitter.h>
#include <primitives/yaml.h>

static String get_name(const Options &options)
{
    String name = fs::current_path().filename().u8string();
    if (!options.options_create.create_proj_name.empty())
        name = options.options_create.create_proj_name;
    return name;
}

SUBCOMMAND_DECL(create)
{
    auto swctx = createSwContext(options);
    const auto root = YAML::Load(project_templates);
    auto &tpls = root["templates"];

    if (options.options_create.create_clear_dir)
    {
        String s;
        if (!options.options_create.create_clear_dir_y)
        {
            std::cout << "Going to clear current directory. Are you sure? [Yes/No]\n";
            std::cin >> s;
        }
        if (options.options_create.create_clear_dir_y || boost::iequals(s, "yes") || boost::iequals(s, "Y"))
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
    if (!options.options_create.create_overwrite_files && fs::directory_iterator(".") != fs::directory_iterator())
        throw SW_RUNTIME_ERROR("directory is not empty");

    if (options.options_create.create_type == "project")
    {
        auto &tpl = tpls[(String)options.options_create.create_template];
        if (!tpl.IsDefined())
            throw SW_RUNTIME_ERROR("No such template: " + options.options_create.create_template);

        auto name = get_name(options);
        auto target = tpl["target"].template as<String>();
        String files;
        for (auto &p : tpl["files"])
        {
            auto fn = p.first.template as<String>();
            files += "t += \"" + fn + "\";\n";
        }
        String deps;
        for (auto &d : get_sequence(tpl["dependencies"]))
            deps += "t += \"" + d + "\"_dep;\n";
        for (auto p : tpl["config"])
        {
            auto fn = p.first.template as<String>();
            auto fn2 = p.second.template as<String>();
            auto &contents = root["files"][fn2];
            if (!contents.IsDefined())
                throw SW_RUNTIME_ERROR("No such file: " + fn + " (" + fn2 + ")");
            auto s = contents.template as<String>();
            boost::replace_all(s, "{target}", target);
            boost::replace_all(s, "{name}", name);
            boost::replace_all(s, "{files}", files);
            boost::replace_all(s, "{deps}", deps);
            write_file(fn, s);
        }
        for (auto p : tpl["files"])
        {
            auto fn = p.first.template as<String>();
            auto fn2 = p.second.template as<String>();
            auto &contents = root["files"][fn2];
            if (!contents.IsDefined())
                throw SW_RUNTIME_ERROR("No such file: " + fn + " (" + fn2 + ")");
            auto s = contents.template as<String>();
            write_file(fn, s);
        }

        if (options.options_create.create_build)
            cli_build(*swctx, options);
        else
        {
            // uses current cmd line which is not suitable for VS
            //cli_generate(*swctx);
            system("sw generate");
        }
        return;
    }

    if (options.options_create.create_type == "config")
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
        return;
    }
    else
        throw SW_RUNTIME_ERROR("Unknown create type");
}
