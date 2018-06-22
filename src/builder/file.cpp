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
#include <primitives/file_monitor.h>
#include <primitives/hash.h>
#include <primitives/templates.h>

#include <sstream>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "file");

#define CPPAN_FILES_EXPLAIN_FILE (getUserDirectories().storage_dir_tmp / "explain.txt")

namespace sw
{

bool useFileMonitor = true;

primitives::filesystem::FileMonitor &get_file_monitor()
{
    static primitives::filesystem::FileMonitor fm;
    return fm;
}

void explainMessage(const String &subject, bool outdated, const String &reason, const String &name)
{
    if (!Settings::get_local_settings().explain_outdated)
        return;
    static Executor e(1);
    static std::ofstream o(CPPAN_FILES_EXPLAIN_FILE.string());
    e.push([=]
    {
        o << subject << ": " << name << "\n";
        o << "outdated = " << (outdated ? "1" : "0") << "\n";
        o << "reason = " << reason << "\n\n";
    });
}

path getFilesLogFileName()
{
    const path bp = getUserDirectories().storage_dir_tmp / "files.log";
    auto p = bp.parent_path() / bp.filename().stem();
    std::ostringstream ss;
    //ss << "." << sha256_short(boost::dll::program_location().string());
    p += ss.str();
    p += bp.extension();
    return p;
}

FileStorage &getFileStorage()
{
    static FileStorage fs;
    return fs;
}

FileStorage::FileStorage()
{
    load();
}

FileStorage::~FileStorage()
{
    try
    {
        save();
    }
    catch (...)
    {
        LOG_ERROR(logger, "Error during file db save");
    }
}

void FileStorage::load()
{
    getDb().load(files);
}

void FileStorage::save()
{
    getDb().save(files);
}

void FileStorage::reset()
{
    /*save();
    // we have some vars (files) not dumped or something like this
    // do not remove!
    files.clear();
    load();*/
    for (auto i = files.getIterator(); i.isValid(); i.next())
    {
        auto &f = *i.getValue();
        f.reset();
    }
}

void FileStorage::registerFile(const File &in_f)
{
    // fs path hash on windows differs for lower and upper cases
#ifdef CPPAN_OS_WINDOWS
    // very slow
    //((File*)&in_f)->file = boost::to_lower_copy(normalize_path(in_f.file));
    ((File*)&in_f)->file = normalize_path(in_f.file);
#endif

    auto r = files.insert(in_f.file);
    if (r.second)
    {
        //r.first->load(in_f.file);
        if (!in_f.file.empty())
            r.first->file = in_f.file;
    }
    //else
        //r.first->isChanged();
    in_f.r = r.first;
}

File::File(const path &p)
    : file(p)
{
    if (file.empty())
        throw std::runtime_error("Empty file");
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
    getFileStorage().registerFile(*this);
}

std::unordered_set<std::shared_ptr<sw::builder::Command>> File::gatherDependentGenerators() const
{
    registerSelf();
    std::unordered_set<std::shared_ptr<sw::builder::Command>> deps;
    for (auto &[f, d] : r->explicit_dependencies)
    {
        if (d->generator)
            deps.insert(d->generator);
    }
    for (auto &[f, d] : r->implicit_dependencies)
    {
        if (d->generator)
            deps.insert(d->generator);
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
    File f(p);
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
    File f(p);
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

/*bool File::operator==(const File &rhs) const
{
    if (r == nullptr && r == rhs.r)
        return file == rhs.file;
    if (file != rhs.file)
        return false;
}*/

FileRecord::FileRecord(const FileRecord &rhs)
{
    *this = rhs;
}

FileRecord &FileRecord::operator=(const FileRecord &rhs)
{
    file = rhs.file;
    last_write_time = rhs.last_write_time;
    size = rhs.size;
    hash = rhs.hash;
    flags = rhs.flags;
    generator = rhs.generator;

    explicit_dependencies = rhs.explicit_dependencies;
    implicit_dependencies = rhs.implicit_dependencies;

    refreshed = rhs.refreshed.load();

    return *this;
}

size_t FileRecord::getHash() const
{
    auto k = std::hash<path>()(file);
    hash_combine(k, last_write_time.time_since_epoch().count());
    for (auto &[f, d] : explicit_dependencies)
        hash_combine(k, d->last_write_time.time_since_epoch().count());
    for (auto &[f, d] : implicit_dependencies)
        hash_combine(k, d->last_write_time.time_since_epoch().count());
    //hash_combine(k, size);
    return k;
}

void FileRecord::reset()
{
    if (generator && generator->executed)
    {
        // do we need to reset changed in this case or not?
        generator.reset();
    }
}

void FileRecord::load(const path &p)
{
    if (!p.empty())
        file = p;
    if (file.empty())
        return;
    if (!fs::exists(file))
        return;
    last_write_time = fs::last_write_time(file);
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

    struct file_holder
    {
        FILE *f = nullptr;
        path fn;
        file_holder(const path &fn) : fn(fn)
        {
            f = primitives::filesystem::fopen(fn, "ab");

            if (!f)
                throw std::runtime_error("Cannot open file log for writing");

            // goes first
            if (setvbuf(f, NULL, _IONBF, 0) != 0)
                throw std::runtime_error("Cannot disable log buffering");

            // Opening a file in append mode doesn't set the file pointer to the file's
            // end on Windows. Do that explicitly.
            fseek(f, 0, SEEK_END);
        }
        ~file_holder()
        {
            fclose(f);
            error_code ec;
            fs::remove(fn, ec);
        }
    };

    // async write to log
    {
        static Executor e(1);
        static auto fn = getFilesLogFileName();
        static file_holder f(fn);
        static std::vector<uint8_t> v;
        e.push([this]
        {
            getDb().write(v, *this);

            fseek(f.f, 0, SEEK_END);
            fwrite(&v[0], v.size(), 1, f.f);
            fflush(f.f);
        });
    }
}

bool FileRecord::refresh()
{
    bool r = false;
    if (!refreshed.compare_exchange_strong(r, true))
        return false;

    // in any case refresh *all* deps
    // do it first, because we might exit early
    for (auto &[f, d] : explicit_dependencies)
    {
        if (d == this)
            continue;
        d->refresh();
    }
    for (auto &[f, d] : implicit_dependencies)
    {
        if (d == this)
            continue;
        d->refresh();
    }

    if (!fs::exists(file))
    {
        EXPLAIN_OUTDATED("file", true, "not found", file.string());
        return true;
    }

    if (useFileMonitor)
    {
        get_file_monitor().addFile(file, [](const path &f)
        {
            auto &r = File(f).getFileRecord();
            error_code ec;
            if (fs::exists(r.file, ec))
                r.last_write_time = fs::last_write_time(f);
            else
                r.refreshed = false;
        });
    }

    auto t = fs::last_write_time(file);
    if (t > last_write_time)
    {
        last_write_time = t;
        return true;
    }

    return false;
}

bool FileRecord::isChanged()
{
    auto is_changed = [this]()
    {
        auto t = getMaxTime();
        if (t > last_write_time)
        {
            last_write_time = t;
            return true;
        }
        return false;
    };

    auto c = refresh();
    c |= is_changed();
    return c;
}

fs::file_time_type FileRecord::getMaxTime() const
{
    auto m = last_write_time;
    for (auto &[f, d] : explicit_dependencies)
    {
        if (d == this)
            continue;
        m = std::max(m, d->getMaxTime());
    }
    for (auto &[f, d] : implicit_dependencies)
    {
        if (d == this)
            continue;
        m = std::max(m, d->getMaxTime());
    }
    return m;
}

bool FileRecord::operator<(const FileRecord &r) const
{
    return last_write_time < r.last_write_time;
}

}
