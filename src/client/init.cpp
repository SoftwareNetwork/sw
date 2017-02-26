/*
 * Copyright (C) 2016-2017, Egor Pugin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "build.h"

#include <config.h>
#include <database.h>

#include <readline/readline.h>
#include <readline/history.h>

#include <iostream>

#include <conio.h>

char **character_name_completion(const char *, int, int);
char *character_name_generator(const char *, int);
int old_string = 0;
String old_s;

int my_getc(FILE *f)
{
    auto c = _getch();
    old_string = !is_valid_project_path_symbol(c) &&
        c != '\b' && c != '-';
    if (!old_string)
    {
        old_s = rl_line_buffer;
        if (c != '\b')
            old_s += c;
        else if (!old_s.empty())
            old_s.resize(old_s.size() - 1);
    }
    return c;
}

void command_init(const Strings &args)
{
    String project_type = "e";
    Project p;
    p.name = fs::current_path().filename().string();

    auto readline = [](String &d)
    {
        String s;
        std::getline(std::cin, s);
        if (!s.empty())
            d = s;
    };

    // interactive mode
    if (args.empty())
    {
        std::cout << "Enter project name [" << p.name << "]: ";
        readline(p.name);
        std::cout << "Enter project type (e - executable, l - library) [" << project_type << "]: ";
        readline(project_type);
        std::cout << "Add some dependencies (y/n) [n]: ";
        String add_deps;
        readline(add_deps);
        if (add_deps[0] == 'y')
        {
            std::cout << "Start entering dependencies' names. You could use TAB to list matching packages.\n";

            rl_attempted_completion_function = character_name_completion;
            rl_completion_query_items = 50;
            //rl_editing_mode = 0; // vi mode
            //rl_getc_function = my_getc;
            //rl_variable_bind("show-mode-in-prompt", "1");
            while (auto buffer = ::readline("> "))
            {
                printf("You entered: %s\n", buffer);
                if (*buffer)
                    add_history(buffer);
                free(buffer);
            }
        }
    }
    else
    {

    }

    if (project_type[0] == 'l')
        p.type = ProjectType::Library;

    boost::system::error_code ec;
    auto root = fs::current_path();

    // checks first
    if (fs::exists(root / p.name) ||
        fs::exists(root / p.name / "src") ||
        fs::exists(root / p.name / "include") ||
        fs::exists(root / p.name / "include" / p.name) ||
        fs::exists(root / p.name / "include" / p.name / (p.name + ".h")) ||
        fs::exists(root / p.name / "src" / (p.name + ".cpp")) ||
        0)
        throw std::runtime_error("One of the fs objects to be created already exist");

    // create, no checks
    fs::create_directories(root / p.name / "src");
    if (p.type == ProjectType::Library)
    {
        fs::create_directories(root / p.name / "include" / p.name);
        write_file(root / p.name / "src" / (p.name + ".cpp"), "#include <" + p.name + "/" + p.name + ".h>\n\n");
        write_file(root / p.name / "include" / p.name / (p.name + ".h"), "//#include <something>\n\n");
    }
    else
    {
        write_file(root / p.name / "src" / (p.name + ".cpp"), "//#include <something>\n\n"
            "int main(int argc, char **argv)\n{\n    return 0;\n}\n");
    }

    p.root_directory = p.name;

    yaml y;
    if (!fs::exists(CPPAN_FILENAME))
    {
        y = p.save();
    }
    else
    {
        auto orig = load_yaml_config(path(CPPAN_FILENAME));
        Config c;
        c.allow_relative_project_names = true;
        //c.allow_local_dependencies = true;
        c.load(orig);
        auto &projects = c.getProjects();
        if (projects.find(p.name) != projects.end())
            throw std::runtime_error("Project " + p.name + " already exists in the config");
        projects[p.name] = p;
        y = c.save();
        orig["projects"] = y["projects"];
        y = orig;
    }
    dump_yaml_config(CPPAN_FILENAME, y);

    build(root);
}

char **
character_name_completion(const char *text, int start, int end)
{
    rl_attempted_completion_over = 1;
    rl_completion_suppress_append = 1;
    if (*text == 0 && start)
        return nullptr;
    return rl_completion_matches(text, character_name_generator);
}

char *
character_name_generator(const char *text, int state)
{
    static std::vector<String> spkgs;
    static size_t i;
    static String t;

    auto read_packages = [](const String &s)
    {
        auto &pdb = getPackagesDatabase();
        auto pkgs = pdb.getMatchingPackages<std::unordered_set>(s);
        std::vector<String> spkgs;
        spkgs.reserve(pkgs.size());
        for (auto &pkg : pkgs)
            spkgs.push_back(pkg.toString());
        return spkgs;
    };

    auto read_versions = [](const String &pkg)
    {
        auto &pdb = getPackagesDatabase();
        auto versions = pdb.getVersionsForPackage(pkg);
        std::vector<String> spkgs;
        spkgs.reserve(versions.size());
        bool has_versions = false;
        for (auto &v : versions)
        {
            spkgs.push_back(pkg + "-" + v.toString());
            if (v.isVersion())
            {
                has_versions = true;
                v.patch = -1;
                spkgs.push_back(pkg + "-" + v.toAnyVersion());
                v.minor = -1;
                spkgs.push_back(pkg + "-" + v.toAnyVersion());
            }
        }
        if (has_versions)
            spkgs.push_back(pkg); // self, * version
        return spkgs;
    };

    if (!state)
    {
        i = -1;
        t = old_string ? old_s : text;
        spkgs = read_packages(t);
        if (spkgs.size() == 1)
            spkgs = read_versions(spkgs[0]);
    }

    while (++i < spkgs.size())
    {
        if (spkgs[i].find(t) != -1)
        {
            return strdup(spkgs[i].c_str());
        }
    }

    return nullptr;
}
