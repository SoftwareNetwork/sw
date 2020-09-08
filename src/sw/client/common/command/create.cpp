// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>

#include "../commands.h"

#include "../inserts.h"

#include <sw/manager/storage.h>

#include <boost/algorithm/string.hpp>
#include <primitives/emitter.h>
#include <primitives/yaml.h>

#include <iostream>

static String get_name(const Options &options)
{
    String name = to_string(fs::current_path().filename().u8string());
    if (!options.options_create.create_proj_name.empty())
        name = options.options_create.create_proj_name;
    return name;
}

static ProjectTemplates createProjectTemplates()
{
    ProjectTemplates t;

    const auto root = YAML::Load(project_templates);
    for (auto &v : root["templates"])
    {
        auto &tpl = v.second;

        ProjectTemplate p;
        p.name = v.first.template as<String>();
        p.desc = tpl["name"].template as<String>();
        p.target = tpl["target"].template as<String>();
        for (auto &f : tpl["files"])
        {
            auto fn = f.first.template as<String>();
            auto fn2 = f.second.template as<String>();
            p.files[fn] = fn2;
        }
        for (auto &f : tpl["config"])
        {
            auto fn = f.first.template as<String>();
            auto fn2 = f.second.template as<String>();
            p.config[fn] = fn2;
        }
        for (auto &d : get_sequence(tpl["dependencies"]))
            p.dependencies.insert(d);
        t.templates[p.name] = p;
    }
    for (auto &v : root["files"])
    {
        auto fn = v.first.template as<String>();
        auto contents = v.second.template as<String>();
        t.files[fn] = contents;
    }

    return t;
}

const ProjectTemplates &getProjectTemplates()
{
    static const auto t = createProjectTemplates();
    return t;
}

SUBCOMMAND_DECL(create)
{
    auto dir = getOptions().options_create.project_directory;
    if (dir.empty())
        dir = ".";
    ScopedCurrentPath scp(dir);

    if (getOptions().options_create.create_clear_dir)
    {
        String s;
        if (!getOptions().options_create.create_clear_dir_y)
        {
            std::cout << "Going to clear current directory. Are you sure? [Yes/No]\n";
            std::cin >> s;
        }
        if (getOptions().options_create.create_clear_dir_y || boost::iequals(s, "yes") || boost::iequals(s, "Y"))
        {
            for (auto &p : fs::directory_iterator(dir))
                fs::remove_all(p);
        }
        else
        {
            if (fs::directory_iterator(dir) != fs::directory_iterator())
                return;
        }
    }

    auto write_file = [this](const path &fn, const String &s)
    {
        if (fs::exists(fn) && !getOptions().options_create.create_overwrite_files)
            throw SW_RUNTIME_ERROR("File already exists: " + to_string(normalize_path(fn)));
        ::write_file(fn, s);
    };

    if (getOptions().options_create.create_type == "project")
    {
        const auto &tpls = getProjectTemplates();
        auto ti = tpls.templates.find((String)getOptions().options_create.create_template);
        if (ti == tpls.templates.end())
            throw SW_RUNTIME_ERROR("No such template: " + getOptions().options_create.create_template);

        const auto &tpl = ti->second;
        auto name = get_name(getOptions());
        String files;
        for (auto &[fn,_] : tpl.files)
            files += "t += \"" + to_string(normalize_path(fn)) + "\";\n";
        String deps;
        for (auto &d : tpl.dependencies)
            deps += "t += \"" + d + "\"_dep;\n";
        for (auto &d : getOptions().options_create.dependencies)
            deps += "t += \"" + d + "\"_dep;\n";
        for (auto &[fn,fn2] : tpl.config)
        {
            const auto &ci = tpls.files.find(fn2);
            if (ci == tpls.files.end())
                throw SW_RUNTIME_ERROR("No such file: " + to_string(normalize_path(fn)) + " (" + to_string(normalize_path(fn2)) + ")");
            auto s = ci->second;
            boost::replace_all(s, "{target}", tpl.target);
            boost::replace_all(s, "{name}", name);
            boost::replace_all(s, "{files}", files);
            boost::replace_all(s, "{deps}", deps);
            write_file(dir / fn, s);
        }
        for (auto &[fn,fn2] : tpl.files)
        {
            const auto &ci = tpls.files.find(fn2);
            if (ci == tpls.files.end())
                throw SW_RUNTIME_ERROR("No such file: " + to_string(normalize_path(fn)) + " (" + to_string(normalize_path(fn2)) + ")");
            auto s = ci->second;
            write_file(dir / fn, s);
        }

        // set our inputs
        Strings inputs;
        inputs.push_back(to_string(normalize_path(dir)));
        SwapAndRestore sr(getInputs(), inputs);

        if (getOptions().options_create.create_build)
            command_build();
        else
            command_generate();
        return;
    }

    if (getOptions().options_create.create_type == "config")
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
        write_file(dir / "sw.cpp", ctx.getText());
        return;
    }
    else
        throw SW_RUNTIME_ERROR("Unknown create type: " + getOptions().options_create.create_type);
}
