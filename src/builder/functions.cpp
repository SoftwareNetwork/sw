// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "functions.h"

#include "file.h"

#include <primitives/hash.h>
#include <primitives/lock.h>

#include <boost/algorithm/string.hpp>

namespace sw
{

void fileWriteOnce(const path &fn, const String &content, const path &lock_dir)
{
    auto h = sha1(content);

    auto hf = sha1(fn.string());
    const auto once = lock_dir / (hf + ".once");
    const auto lock = lock_dir / hf;

    if (!fs::exists(once) || h != read_file(once) || !fs::exists(fn))
    {
        ScopedFileLock fl(lock);
        write_file(fn, content);
        write_file(once, h);

        File f(fn);
        f.getFileRecord().load();
    }
}

void fileWriteSafe(const path &fn, const String &content, const path &lock_dir)
{
    auto hf = sha1(fn.string());
    const auto lock = lock_dir / hf;

    ScopedFileLock fl(lock);
    write_file(fn, content);

    File f(fn);
    f.getFileRecord().load();
}

void replaceInFileOnce(const path &fn, const String &from, const String &to, const path &lock_dir)
{
    auto hf = sha1(fn.string());

    auto uniq = fn.string() + from + to;
    auto h = sha1(uniq).substr(0, 5);
    auto hfn = lock_dir / (hf + "." + h);

    if (fs::exists(hfn))
        return;

    const auto lock = lock_dir / hf;
    ScopedFileLock fl(lock);

    // double check
    if (fs::exists(hfn))
        return;

    auto s = read_file(fn);
    boost::replace_all(s, from, to);
    write_file(fn, s); // if different?
    write_file(hfn, "");

    File f(fn);
    f.getFileRecord().load();
}

void pushFrontToFileOnce(const path &fn, const String &text, const path &lock_dir)
{
    auto hf = sha1(fn.string());

    auto uniq = fn.string() + text;
    auto h = sha1(uniq).substr(0, 5);
    auto hfn = lock_dir / (hf + "." + h);

    if (fs::exists(hfn))
        return;

    const auto lock = lock_dir / hf;
    ScopedFileLock fl(lock);

    // double check
    if (fs::exists(hfn))
        return;

    const auto orig = lock_dir / (hf + ".orig");
    if (!fs::exists(orig))
        fs::copy_file(fn, orig, fs::copy_options::overwrite_existing);
    else
        fs::copy_file(orig, fn, fs::copy_options::overwrite_existing);

    auto s = read_file(fn);
    s = text + "\n" + s;
    write_file(fn, s);
    write_file(hfn, "");

    File f(fn);
    f.getFileRecord().load();
}

void pushBackToFileOnce(const path &fn, const String &text, const path &lock_dir)
{
    auto hf = sha1(fn.string());

    auto uniq = fn.string() + text;
    auto h = sha1(uniq).substr(0, 5);
    auto hfn = lock_dir / (hf + "." + h);

    if (fs::exists(hfn))
        return;

    const auto lock = lock_dir / hf;
    ScopedFileLock fl(lock);

    // double check
    if (fs::exists(hfn))
        return;

    const auto orig = lock_dir / (hf + ".orig");
    if (!fs::exists(orig))
        fs::copy_file(fn, orig, fs::copy_options::overwrite_existing);
    else
        fs::copy_file(orig, fn, fs::copy_options::overwrite_existing);

    auto s = read_file(fn);
    s = s + "\n" + text;
    write_file(fn, s);
    write_file(hfn, "");

    File f(fn);
    f.getFileRecord().load();
}

}

void __cppan_dummy_x() {}
