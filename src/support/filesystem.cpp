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

#include "filesystem.h"

path get_config_filename()
{
    return get_root_directory() / CPPAN_FILENAME;
}

path get_root_directory()
{
    return get_home_directory() / ".cppan";
}

path temp_directory_path(const path &subdir)
{
    auto p = fs::temp_directory_path() / "cppan" / subdir;
    fs::create_directories(p);
    return p;
}

path get_temp_filename(const path &subdir)
{
    return temp_directory_path(subdir) / fs::unique_path();
}

String get_stamp_filename(const String &prefix)
{
    return prefix + ".hash";
}

String make_archive_name(const String &fn)
{
    if (!fn.empty())
        return fn + ".tar.gz";
    return "cppan.tar.gz";
}

void findRootDirectory1(const path &p, path &root, int depth = 0)
{
    // limit recursion
    if (depth++ > 10)
        return;

    std::vector<path> pfiles;
    std::vector<path> pdirs;
    for (auto &pi : boost::make_iterator_range(fs::directory_iterator(p), {}))
    {
        auto f = pi.path().filename().string();
        if (f == CPPAN_FILENAME)
            continue;
        if (fs::is_regular_file(pi))
        {
            pfiles.push_back(pi);
            break;
        }
        else if (fs::is_directory(pi))
        {
            pdirs.push_back(pi);
            if (pdirs.size() > 1)
                break;
        }
    }
    if (pfiles.empty() && pdirs.size() == 1)
    {
        auto d = fs::relative(*pdirs.begin(), p);
        root /= d;
        findRootDirectory1(p / d, root);
    }
	else if (depth == 1)
	{
		root = p;
	}
}

path findRootDirectory(const path &p)
{
    path root;
    findRootDirectory1(p, root);
    return root;
}
