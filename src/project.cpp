/*
 * Copyright (c) 2016, Egor Pugin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *     3. Neither the name of the copyright holder nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "project.h"

#include "bazel/bazel.h"
#include "config.h"

#ifdef _WIN32
#include <windows.h>

#include <libarchive/archive.h>
#include <libarchive/archive_entry.h>
#else
#include <archive.h>
#include <archive_entry.h>
#endif

#include <boost/algorithm/string.hpp>

#include <regex>

using MimeType = String;
using MimeTypes = std::set<MimeType>;

static const MimeTypes source_mime_types{
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

static const std::set<String> header_file_extensions{
    ".h",
    ".hh",
    ".hpp",
    ".hxx",
    ".h++",
    ".HPP",
};

static const std::set<String> source_file_extensions{
    ".c",
    ".cc",
    ".cpp",
    ".cxx",
    ".c++",
    ".CPP",
};

static const std::set<String> other_source_file_extensions{
    ".s",
    ".S",
    ".asm",
    ".ipp",
};

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
    auto fret = system_with_output("file -ib " + p.string());
    auto s = std::get<1>(fret);
    return is_valid_file_type(types, p, s, error, check_ext);
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

void check_file_types(const Files &files, const path &root)
{
    if (files.empty())
        return;

    String errors;
    for (auto &file : files)
    {
        auto s = (root / file).string();
        if (!check_filename(s))
            errors += "File '" + s + "' has prohibited symbols\n";
    }
    if (!errors.empty())
        throw std::runtime_error("Project sources did not pass file checks:\n" + errors);

    auto fn = get_temp_filename();
    std::ofstream o(fn.string(), std::ios::binary | std::ios::out);
    if (!o)
        throw std::runtime_error("Cannot open file for writing: " + fn.string());
    auto cwd = fs::current_path();
    for (auto &file : files)
    {
        auto s = (cwd / root / file).string();
        std::replace(s.begin(), s.end(), '\\', '/');
        o << "file -ib " << s << "\n";
    }
    o.close();

    auto fn2 = get_temp_filename();
    system(("sh " + fn.string() + " > " + fn2.string()).c_str());
    fs::remove(fn);
    std::ifstream ifile(fn2.string());
    if (!ifile)
        throw std::runtime_error("Cannot open file for reading: " + fn2.string());
    std::vector<String> lines;
    String s;
    while (std::getline(ifile, s))
    {
        if (!s.empty())
            lines.push_back(s);
    }
    ifile.close();
    fs::remove(fn2);
    if (lines.size() != files.size())
        throw std::runtime_error("Error during file checking");

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

ProjectPath relative_name_to_absolute(const ProjectPath &root_project, const String &name)
{
    ProjectPath ppath;
    if (name.empty())
        return ppath;
    if (ProjectPath(name).is_relative())
    {
        if (root_project.empty())
            throw std::runtime_error("You're using relative names, but 'root_project' is missing");
        ppath = root_project / name;
    }
    else
        ppath = name;
    return ppath;
}

Project::Project(const ProjectPath &root_project)
    : root_project(root_project)
{
}

void Project::findSources(path p)
{
    // try to auto choose root_directory
    {
        std::vector<path> pfiles;
        std::vector<path> pdirs;
        for (auto &pi : boost::make_iterator_range(fs::directory_iterator(p), {}))
        {
            auto f = pi.path().filename().string();
            if (f == CPPAN_FILENAME)
                continue;
            if (fs::is_regular_file(pi))
                pfiles.push_back(pi);
            else if (fs::is_directory(pi))
                pdirs.push_back(pi);
        }
        if (pfiles.empty() && pdirs.size() == 1 && root_directory.empty())
            root_directory = fs::relative(*pdirs.begin(), p);
    }

    p /= root_directory;

    if (import_from_bazel)
    {
        auto b = read_file(p / BAZEL_BUILD_FILE);
        auto f = bazel::parse(b);
        String project_name;
        if (!ppath.empty())
            project_name = ppath.back();
        auto files = f.getFiles(project_name);
        sources.insert(files.begin(), files.end());
        sources.insert(BAZEL_BUILD_FILE);
    }

    for (auto i = sources.begin(); i != sources.end();)
    {
        if (fs::exists(p / *i))
        {
            files.insert(*i);
            sources.erase(i++);
            continue;
        }
        ++i;
    }

    if ((sources.empty() && files.empty()) && !empty)
        throw std::runtime_error("'files' must be populated");

    std::map<String, std::regex> rgxs, rgxs_exclude;
    for (auto &e : sources)
        rgxs[e] = std::regex(e);
    for (auto &e : exclude_from_package)
        rgxs_exclude[e] = std::regex(e);

    if (!rgxs.empty())
    {
        for (auto &f : boost::make_iterator_range(fs::recursive_directory_iterator(p), {}))
        {
            if (!fs::is_regular_file(f))
                continue;

            String s = fs::relative(f.path(), p).string();
            std::replace(s.begin(), s.end(), '\\', '/');

            for (auto &e : rgxs)
            {
                if (std::regex_match(s, e.second))
                    files.insert(s);
            }
        }
    }

    if (!rgxs_exclude.empty())
    {
        auto to_remove = files;
        for (auto &f : files)
        {
            String s = fs::relative(f, p).string();
            std::replace(s.begin(), s.end(), '\\', '/');

            for (auto &e : rgxs_exclude)
            {
                if (std::regex_match(s, e.second))
                {
                    to_remove.erase(f);
                    break;
                }
            }
        }
        files = to_remove;
    }

    if (files.empty() && !empty)
        throw std::runtime_error("no files found");

    // disable on windows during testing
#ifndef WIN32
    check_file_types(files, root_directory);
#endif

    if (!header_only) // do not check if forced header_only
        header_only = std::none_of(files.begin(), files.end(), is_valid_source);

    if (!license.empty())
    {
        if (!fs::exists(root_directory / license))
            throw std::runtime_error("License does not exists");
        if (fs::file_size(root_directory / license) > 512 * 1024)
            throw std::runtime_error("license is invalid (should be text/plain and less than 512 KB)");
        files.insert(license);
    }

    if (!root_directory.empty())
        fs::copy_file(cppan_filename, root_directory / cppan_filename, fs::copy_option::overwrite_if_exists);
    files.insert(cppan_filename);
}

bool Project::writeArchive(const String &filename) const
{
    bool result = true;
    auto a = archive_write_new();
    archive_write_add_filter_gzip(a);
    archive_write_set_format_pax_restricted(a);
    archive_write_open_filename(a, filename.c_str());
    for (auto &f : files)
    {
        auto fn = f.string();
        auto fn_real = root_directory / f;
        if (!fs::exists(fn_real))
        {
            result = false;
            continue;
        }
        auto sz = fs::file_size(fn_real);
        auto e = archive_entry_new();
        archive_entry_set_pathname(e, fn.c_str());
        archive_entry_set_size(e, sz);
        archive_entry_set_filetype(e, AE_IFREG);
        archive_entry_set_perm(e, 0644);
        archive_write_header(a, e);
        auto fp = fopen(fn_real.string().c_str(), "rb");
        if (!fp)
        {
            archive_entry_free(e);
            result = false;
            continue;
        }
        char buff[8192];
        size_t len;
        len = fread(buff, 1, sizeof(buff), fp);
        while (len > 0)
        {
            archive_write_data(a, buff, len);
            len = fread(buff, 1, sizeof(buff), fp);
        }
        fclose(fp);
        archive_entry_free(e);
    }
    archive_write_close(a);
    archive_write_free(a);
    return result;
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
        if (d.flags[pfIncludeDirectoriesOnly])
        {
            yaml n2;
            n2["version"] = d.version.toAnyVersion();
            n2[INCLUDE_DIRECTORIES_ONLY] = true;
            n[dd.first] = n2;
        }
        else
            n[dd.first] = d.version.toAnyVersion();
    }
    node[DEPENDENCIES_NODE] = root;
}

void Project::load(const yaml &root)
{
    source = load_source(root);

    EXTRACT_VAR(root, empty, "empty", bool);

    EXTRACT_VAR(root, shared_only, "shared_only", bool);
    EXTRACT_VAR(root, static_only, "static_only", bool);
    EXTRACT_VAR(root, header_only, "header_only", bool);

    EXTRACT_VAR(root, import_from_bazel, "import_from_bazel", bool);

    EXTRACT_VAR(root, copy_to_output_dir, "copy_to_output_dir", bool);

    // standards
    {
        EXTRACT_AUTO(c_standard);
        if (c_standard == 0)
        {
            EXTRACT_VAR(root, c_standard, "c", int);
        }
        EXTRACT_AUTO(cxx_standard);
        if (cxx_standard == 0)
        {
            EXTRACT_VAR(root, cxx_standard, "c++", int);
        }
    }

    if (shared_only && static_only)
        throw std::runtime_error("Project cannot be static and shared simultaneously");

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
    read_dir(unpack_directory, "unpack_directory");

    get_map_and_iterate(root, "include_directories", [this](const auto &n)
    {
        auto f = n.first.template as<String>();
        if (f == "public")
        {
            auto s = get_sequence<String>(n.second);
            include_directories.public_.insert(s.begin(), s.end());
        }
        else if (f == "private")
        {
            auto s = get_sequence<String>(n.second);
            include_directories.private_.insert(s.begin(), s.end());
        }
        else
            throw std::runtime_error("include key must be only 'public' or 'private'");
    });
    if (include_directories.public_.empty())
    {
        if (fs::exists("include"))
            include_directories.public_.insert("include");
        else
            include_directories.public_.insert(".");
    }
    if (include_directories.private_.empty())
    {
        if (fs::exists("src"))
            include_directories.private_.insert("src");
    }
    include_directories.public_.insert("${CMAKE_CURRENT_BINARY_DIR}");

    bs_insertions.get_config_insertions(root);

    get_map_and_iterate(root, "options", [this](const auto &opt_level)
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
        add_opts_common(opt_level.second["compile_options"], option.compile_options, option.system_compile_options);
        add_opts_common(opt_level.second["link_options"], option.link_options, option.system_link_options);
        add_opts_common(opt_level.second["link_libraries"], option.link_libraries, option.system_link_libraries);

        option.include_directories = get_sequence_set<String, String>(opt_level.second, "include_directories");
        option.link_directories = get_sequence_set<String, String>(opt_level.second, "link_directories");
        option.global_definitions = get_sequence_set<String, String>(opt_level.second, "global_definitions");

        option.bs_insertions.get_config_insertions(opt_level.second);
    });

    auto read_single_dep = [this](auto &deps, const auto &d)
    {
        if (d.IsScalar())
        {
            Package dependency;
            dependency.ppath = relative_name_to_absolute(root_project, d.template as<String>());
            deps[dependency.ppath.toString()] = dependency;
        }
        else if (d.IsMap())
        {
            Package dependency;
            if (d["name"].IsDefined())
                dependency.ppath = relative_name_to_absolute(root_project, d["name"].template as<String>());
            if (d["package"].IsDefined())
                dependency.ppath = relative_name_to_absolute(root_project, d["package"].template as<String>());
            if (d["version"].IsDefined())
                dependency.version = d["version"].template as<String>();
            if (d[INCLUDE_DIRECTORIES_ONLY].IsDefined())
                dependency.flags.set(pfIncludeDirectoriesOnly, d[INCLUDE_DIRECTORIES_ONLY].template as<bool>());
            deps[dependency.ppath.toString()] = dependency;
        }
    };

    get_variety(root, DEPENDENCIES_NODE,
        [this](const auto &d)
    {
        Package dependency;
        dependency.ppath = relative_name_to_absolute(root_project, d.template as<String>());
        dependencies[dependency.ppath.toString()] = dependency;
    },
        [this, &read_single_dep](const auto &dall)
    {
        for (auto d : dall)
            read_single_dep(dependencies, d);
    },
        [this, &read_single_dep](const auto &dall)
    {
        auto get_dep = [this](auto &deps, const auto &d)
        {
            Package dependency;
            dependency.ppath = relative_name_to_absolute(root_project, d.first.template as<String>());
            if (d.second.IsScalar())
                dependency.version = d.second.template as<String>();
            else if (d.second.IsMap())
            {
                for (const auto &v : d.second)
                {
                    auto key = v.first.template as<String>();
                    if (key == "version")
                        dependency.version = v.second.template as<String>();
                    else if (key == INCLUDE_DIRECTORIES_ONLY)
                        dependency.flags.set(pfIncludeDirectoriesOnly, v.second.template as<bool>());
                    // TODO: re-enable when adding patches support
                    //else if (key == "package_dir")
                    //    dependency.package_dir_type = packages_dir_type_from_string(v.second.template as<String>());
                    //else if (key == "patches")
                    //{
                    //    for (const auto &p : v.second)
                    //        dependency.patches.push_back(template as<String>());
                    //}
                    else
                        throw std::runtime_error("Unknown key: " + key);
                }
            }
            else
                throw std::runtime_error("Dependency should be a scalar or a map");
            deps[dependency.ppath.toString()] = dependency;
        };

        Packages dependencies_private;

        auto extract_deps = [&dall, this, &get_dep, &read_single_dep](const auto &str, auto &deps)
        {
            auto &priv = dall[str];
            if (priv.IsDefined())
            {
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
            }
        };

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
            {
                get_dep(dependencies, d);
            }
        }
    });

    auto read_sources = [&root](auto &a, const String &key, bool required = true)
    {
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
    if (import_from_bazel)
        exclude_from_build.insert(BAZEL_BUILD_FILE);

    aliases = get_sequence_set<String>(root, "aliases");
}

void Project::prepareExports() const
{
    // very stupid algorithm
    auto api = CPPAN_EXPORT_PREFIX + pkg.variable_name;

    for (auto &f : boost::make_iterator_range(fs::recursive_directory_iterator(pkg.getDirSrc()), {}))
    {
        if (!fs::is_regular_file(f) || f.path().filename() == CPPAN_FILENAME)
            continue;
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
