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

#include <linenoise.hpp>

#include <iostream>

const String invitation = "> ";

void completion_callback(const char *s, Strings &completions);

bool is_y(const String &s)
{
    if (s.empty())
        return false;
    return s[0] == 'y' || s[0] == 'Y';
}

auto read_packages(const String &s)
{
    auto &pdb = getPackagesDatabase();
    auto pkgs = pdb.getMatchingPackages<std::unordered_set>(s);
    std::vector<String> spkgs;
    spkgs.reserve(pkgs.size());
    for (auto &pkg : pkgs)
        spkgs.push_back(pkg.toString());
    return spkgs;
}

auto read_versions(const String &pkg)
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
        spkgs.push_back(pkg + "-*"); // self, * version
    return spkgs;
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
        if (is_y(add_deps))
        {
            std::cout << "Start entering dependency names. Press TAB to list matching packages, ESC to stop.\n";

            linenoise::SetCompletionCallback(completion_callback);
            std::string line;
            while (!linenoise::Readline(invitation.c_str(), line))
            {
                linenoise::AddHistory(line.c_str());
                try
                {
                    auto d = extractFromString(line);

                    // check pkg
                    if (read_packages(d.ppath.toString()).empty())
                    {
                        std::cout << "No such package.\n";
                        continue;
                    }

                    // check version
                    auto &pdb = getPackagesDatabase();
                    auto v = pdb.getExactVersionForPackage(d);

                    p.dependencies[d.ppath.toString()] = d;
                }
                catch (const std::exception &e)
                {
                    if (read_packages(line).size() == 1)
                    {
                        std::cout << "Please, enter version after '-' symbol.\n";
                        continue;
                    }
                    std::cout << e.what() << "\n";
                }
            }
        }
    }
    else
    {
        // use program options
    }

    if (project_type[0] == 'l')
        p.type = ProjectType::Library;

    boost::system::error_code ec;
    auto root = fs::current_path();

    Config c;
    c.allow_relative_project_names = true;
    //c.allow_local_dependencies = true;

    yaml orig;
    if (fs::is_regular_file(CPPAN_FILENAME))
    {
        orig = load_yaml_config(path(CPPAN_FILENAME));
        c.load(orig);
    }

    // checks first
    auto &projects = c.getProjects();
    if (projects.find(p.name) != projects.end())
        throw std::runtime_error("Project " + p.name + " already exists in the config");

    if (fs::exists(root / p.name) ||
        fs::exists(root / p.name / "src") ||
        fs::exists(root / p.name / "include") ||
        fs::exists(root / p.name / "include" / p.name) ||
        fs::exists(root / p.name / "include" / p.name / (p.name + ".h")) ||
        fs::exists(root / p.name / "src" / (p.name + ".cpp")) ||
        0)
        throw std::runtime_error("File or dir with such name already exist");

    // TODO: add deps includes from hints
    // into header? cpp? <- cpp! to hide from users

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
        projects[p.name] = p;
        y = c.save();
        orig["projects"] = y["projects"];
        y = orig;
    }
    dump_yaml_config(CPPAN_FILENAME, y);

    build();
}

void completion_callback(const char *in, Strings &completions)
{
    String s = in;
    completions = read_packages(s);
    if (completions.empty() && !s.empty())
    {
        s.resize(s.size() - 1);
        completions = read_packages(s);
    }
    if (completions.size() == 1)
        completions = read_versions(completions[0]);

    std::sort(completions.begin(), completions.end());
    completions.erase(std::unique(completions.begin(), completions.end()), completions.end());

    std::cout << "\n";
    bool show = true;
    if (completions.size() > 50)
    {
        show = false;
        std::cout << "Display all " << completions.size() << " possibilities? (y or n)" << " ";
        int c = 0;
#ifdef _WIN32
        linenoise::win32read(&c);
#else
        read(0, &c, 1);
#endif
        std::cout << "\n";
        show = is_y(s = c);
        if (!show)
            completions.clear();
    }
    if (show)
    {
        std::ios_base::sync_with_stdio(false);
        for (auto &c : completions)
            std::cout << c << "\n";
        std::ios_base::sync_with_stdio(true);
    }
    std::cout << invitation;
}
