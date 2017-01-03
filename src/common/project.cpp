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

#include "project.h"

#include "bazel/bazel.h"
#include "config.h"
#include "command.h"
#include "http.h"
#include "pack.h"
#include "resolver.h"

#include "printers/printer.h"

#include <boost/algorithm/string.hpp>

#include <regex>

#include "logger.h"
DECLARE_STATIC_LOGGER(logger, "project");

using MimeType = String;
using MimeTypes = std::set<MimeType>;

const MimeTypes source_mime_types{
    "inode/x-empty", // empty file

    "text/x-asm",
    "text/x-c",
    "text/x-c++",
    "text/plain",
    "text/html", // ?
    "text/tex", // ? file with many comments can be this
    "text/x-makefile", // some .in files
    "text/x-shellscript", // some .in files
};

const std::set<String> header_file_extensions{
    ".h",
    ".hh",
    ".hpp",
    ".hxx",
    ".h++",
    ".H++",
    ".HPP",
    ".H",
};

const std::set<String> source_file_extensions{
    ".c",
    ".cc",
    ".cpp",
    ".cxx",
    ".c++",
    ".C++",
    ".CPP",
    // Objective-C
    ".m",
    ".mm",
    ".C",
};

const std::set<String> other_source_file_extensions{
    ".s",
    ".S",
    ".asm",
    ".ipp",
};

const auto bazel_filenames = { "BUILD", "BUILD.bazel" };

bool is_allowed_file_extension(const path &p)
{
    auto e = p.extension().string();
    auto check = [&e](const auto &src)
    {
        return std::any_of(src.begin(), src.end(), [&e](const auto &s)
        {
            return s == e;
        });
    };
    if (check(header_file_extensions) ||
        check(source_file_extensions) ||
        check(other_source_file_extensions))
        return true;
    return false;
}

bool is_valid_file_type(const MimeTypes &types, const path &p, const String &s, String *error = nullptr, bool check_ext = false)
{
    auto mime = s.substr(0, s.find(';'));
    bool ok = std::any_of(types.begin(), types.end(), [&mime](const auto &s)
    {
        return mime == s;
    });
    if (!ok && check_ext)
        ok = is_allowed_file_extension(p);
    if (!ok && error)
        *error = "not supported: " + p.string() + ", mime: " + mime;
    return ok;
}

bool is_valid_file_type(const MimeTypes &types, const path &p, String *error = nullptr, bool check_ext = false)
{
    command::Args args;
    args.push_back("file");
    args.push_back("-ib");
    args.push_back(p.string());
    auto fret = command::execute_and_capture(args);
    return is_valid_file_type(types, p, fret.out, error, check_ext);
}

bool is_valid_source_mime_type(const path &p, String *error = nullptr)
{
    return is_valid_file_type(source_mime_types, p, error, true);
}

bool is_valid_source(const path &p)
{
    if (!p.has_extension())
        return false;
    auto e = p.extension().string();
    return source_file_extensions.find(e) != source_file_extensions.end();
}

bool check_filename(const String &s)
{
    for (auto &c : s)
    {
        if (c < 0 || c > 127)
            return false;
        if (isalnum((uint8_t)c))
            continue;
        switch (c)
        {
        case '/':
        case '\\':
        case ':':
        case '.':
        case '_':
        case '-':
        case '+':
            break;
        default:
            return false;
        }
    }
    return true;
}

void check_file_types(const Files &files)
{
    if (files.empty())
        return;

    String errors;
    for (auto &file : files)
    {
        auto s = file.string();
        if (!check_filename(s))
            errors += "File '" + s + "' has prohibited symbols\n";
    }
    if (!errors.empty())
        throw std::runtime_error("Project sources did not pass file checks:\n" + errors);

    auto fn = get_temp_filename();
    std::ofstream o(fn.string(), std::ios::binary | std::ios::out);
    if (!o)
        throw std::runtime_error("Cannot open file for writing: " + fn.string());
    for (auto &file : files)
        o << "file -ib " << normalize_path(file) << "\n";
    o.close();

    auto ret = command::execute_and_capture({ "sh", fn.string() });
    fs::remove(fn);

    if (ret.rc != 0)
        throw std::runtime_error("Error during file checking: rc = " + std::to_string(ret.rc));

    std::vector<String> lines, sh_out;
    boost::split(sh_out, ret.out, boost::is_any_of("\r\n"));
    for (auto &s : sh_out)
    {
        boost::trim(s);
        if (!s.empty())
            lines.push_back(s);
    }
    if (lines.size() != files.size())
        throw std::runtime_error("Error during file checking: number of output lines does not match");

    int i = 0;
    for (auto &file : files)
    {
        String error;
        is_valid_file_type(source_mime_types, file, lines[i], &error, true);
        if (!error.empty())
            errors += error + "\n";
        i++;
    }
    if (!errors.empty())
        throw std::runtime_error("Project did not pass file checks:\n" + errors);
}

void get_config_insertion(const yaml &n, const String &key, String &dst)
{
    dst = get_scalar<String>(n, key);
    boost::trim(dst);
}

void load_source_and_version(const yaml &root, Source &source, Version &version)
{
    String ver;
    EXTRACT_VAR(root, ver, "version", String);
    if (!ver.empty())
        version = Version(ver);

    if (load_source(root, source) && source.which() == 0)
    {
        auto &git = boost::get<Git>(source);
        if (ver.empty())
        {
            if (git.branch.empty() && git.tag.empty())
            {
                ver = "master";
                version = Version(ver);
            }
            else if (!git.branch.empty())
            {
                ver = git.branch;
                try
                {
                    // branch may contain bad symbols, so put in try...catch
                    version = Version(ver);
                }
                catch (std::exception &)
                {
                }
            }
            else if (!git.tag.empty())
            {
                ver = git.tag;
                try
                {
                    // tag may contain bad symbols, so put in try...catch
                    version = Version(ver);
                }
                catch (std::exception &)
                {
                }
            }
        }

        if (version.isValid() && git.branch.empty() && git.tag.empty() && git.commit.empty())
        {
            if (version.isBranch())
                git.branch = version.toString();
            else
                git.tag = version.toString();
        }
    }
}

void BuildSystemConfigInsertions::load(const yaml &n)
{
#define ADD_CFG_INSERTION(x) get_config_insertion(n, #x, x)
    ADD_CFG_INSERTION(pre_sources);
    ADD_CFG_INSERTION(post_sources);
    ADD_CFG_INSERTION(post_target);
    ADD_CFG_INSERTION(post_alias);
#undef ADD_CFG_INSERTION
}

void BuildSystemConfigInsertions::save(yaml &n) const
{
#define ADD_CFG_INSERTION(x) \
    if (!x.empty())          \
    {                        \
        n[#x] = x;           \
    }

    ADD_CFG_INSERTION(pre_sources);
    ADD_CFG_INSERTION(post_sources);
    ADD_CFG_INSERTION(post_target);
    ADD_CFG_INSERTION(post_alias);
#undef ADD_CFG_INSERTION
}

void Patch::load(const yaml &root)
{
    get_map_and_iterate(root, "replace_in_files", [this](auto &v)
    {
        if (!v.second.IsMap())
            throw std::runtime_error("Members of 'replace_in_files' should be maps");
        if (!(v.second["from"].IsDefined() && v.second["to"].IsDefined()))
            throw std::runtime_error("There are no 'from' and 'to' inside 'replace_in_files'");
        auto from = v.second["from"].template as<String>();
        auto to = v.second["to"].template as<String>();
        replace_in_files[from] = to;
    });
}

void Patch::save(yaml &node) const
{
    yaml root;
    for (auto &r : replace_in_files)
        root[r.first] = r.second;
    if (!replace_in_files.empty())
        node["patch"] = root;
}

Project::Project()
    : Project(ProjectPath())
{

}

Project::Project(const ProjectPath &root_project)
    : root_project(root_project)
{
}

void Project::findSources(path p)
{
    // output file list (files) must contain absolute paths
    //

    // correct root dir is detected and set during load phase
    if (p.empty())
        p = fs::current_path();
    if (p != root_directory)
        p /= root_directory;

    if (import_from_bazel)
    {
        path bfn;
        for (auto &f : bazel_filenames)
        {
            if (fs::exists(p / f))
            {
                bfn = p / f;
                break;
            }
        }

        auto b = read_file(bfn);
        auto f = bazel::parse(b);
        String project_name;
        if (!pkg.ppath.empty())
            project_name = pkg.ppath.back();
        auto files = f.getFiles(project_name);
        sources.insert(files.begin(), files.end());
        sources.insert(bfn.filename().string());
    }

    for (auto i = sources.begin(); i != sources.end();)
    {
        auto f = p / *i;
        if (fs::exists(f) && fs::is_regular_file(f))
        {
            files.insert(f);
            sources.erase(i++);
            continue;
        }
        ++i;
    }

    if ((sources.empty() && files.empty()) && !empty)
        throw std::runtime_error("'files' must be populated");

    auto create_regex = [&p](const auto &e)
    {
        auto s = normalize_path(p);
        if (!s.empty() && s.back() != '/')
            s += "/";
        return std::regex(s + e);
    };

    std::map<String, std::regex> rgxs, rgxs_exclude;

    for (auto &e : sources)
        rgxs[e] = create_regex(e);
    if (!rgxs.empty())
    {
        for (auto &f : boost::make_iterator_range(fs::recursive_directory_iterator(p), {}))
        {
            if (!fs::is_regular_file(f))
                continue;

            auto s = normalize_path(f);
            for (auto &e : rgxs)
            {
                if (!std::regex_match(s, e.second))
                    continue;
                files.insert(f);
                break;
            }
        }
    }

    for (auto &e : exclude_from_package)
        rgxs_exclude[e] = create_regex(e);
    if (!rgxs_exclude.empty())
    {
        auto to_remove = files;
        for (auto &f : files)
        {
            auto s = normalize_path(f);
            for (auto &e : rgxs_exclude)
            {
                if (!std::regex_match(s, e.second))
                    continue;
                to_remove.erase(f);
                break;
            }
        }
        files = to_remove;
    }

    if (files.empty() && !empty)
        throw std::runtime_error("no files found");

    // disable on windows
#ifndef _WIN32
    if (!custom)
        check_file_types(files);
#endif

    // do not check if forced header_only (no matter true or false)
    if (!header_only && !custom)
        header_only = std::none_of(files.begin(), files.end(), is_valid_source);

    // when we see only headers, mark type as library
    // useful for local projects
    if (header_only && header_only.get())
    {
        type = ProjectType::Library;
        pkg.flags.set(pfHeaderOnly);
    }

    auto check_license = [this](auto &name, String *error = nullptr)
    {
        auto license_error = [&error](auto &err)
        {
            if (error)
            {
                *error = err;
                return false;
            }
            throw std::runtime_error(err);
        };
        if (!fs::exists(root_directory / name))
            return license_error("license does not exists");
        if (fs::file_size(root_directory / name) > 512 * 1024)
            return license_error("license is invalid (should be text/plain and less than 512 KB)");
        return true;
    };

    if (!pkg.flags[pfLocalProject])
    {
        if (!license.empty())
        {
            if (check_license(license))
                files.insert(license);
        }
        else
        {
            String error;
            auto try_license = [&error, &check_license, this](auto &lic)
            {
                if (check_license(lic, &error))
                {
                    files.insert(lic);
                    return true;
                }
                return false;
            };
            if (try_license("LICENSE") ||
                try_license("COPYING") ||
                try_license("LICENSE.txt") ||
                try_license("license.txt") ||
                try_license("LICENSE.md"))
                (void)error;
        }
    }

    if (!root_directory.empty() && !pkg.flags[pfLocalProject])
        fs::copy_file(CPPAN_FILENAME, root_directory / CPPAN_FILENAME, fs::copy_option::overwrite_if_exists);
    files.insert(CPPAN_FILENAME);
}

bool Project::writeArchive(const path &fn) const
{
    ScopedCurrentPath cp(root_directory);
    return pack_files(fn, files, cp.get_cwd());
}

void Project::save_dependencies(yaml &node) const
{
    if (dependencies.empty())
        return;

    yaml root;
    for (auto &dd : dependencies)
    {
        auto &d = dd.second;
        yaml n;
        if (d.flags[pfPrivateDependency])
            n = root["private"];
        else
            n = root["public"];

        // always save as map
        yaml n2;
        n2["version"] = d.version.toAnyVersion();

        if (!d.reference.empty())
            n2["reference"] = d.reference;
        if (d.flags[pfIncludeDirectoriesOnly])
            n2[INCLUDE_DIRECTORIES_ONLY] = true;

        n[dd.first] = n2;
    }
    node[DEPENDENCIES_NODE] = root;
}

ProjectPath Project::relative_name_to_absolute(const String &name)
{
    ProjectPath ppath;
    if (name.empty())
        return ppath;
    if (ProjectPath(name).is_relative())
    {
        auto ld = load_local_dependency(name);
        if (ld)
            return ld.value();
        if (allow_relative_project_names)
        {
            ppath.push_back(name);
            return ppath;
        }
        if (root_project.empty())
            throw std::runtime_error("You're using relative names, but 'root_project' is missing");
        ppath = root_project / name;
    }
    else
        ppath = name;
    return ppath;
}

optional<ProjectPath> Project::load_local_dependency(const String &name)
{
    optional<ProjectPath> pp;
    if (allow_local_dependencies && (fs::exists(name) || isUrl(name)))
    {
        std::set<Package> pkgs;
        Config c;
        String n;
        std::tie(pkgs, c, n) = rd.read_packages_from_file(name);
        pp = c.pkg.ppath;
    }
    return pp;
}

void Project::load(const yaml &root)
{
    load_source_and_version(root, source, pkg.version);

    EXTRACT_AUTO(empty);
    EXTRACT_AUTO(custom);

    EXTRACT_AUTO(shared_only);
    EXTRACT_AUTO(static_only);
    EXTRACT_VAR(root, header_only, "header_only", bool);

    if (shared_only && static_only)
        throw std::runtime_error("Project cannot be static and shared simultaneously");

    EXTRACT_AUTO(import_from_bazel);
    EXTRACT_AUTO(prefer_binaries);
    EXTRACT_AUTO(export_all_symbols);
    EXTRACT_AUTO(build_dependencies_with_same_config);

    EXTRACT_AUTO(api_name);

    // standards
    {
        EXTRACT_AUTO(c_standard);
        if (c_standard == 0)
        {
            EXTRACT_VAR(root, c_standard, "c", int);
        }

        String cxx;
        EXTRACT_VAR(root, cxx, "cxx_standard", String);
        if (cxx.empty())
            EXTRACT_VAR(root, cxx, "c++", String);

        if (!cxx.empty())
        {
            try
            {
                cxx_standard = std::stoi(cxx);
            }
            catch (const std::exception&)
            {
                if (cxx == "1z")
                    cxx_standard = 17;
                else if (cxx == "2x")
                    cxx_standard = 20;
            }
        }
    }

    license = get_scalar<String>(root, "license");

    auto read_dir = [&root](auto &p, const String &s)
    {
        get_scalar_f(root, s, [&p, &s](const auto &n)
        {
            auto cp = fs::current_path();
            p = n.template as<String>();
            if (!is_under_root(cp / p, cp))
                throw std::runtime_error("'" + s + "' must not point outside the current dir: " + p.string() + ", " + cp.string());
        });
    };

    read_dir(root_directory, "root_directory");
    if (root_directory.empty())
        read_dir(root_directory, "root_dir");
    read_dir(unpack_directory, "unpack_directory");
    if (unpack_directory.empty())
        read_dir(unpack_directory, "unpack_dir");

    // include_directories
    {
        get_variety(root, "include_directories",
            [this](const auto &d)
        {
            include_directories.public_.insert(d.template as<String>());
        },
            [this](const auto &dall)
        {
            for (auto d : dall)
                include_directories.public_.insert(d.template as<String>());
        },
            [this, &root](const auto &)
        {
            get_map_and_iterate(root, "include_directories", [this](const auto &n)
            {
                auto f = n.first.template as<String>();
                auto s = get_sequence<String>(n.second);
                if (f == "public")
                    include_directories.public_.insert(s.begin(), s.end());
                else if (f == "private")
                    include_directories.private_.insert(s.begin(), s.end());
                else if (f == "interface")
                    include_directories.interface_.insert(s.begin(), s.end());
                else
                    throw std::runtime_error("include key must be only 'public' or 'private' or 'interface'");
            });
        });
    }

    bs_insertions.load(root);
    options = loadOptionsMap(root);

    // deps
    {
        auto read_version = [](auto &dependency, const String &v)
        {
            if (!dependency.flags[pfLocalProject])
            {
                dependency.version = v;
                return;
            }

            if (rd.has_local_package(dependency.ppath))
                dependency.version = Version(LOCAL_VERSION_NAME);
            else
            {
                auto nppath = dependency.ppath / v;
                if (rd.has_local_package(nppath))
                {
                    dependency.ppath = nppath;
                    dependency.version = Version(LOCAL_VERSION_NAME);
                }
                else
                    throw std::runtime_error("Unknown local dependency: " + nppath.toString());
            }
        };

        auto read_single_dep = [this, &read_version](auto &deps, const auto &d, Package dependency = Package())
        {
            if (d.IsScalar())
            {
                dependency.ppath = this->relative_name_to_absolute(d.template as<String>());
            }
            else if (d.IsMap())
            {
                // read only field related to ppath - name, local
                if (d["name"].IsDefined())
                    dependency.ppath = this->relative_name_to_absolute(d["name"].template as<String>());
                if (d["package"].IsDefined())
                    dependency.ppath = this->relative_name_to_absolute(d["package"].template as<String>());
                if (dependency.ppath.empty() && d.size() == 1)
                {
                    dependency.ppath = this->relative_name_to_absolute(d.begin()->first.template as<String>());
                    if (dependency.ppath.is_loc())
                        dependency.flags.set(pfLocalProject);
                    read_version(dependency, d.begin()->second.template as<String>());
                }
                if (d["local"].IsDefined() && allow_local_dependencies)
                {
                    // WARNING!
                    // probably this could be dangerous, maybe remove?
                    // if set local dep for a secure file on the system
                    // it will be read (or not?); how this affects system?
                    // will not lead to exec shell code somehow or whatever?
                    // ???
                    auto lp = d["local"].template as<String>();
                    auto ld = this->load_local_dependency(lp);
                    if (!ld)
                    {
                        if (!dependency.ppath.empty() && !dependency.ppath.is_loc())
                        {
                            try
                            {
                                Packages p;
                                p[dependency.ppath.toString()] = dependency;
                                resolve_dependencies(p);
                            }
                            catch (const std::exception &)
                            {
                                // if not resolved, fail finally
                                throw;
                            }
                        }

                        if (dependency.ppath.empty())
                            throw std::runtime_error("Could not load local project: " + lp);
                    }
                    dependency.ppath = ld.value();
                }
            }

            if (dependency.ppath.is_loc())
            {
                dependency.flags.set(pfLocalProject);

                // version will be read for local project
                // even 2nd arg is not valid
                read_version(dependency, d["version"].template as<String>());
            }

            if (d.IsMap())
            {
                // read other map fields
                if (d["version"].IsDefined())
                    read_version(dependency, d["version"].template as<String>());
                if (d["ref"].IsDefined())
                    dependency.reference = d["ref"].template as<String>();
                if (d["reference"].IsDefined())
                    dependency.reference = d["reference"].template as<String>();
                if (d[INCLUDE_DIRECTORIES_ONLY].IsDefined())
                    dependency.flags.set(pfIncludeDirectoriesOnly, d[INCLUDE_DIRECTORIES_ONLY].template as<bool>());
            }

            if (dependency.flags[pfLocalProject])
                dependency.createNames();

            deps[dependency.ppath.toString()] = dependency;
        };

        get_variety(root, DEPENDENCIES_NODE,
            [this, &read_single_dep](const auto &d)
        {
            read_single_dep(dependencies, d);
        },
            [this, &read_single_dep](const auto &dall)
        {
            for (auto d : dall)
                read_single_dep(dependencies, d);
        },
            [this, &read_single_dep, &read_version](const auto &dall)
        {
            auto get_dep = [this, &read_version, &read_single_dep](auto &deps, const auto &d)
            {
                Package dependency;

                dependency.ppath = this->relative_name_to_absolute(d.first.template as<String>());
                if (dependency.ppath.is_loc())
                    dependency.flags.set(pfLocalProject);

                if (d.second.IsScalar())
                    read_version(dependency, d.second.template as<String>());
                else if (d.second.IsMap())
                {
                    read_single_dep(deps, d.second, dependency);
                    return;
                }
                else
                    throw std::runtime_error("Dependency should be a scalar or a map");

                if (dependency.flags[pfLocalProject])
                    dependency.createNames();

                deps[dependency.ppath.toString()] = dependency;
            };

            auto extract_deps = [&dall, this, &get_dep, &read_single_dep](const auto &str, auto &deps)
            {
                auto priv = dall[str];
                if (!priv.IsDefined())
                    return;
                if (priv.IsMap())
                {
                    get_map_and_iterate(dall, str,
                        [this, &get_dep, &deps](const auto &d)
                    {
                        get_dep(deps, d);
                    });
                }
                else if (priv.IsSequence())
                {
                    for (auto d : priv)
                        read_single_dep(deps, d);
                }
            };

            Packages dependencies_private;
            extract_deps("private", dependencies_private);
            extract_deps("public", dependencies);

            for (auto &d : dependencies_private)
            {
                d.second.flags.set(pfPrivateDependency);
                dependencies.insert(d);
            }

            if (dependencies.empty() && dependencies_private.empty())
            {
                for (auto d : dall)
                    get_dep(dependencies, d);
            }
        });
    }

    auto read_sources = [&root](auto &a, const String &key, bool required = true)
    {
        a.clear();
        auto files = root[key];
        if (!files.IsDefined())
            return;
        if (files.IsScalar())
        {
            a.insert(files.as<String>());
        }
        else if (files.IsSequence())
        {
            for (const auto &v : files)
                a.insert(v.as<String>());
        }
        else if (files.IsMap())
        {
            for (const auto &group : files)
            {
                if (group.second.IsScalar())
                    a.insert(group.second.as<String>());
                else if (group.second.IsSequence())
                {
                    for (const auto &v : group.second)
                        a.insert(v.as<String>());
                }
                else if (group.second.IsMap())
                {
                    String root = get_scalar<String>(group.second, "root");
                    auto v = get_sequence<String>(group.second, "files");
                    for (auto &e : v)
                        a.insert(root + "/" + e);
                }
            }
        }
    };

    read_sources(sources, "files");
    read_sources(build_files, "build");
    read_sources(exclude_from_package, "exclude_from_package");
    read_sources(exclude_from_build, "exclude_from_build");

    aliases = get_sequence_set<String>(root, "aliases");
    checks.load(root);

    auto patch_node = root["patch"];
    if (patch_node.IsDefined())
        patch.load(patch_node);

    EXTRACT_AUTO(name);

    String pt;
    EXTRACT_VAR(root, pt, "type", String);
    if (pt == "l" || pt == "lib" || pt == "library")
        type = ProjectType::Library;
    else if (pt == "e" || pt == "exe" || pt == "executable")
        type = ProjectType::Executable;

    String lt;
    EXTRACT_VAR(root, lt, "library_type", String);
    if (lt == "static")
    {
        library_type = LibraryType::Static;
        static_only = true;
    }
    if (lt == "shared" || lt == "dll")
    {
        library_type = LibraryType::Shared;
        shared_only = true;
    }
    if (lt == "module")
    {
        library_type = LibraryType::Module;
        shared_only = true;
    }

    String et;
    EXTRACT_VAR(root, et, "executable_type", String);
    if (et == "win32")
        executable_type = ExecutableType::Win32;

    // after loading process input data where it's necessary

    // we also store original data in ptr
    // this is useful for printing original config (project)
    original_project = std::make_shared<Project>(*this);

    // we're trying to find root directory
    // to make some following default checks available
    // try to detect and prepend root dir
    {
        auto root = findRootDirectory();
        if (root_directory.empty())
            root_directory = root;
        else if (root_directory != root)
            root_directory = root / root_directory;
    }

    // idirs
    if (defaults_allowed && include_directories.public_.empty())
    {
        if (fs::exists("include"))
            include_directories.public_.insert("include");
        else
        {
            if (fs::exists(root_directory / "include"))
                include_directories.public_.insert(normalize_path(root_directory / "include"));
            else
            {
                include_directories.public_.insert(".");
                // one case left: root_directory / "."
            }
        }
    }
    if (defaults_allowed && include_directories.private_.empty())
    {
        std::function<void(const String &, const String &)> autodetect_source_dir;
        autodetect_source_dir = [this, &autodetect_source_dir](const String &current, const String &next = String())
        {
            if (fs::exists(current))
            {
                if (fs::exists("include"))
                    include_directories.private_.insert(current);
                else
                    include_directories.public_.insert(current);
            }
            else
            {
                if (fs::exists(root_directory / current))
                {
                    if (fs::exists(root_directory / "include"))
                        include_directories.private_.insert(normalize_path(root_directory / current));
                    else
                    {
                        include_directories.public_.insert(current);
                        // one case left: root_directory / "src"
                    }
                }
                else
                {
                    // now check next dir
                    if (!next.empty())
                        autodetect_source_dir(next, "");
                }
            }
        };
        autodetect_source_dir("src", "lib");
    }
    include_directories.public_.insert("${BDIR}");

    // files
    files_loaded = root["files"].IsDefined() && !sources.empty();
    if (defaults_allowed && sources.empty())
    {
        // try to add some default dirs
        // root_directory will be removed (entered),
        // so do not insert like 'insert(root_directory / "dir/.*");'
        if (fs::exists(root_directory / "include"))
            sources.insert("include/.*");
        if (fs::exists(root_directory / "src"))
            sources.insert("src/.*");
        else if (fs::exists(root_directory / "lib"))
            sources.insert("lib/.*");

        if (sources.empty())
        {
            // no include, source dirs
            // try to add all types of C/C++ program files to gather
            // regex means all sources in root dir (without slashes '/')
            auto r_replace = [](auto &s)
            {
                return boost::replace_all_copy(s, "+", "\\+");
            };
            for (auto &v : header_file_extensions)
                sources.insert("[^/]*\\" + r_replace(v));
            for (auto &v : source_file_extensions)
                sources.insert("[^/]*\\" + r_replace(v));
        }
    }
    if (import_from_bazel)
    {
        for (auto &bfn : bazel_filenames)
            exclude_from_build.insert(bfn);
    }
}

yaml Project::save() const
{
    if (original_project)
        return original_project->save();

    yaml root;

#define ADD_IF_VAL(x, c, v) if (c) root[#x] = v
#define ADD_IF_VAL_TRIPLE(x) ADD_IF_VAL(x, x, x)
#define ADD_IF_NOT_EMPTY_VAL(x, v) ADD_IF_VAL(x, !x.empty(), v)
#define ADD_IF_NOT_EMPTY(x) ADD_IF_NOT_EMPTY_VAL(x, x)
#define ADD_IF_EQU_VAL(x, e, v) ADD_IF_VAL(x, (x) == (e), v)
#define ADD_SET(x, s) for (auto &v : s) root[#x].push_back(v)

    if (isValidSourceUrl(source))
        save_source(root, source);
    if (pkg.version.isValid() &&
        (pkg.version.type == VersionType::Version ||
         pkg.version.type == VersionType::Branch))
        root["version"] = pkg.version.toString();

    ADD_IF_NOT_EMPTY(name);
    ADD_IF_NOT_EMPTY(license);

    ADD_IF_EQU_VAL(type, ProjectType::Library, "library");
    ADD_IF_EQU_VAL(library_type, LibraryType::Shared, "shared");
    ADD_IF_EQU_VAL(library_type, LibraryType::Module, "module");
    ADD_IF_EQU_VAL(executable_type, ExecutableType::Win32, "win32");

    ADD_IF_NOT_EMPTY_VAL(root_directory, normalize_path(root_directory));
    ADD_IF_NOT_EMPTY_VAL(unpack_directory, normalize_path(unpack_directory));

    if (c_standard)
        root["c"] = c_standard;
    if (cxx_standard)
        root["c++"] = cxx_standard;

    ADD_IF_VAL_TRIPLE(empty);
    ADD_IF_VAL_TRIPLE(custom);

    ADD_IF_VAL_TRIPLE(static_only);
    ADD_IF_VAL_TRIPLE(shared_only);
    if (header_only)
        root["header_only"] = header_only.get();

    ADD_IF_VAL_TRIPLE(import_from_bazel);
    ADD_IF_VAL_TRIPLE(prefer_binaries);
    ADD_IF_VAL_TRIPLE(export_all_symbols);
    ADD_IF_VAL_TRIPLE(build_dependencies_with_same_config);

    ADD_IF_NOT_EMPTY(api_name);

    ADD_SET(files, sources);
    ADD_SET(build, build_files);
    ADD_SET(exclude_from_package, exclude_from_package);
    ADD_SET(exclude_from_build, exclude_from_build);

    for (auto &v : include_directories.public_)
        root["include_directories"]["public"].push_back(normalize_path(v));
    for (auto &v : include_directories.private_)
        root["include_directories"]["private"].push_back(normalize_path(v));
    for (auto &v : include_directories.interface_)
        root["include_directories"]["interface"].push_back(normalize_path(v));
    saveOptionsMap(root, options);
    ADD_SET(aliases, aliases);
    checks.save(root);
    save_dependencies(root);
    patch.save(root);
    bs_insertions.save(root);

    return root;
}

void Project::prepareExports() const
{
    // very stupid algorithm
    auto api = CPPAN_EXPORT_PREFIX + pkg.variable_name;

    auto &srcs = getSources();
    for (auto &f : srcs)
    {
        auto s = read_file(f, true);

        boost::algorithm::replace_all(s, CPPAN_EXPORT, api);

        String p, e;
        std::vector<String> ev;
        for (auto &n : pkg.ppath)
        {
            p += "namespace " + n + " {\n";
            ev.push_back("} // namespace " + n + "\n");
        }
        std::reverse(ev.begin(), ev.end());
        for (auto &n : ev)
            e += n;

        boost::algorithm::replace_all(s, CPPAN_PROLOG, p);
        boost::algorithm::replace_all(s, CPPAN_EPILOG, e);

        write_file_if_different(f, s);
    }
}

void Project::patchSources() const
{
    auto &srcs = getSources();
    for (auto &f : srcs)
    {
        auto s = read_file(f, true);
        for (auto &p : patch.replace_in_files)
            boost::algorithm::replace_all(s, p.first, p.second);
        write_file_if_different(f, s);
    }
}

const Files &Project::getSources() const
{
    if (!files.empty())
        return files;
    for (auto &f : boost::make_iterator_range(fs::recursive_directory_iterator(pkg.getDirSrc()), {}))
    {
        if (!fs::is_regular_file(f) || f.path().filename() == CPPAN_FILENAME)
            continue;
        files.insert(f);
    }
    return files;
}

void Project::setRelativePath(const String &name)
{
    pkg.ppath = relative_name_to_absolute(name);
}

void Project::applyFlags(ProjectFlags &flags) const
{
    flags.set(pfExecutable, type == ProjectType::Executable);
}

void Project::addDependency(const Package &p)
{
    auto i = dependencies.insert({ p.ppath.toString(), p });
    i.first->second.createNames();
}

OptionsMap loadOptionsMap(const yaml &root)
{
    OptionsMap options;
    if (root["options"].IsDefined() && !root["options"].IsMap())
        return options;
    get_map_and_iterate(root, "options", [&options](const auto &opt_level)
    {
        auto ol = opt_level.first.template as<String>();
        if (!(ol == "any" || ol == "static" || ol == "shared"))
            throw std::runtime_error("Wrong option level dicrective");
        if (!opt_level.second.IsMap())
            throw std::runtime_error("'" + ol + "' should be a map");

        auto &option = options[ol];

        auto add_opts = [](const auto &defs, const auto &s, auto &c)
        {
            if (!defs.IsDefined())
                return;
            auto dd = get_sequence_set<String, String>(defs, s);
            for (auto &d : dd)
                c.insert({ s,d });
        };
        auto add_opts_common = [&add_opts](const auto &opts, auto &c, auto &sc)
        {
            add_opts(opts, "public", c);
            add_opts(opts, "private", c);
            add_opts(opts, "interface", c);

            for (auto kv : opts)
            {
                auto f = kv.first.template as<String>();
                if (f == "public" || f == "private" || f == "interface")
                    continue;
                auto &scv = sc[f];
                add_opts(kv.second, "public", scv);
                add_opts(kv.second, "private", scv);
                add_opts(kv.second, "interface", scv);
            }
        };
        add_opts_common(opt_level.second["definitions"], option.definitions, option.system_definitions);
        add_opts_common(opt_level.second["include_directories"], option.include_directories, option.system_include_directories);
        add_opts_common(opt_level.second["compile_options"], option.compile_options, option.system_compile_options);
        add_opts_common(opt_level.second["link_options"], option.link_options, option.system_link_options);
        add_opts_common(opt_level.second["link_libraries"], option.link_libraries, option.system_link_libraries);

        option.link_directories = get_sequence_set<String, String>(opt_level.second, "link_directories");

        option.bs_insertions.load(opt_level.second);
    });
    return options;
}

void saveOptionsMap(yaml &node, const OptionsMap &m)
{
    if (m.empty())
        return;

    yaml root;
    for (auto &ol : m)
    {
        auto &o = ol.second;

#define ADD_OPT(x) \
        for (auto &v : o.x) \
            root[ol.first][#x][v.first].push_back(v.second)
#define ADD_OPT_SYS(x) \
        for (auto &v1 : o.system_##x) \
            for (auto &v : v1.second) \
                root[ol.first][#x][v1.first][v.first].push_back(v.second)

        ADD_OPT(definitions);
        ADD_OPT(include_directories);
        ADD_OPT(compile_options);
        ADD_OPT(link_options);
        ADD_OPT(link_libraries);

        ADD_OPT_SYS(definitions);
        ADD_OPT_SYS(include_directories);
        ADD_OPT_SYS(compile_options);
        ADD_OPT_SYS(link_options);
        ADD_OPT_SYS(link_libraries);

        for (auto &v : o.link_directories)
            root[ol.first]["link_directories"].push_back(v);

        auto n = root[ol.first];
        o.bs_insertions.save(n);
    }
    node["options"] = root;
}
