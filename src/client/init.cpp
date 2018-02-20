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

using void_f = std::function<void(void)>;

const String invitation = "> ";

void completion_callback(const char *s, Strings &completions);

bool is_y(const String &s)
{
    if (s.empty())
        return false;
    return s[0] == 'y' || s[0] == 'Y';
}

void readline(String &d)
{
    String s;
    std::getline(std::cin, s);
    if (!s.empty())
        d = s;
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

bool y_n_branch(const String &s, const void_f &yf = void_f(), const void_f &nf = void_f())
{
    std::cout << s << " (yes/no) [no]: ";
    String t;
    readline(t);
    bool y = is_y(t);
    if (y && yf)
        yf();
    else if (nf)
        nf();
    return y;
}

void command_init(const Strings &args)
{
    bool script = false;
    bool build_project = true;
    bool header_only = false;
    String project_type = "e";
    String idir;
    Project p;
    p.name = current_thread_path().filename().string();

    // interactive mode
    if (args.empty())
    {
        script = y_n_branch("Create script?");
        std::cout << "Enter project name [" << p.name << "]: ";
        readline(p.name);
        idir = p.name;

        std::cout << "Enter project type (e - executable, l - library) [" << project_type << "]: ";
        readline(project_type);

        if (project_type[0] == 'l')
        {
            p.type = ProjectType::Library;

            y_n_branch("Header only?", [&] { header_only = true; });

            std::cout << "Enter include directory name [" << idir << "]: ";
            readline(idir);
        }

        y_n_branch("Add some dependencies?", [&]
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
            linenoise::disableRawMode(0);
        });
    }
    else
    {
        // TODO: use program options?

        int i = 0;

        script = args[i++] == "script";
        p.name = args[i++];
        idir = p.name;
        project_type = args[i++];

        if (project_type[0] == 'l' || project_type[0] == 'h')
        {
            p.type = ProjectType::Library;
            idir = args[i++];
            if (project_type[0] == 'h')
                header_only = true;
        }

        for (; i < (int)args.size(); i++)
        {
            const auto &line = args[i];
            try
            {
                auto d = extractFromString(line);

                // check pkg
                if (read_packages(d.ppath.toString()).empty())
                {
                    std::cout << "No such package:" << line << "\n";
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
                    std::cout << "Please, enter version after '-' symbol:" << line << "\n";
                    continue;
                }
                std::cout << e.what() << "\n";
            }
        }

        build_project = false;
    }

    boost::system::error_code ec;
    auto root = current_thread_path();

    static const auto err_exist = "File or dir with such name already exist";
    static const auto int_main = "int main(int argc, char **argv)\n{\n    return 0;\n}\n"s;

    if (script)
    {
        auto n = p.name + ".cpp";
        if (fs::exists(root / n))
            throw std::runtime_error(err_exist);
        write_file(root / n, "/*\n" + dump_yaml_config(p.save()) + "*/\n\n" +
            int_main);

        if (build_project && y_n_branch("Build the project?"))
            build(root / n);
    }
    else
    {
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
            fs::exists(root / p.name / "include" / idir) ||
            fs::exists(root / p.name / "include" / idir / (p.name + ".h")) ||
            fs::exists(root / p.name / "src" / (p.name + ".cpp")) ||
            0)
            throw std::runtime_error(err_exist);

        // TODO: add deps includes from hints
        // into header? cpp? <- cpp! to hide from users

        // create, no checks
        fs::create_directories(root / p.name / "src");
        if (p.type == ProjectType::Library)
        {
            fs::create_directories(root / p.name / "include" / idir);
            write_file(root / p.name / "include" / idir / (p.name + ".h"), "//#include <something>\n\n");
            if (header_only)
                write_file(root / p.name / "src" / (p.name + ".cpp"), "#include <" + idir + "/" + p.name + ".h>\n\n");
        }
        else
        {
            write_file(root / p.name / "src" / (p.name + ".cpp"), "//#include <something>\n\n"
                + int_main);
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

        if (build_project && y_n_branch("Build the project?"))
            build();
    }
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
