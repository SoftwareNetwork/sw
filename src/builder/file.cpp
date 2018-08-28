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
#include <primitives/file_monitor.h>
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

bool useFileMonitor = true;

primitives::filesystem::FileMonitor &get_file_monitor()
{
    static primitives::filesystem::FileMonitor fm;
    return fm;
}

void explainMessage(const String &subject, bool outdated, const String &reason, const String &name)
{
    if (!explain_outdated)
        return;
    static Executor e(1);
    static std::ofstream o(CPPAN_FILES_EXPLAIN_FILE.string());
    e.push([=]
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

void async_file_log(const FileRecord *r)
{
    struct file_holder
    {
        ScopedFile f;
        path fn;
        file_holder(const path &fn) : f(fn, "ab")
        {
            // goes first
            // but maybe remove?
            if (setvbuf(f.getHandle(), NULL, _IONBF, 0) != 0)
                throw std::runtime_error("Cannot disable log buffering");

            // Opening a file in append mode doesn't set the file pointer to the file's
            // end on Windows. Do that explicitly.
            fseek(f.getHandle(), 0, SEEK_END);
        }
        ~file_holder()
        {
            error_code ec;
            fs::remove(fn, ec);
        }
    };

    // async write to log
    {
        static Executor e(1);
        static auto fn = getFilesLogFileName(r->fs->config);
        static file_holder f(fn);
        static std::vector<uint8_t> v;
        e.push([r]
        {
            getDb().write(v, *r);

            //fseek(f.f, 0, SEEK_END);
            fwrite(&v[0], v.size(), 1, f.f.getHandle());
            fflush(f.f.getHandle());
        });
    }
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
    if (r->data->file.empty())
        r->data->file = file;
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
    getFileDataStorage().registerFile(*this);
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

FileRecord::FileRecord(const FileRecord &rhs)
{
    *this = rhs;
}

FileRecord &FileRecord::operator=(const FileRecord &rhs)
{
    data = rhs.data;
    generator = rhs.generator;

    explicit_dependencies = rhs.explicit_dependencies;
    implicit_dependencies = rhs.implicit_dependencies;

    refreshed = rhs.refreshed.load();

    return *this;
}

size_t FileData::getHash() const
{
    auto k = std::hash<path>()(file);
    hash_combine(k, last_write_time.time_since_epoch().count());
    //hash_combine(k, size);
    return k;
}

size_t FileRecord::getHash() const
{
    auto k = data->getHash();
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
}

bool FileData::load(const path &p)
{
    if (!p.empty())
        file = p;
    if (file.empty())
        return false;
    if (!fs::exists(file))
        return false;
    last_write_time = fs::last_write_time(file);
    //size = fs::file_size(file);
    // do not calc hashes on the first run
    // we do this on the first mismatch
    //hash = sha256(file);
    return true;
}

void FileRecord::load(const path &p)
{
    if (!data->load(p))
        return;

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

    async_file_log(this);
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

    if (!fs::exists(data->file))
    {
        EXPLAIN_OUTDATED("file", true, "not found", data->file.u8string());
        return true;
    }

    if (useFileMonitor)
    {
        if (!fs)
            throw std::runtime_error("Empty file storage");
        get_file_monitor().addFile(data->file, [fs = fs](const path &f)
        {
            auto &r = File(f, *fs).getFileRecord();
            error_code ec;
            if (fs::exists(r.data->file, ec))
                r.data->last_write_time = fs::last_write_time(f);
            else
                r.refreshed = false;
        });
    }

    bool result = false;

    auto t = fs::last_write_time(data->file);
    if (t > data->last_write_time)
    {
        if (data->last_write_time.time_since_epoch().count() == 0)
            EXPLAIN_OUTDATED("file", true, "last_write_time changed", data->file.u8string());
        else
            EXPLAIN_OUTDATED("file", true, "empty last_write_time", data->file.u8string());
        data->last_write_time = t;
        result = true;
    }

    //async_file_log(this);

    return result;
}

bool FileRecord::isChanged()
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

    auto c = refresh();
    if (c)
    {
        EXPLAIN_OUTDATED("file", true, "changed after refresh", data->file.u8string());
    }
    c |= is_changed();
    if (c)
    {
        EXPLAIN_OUTDATED("file", true, "changed after checking max time", data->file.u8string());
    }
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
        throw std::runtime_error("Setting generator twice on file: " + data->file.u8string());
    //generator.reset();
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
            EXPLAIN_OUTDATED("file", true, "explicit " + f.u8string() + " lwt is greater", data->file.u8string());
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
            EXPLAIN_OUTDATED("file", true, "implicit " + f.u8string() + " lwt is greater", data->file.u8string());
        }
    }
    return m;
}

bool FileData::operator<(const FileData &r) const
{
    return last_write_time < r.last_write_time;
}

bool FileRecord::operator<(const FileRecord &r) const
{
    return *data < *r.data;
}

}
