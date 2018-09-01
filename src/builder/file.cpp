// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "file.h"

#include "concurrent_map.h"
#include "db.h"
#include "file_storage.h"

#include <directories.h>
#include <hash.h>
#include <settings.h>

#include <boost/algorithm/string.hpp>
#include <boost/dll.hpp>
#include <primitives/executor.h>
#include <primitives/hash.h>
#include <primitives/templates.h>
#include <primitives/debug.h>
#include <primitives/sw/settings.h>

#include <sstream>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "file");

#define CPPAN_FILES_EXPLAIN_FILE (getUserDirectories().storage_dir_tmp / "explain.txt")

static cl::opt<bool> explain_outdated("explain-outdated", cl::desc("Explain outdated files"));

namespace sw
{

static Executor explain_executor("explain executor", 1);

void explainMessage(const String &subject, bool outdated, const String &reason, const String &name)
{
    if (!explain_outdated)
        return;
    static std::ofstream o(CPPAN_FILES_EXPLAIN_FILE.string()); // goes first
    explain_executor.push([=]
    {
        if (!outdated)
            return;
        o << subject << ": " << name << "\n";
        o << "outdated\n";
        o << "reason = " << reason << "\n" << std::endl;
    });
}

String GetCurrentModuleNameHash();

path getFilesLogFileName(const String &config)
{
    auto cfg = sha256_short(GetCurrentModuleNameHash() + "_" + config);
    const path bp = getUserDirectories().storage_dir_tmp / ("files_" + cfg + ".log");
    auto p = bp.parent_path() / bp.filename().stem();
    std::ostringstream ss;
    //ss << "." << sha256_short(boost::dll::program_location().string());
    p += ss.str();
    p += bp.extension();
    return p;
}

File::File(FileStorage &s)
    : fs(&s)
{
}

File::File(const path &p, FileStorage &s)
    : fs(&s), file(p)
{
    if (file.empty())
        throw std::runtime_error("Empty file");
    if (!fs)
        throw std::runtime_error("Empty file storage");
    registerSelf();
    if (r->file.empty())
        r->file = file;
}

File &File::operator=(const path &rhs)
{
    file = rhs;
    registerSelf();
    return *this;
}

void File::registerSelf() const
{
    if (r)
        return;
    fs->registerFile(*this);
}

std::unordered_set<std::shared_ptr<sw::builder::Command>> File::gatherDependentGenerators() const
{
    registerSelf();
    std::unordered_set<std::shared_ptr<sw::builder::Command>> deps;
    for (auto &[f, d] : r->explicit_dependencies)
    {
        if (d->isGenerated())
            deps.insert(d->getGenerator());
    }
    for (auto &[f, d] : r->implicit_dependencies)
    {
        if (d->isGenerated())
            deps.insert(d->getGenerator());
    }
    return deps;
}

path File::getPath() const
{
    return file;
}

void File::addExplicitDependency(const path &p)
{
    registerSelf();
    File f(p, *fs);
    // FIXME:
    static std::mutex m;
    std::unique_lock<std::mutex> lk(m);
    r->explicit_dependencies.emplace(p, f.r);
}

void File::addExplicitDependency(const Files &files)
{
    for (auto &p : files)
        addExplicitDependency(p);
}

void File::addImplicitDependency(const path &p)
{
    registerSelf();
    File f(p, *fs);
    // FIXME:
    static std::mutex m;
    std::unique_lock<std::mutex> lk(m);
    r->implicit_dependencies.emplace(p, f.r);
}

void File::addImplicitDependency(const Files &files)
{
    for (auto &p : files)
        addImplicitDependency(p);
}

void File::clearDependencies()
{
    registerSelf();
    r->explicit_dependencies.clear();
    r->implicit_dependencies.clear();
}

void File::clearImplicitDependencies()
{
    registerSelf();
    r->implicit_dependencies.clear();
}

FileRecord &File::getFileRecord()
{
    registerSelf();
    return *r;
}

const FileRecord &File::getFileRecord() const
{
    return ((File*)this)->getFileRecord();
}

bool File::isChanged() const
{
    registerSelf();
    return r->isChanged();
}

bool File::isGenerated() const
{
    registerSelf();
    return r->isGenerated();
}

bool File::isGeneratedAtAll() const
{
    registerSelf();
    return r->isGeneratedAtAll();
}

/*bool File::operator==(const File &rhs) const
{
    if (r == nullptr && r == rhs.r)
        return file == rhs.file;
    if (file != rhs.file)
        return false;
}*/

FileData::FileData(const FileData &rhs)
{
    *this = rhs;
}

FileData &FileData::operator=(const FileData &rhs)
{
    last_write_time = rhs.last_write_time;
    size = rhs.size;
    hash = rhs.hash;
    flags = rhs.flags;

    refreshed = rhs.refreshed.load();

    return *this;
}

FileRecord::FileRecord(const FileRecord &rhs)
{
    *this = rhs;
}

FileRecord &FileRecord::operator=(const FileRecord &rhs)
{
    if (rhs.fs)
        fs = rhs.fs;

    file = rhs.file;
    data = rhs.data;

    generator = rhs.generator;

    explicit_dependencies = rhs.explicit_dependencies;
    implicit_dependencies = rhs.implicit_dependencies;

    return *this;
}

size_t FileRecord::getHash() const
{
    auto k = std::hash<path>()(file);
    hash_combine(k, data->last_write_time.time_since_epoch().count());
    //hash_combine(k, size);
    for (auto &[f, d] : explicit_dependencies)
        hash_combine(k, d->data->last_write_time.time_since_epoch().count());
    for (auto &[f, d] : implicit_dependencies)
        hash_combine(k, d->data->last_write_time.time_since_epoch().count());
    return k;
}

void FileRecord::reset()
{
    //if (generator && generator->executed())
    {
        // do we need to reset changed in this case or not?
        //if (!generator.expired())
            //generator.reset();
    }

    data->refreshed = false;
}

void FileRecord::load(const path &p)
{
    if (!p.empty())
        file = p;
    if (file.empty())
        return;
    if (!fs::exists(file))
        return;
    auto lwt = fs::last_write_time(file);
    if (lwt < data->last_write_time)
        return;
    //size = fs::file_size(file);
    // do not calc hashes on the first run
    // we do this on the first mismatch
    //hash = sha256(file);

    // also update deps
    for (auto &[f, d] : explicit_dependencies)
    {
        if (d == this || !d)
            continue;
        if (d->isChanged())
            d->load();
    }
    for (auto &[f, d] : implicit_dependencies)
    {
        if (d == this || !d)
            continue;
        if (d->isChanged())
            d->load();
    }

    fs->async_file_log(this);
}

bool FileRecord::refresh(bool use_file_monitor)
{
    bool r = false;
    if (!data->refreshed.compare_exchange_strong(r, true))
        return false;

    //DEBUG_BREAK_IF_PATH_HAS(file, "tclOOStubLib.c.45b4313d.obj");

    // in any case refresh *all* deps
    // do it first, because we might exit early
    for (auto &[f, d] : explicit_dependencies)
    {
        if (d == this)
            continue;
        d->refresh(use_file_monitor);
    }
    for (auto &[f, d] : implicit_dependencies)
    {
        if (d == this)
            continue;
        d->refresh(use_file_monitor);
    }

    if (!fs::exists(file))
    {
        EXPLAIN_OUTDATED("file", true, "not found", file.u8string());
        return true;
    }

    bool result = false;

    auto t = fs::last_write_time(file);
    if (t > data->last_write_time)
    {
        if (data->last_write_time.time_since_epoch().count() == 0)
            EXPLAIN_OUTDATED("file", true, "last_write_time changed", file.u8string());
        else
            EXPLAIN_OUTDATED("file", true, "empty last_write_time", file.u8string());
        data->last_write_time = t;
        result = true;
    }

    //if (use_file_monitor)
        //fs->async_file_log(this);

    return result;
}

bool FileRecord::isChanged(bool use_file_monitor)
{
    auto is_changed = [this]()
    {
        auto t = getMaxTime();
        if (t > data->last_write_time)
        {
            data->last_write_time = t;
            return true;
        }
        return false;
    };

    auto c = refresh(use_file_monitor);
    if (c)
    {
        EXPLAIN_OUTDATED("file", true, "changed after refresh", file.u8string());
    }
    c |= is_changed();
    if (c)
    {
        EXPLAIN_OUTDATED("file", true, "changed after checking max time", file.u8string());
    }

    if (use_file_monitor && c)
        fs->async_file_log(this);

    /*bool r = false;
    if (use_file_monitor && saved.compare_exchange_strong(r, true))
        fs->async_file_log(this);*/

    return c;
}

void FileRecord::setGenerator(const std::shared_ptr<builder::Command> &g)
{
    //DEBUG_BREAK_IF_PATH_HAS(file, "/primitives.filesystem-master.dll");

    if (!g)
        return;

    auto gold = generator.lock();
    if (gold && (gold != g &&
        !gold->isExecuted() &&
        !gold->maybe_unused &&
        gold->getHash() != g->getHash()))
        throw std::runtime_error("Setting generator twice on file: " + file.u8string());
    generator = g;
    generated_ = true;
}

std::shared_ptr<builder::Command> FileRecord::getGenerator() const
{
    return generator.lock();
}

bool FileRecord::isGenerated() const
{
    return !!generator.lock();
}

fs::file_time_type FileRecord::getMaxTime() const
{
    auto m = data->last_write_time;
    for (auto &[f, d] : explicit_dependencies)
    {
        if (d == this)
            continue;
        auto dm = d->getMaxTime();
        if (dm > m)
        {
            m = dm;
            EXPLAIN_OUTDATED("file", true, "explicit " + f.u8string() + " is newer", file.u8string());
        }
    }
    for (auto &[f, d] : implicit_dependencies)
    {
        if (d == this)
            continue;
        auto dm = d->getMaxTime();
        if (dm > m)
        {
            m = dm;
            EXPLAIN_OUTDATED("file", true, "implicit " + f.u8string() + " is newer", file.u8string());
        }
    }
    return m;
}

bool FileRecord::operator<(const FileRecord &r) const
{
    return data->last_write_time < r.data->last_write_time;
}

}
