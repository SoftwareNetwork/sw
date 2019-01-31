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

#define CPPAN_FILES_EXPLAIN_FILE ".sw/misc/explain.txt"

namespace sw
{

static Executor explain_executor("explain executor", 1);

void explainMessage(const String &subject, bool outdated, const String &reason, const String &name)
{
    //if (!explain_outdated)
        //return;
    static std::ofstream o([]()
    {
        fs::create_directories(path(CPPAN_FILES_EXPLAIN_FILE).parent_path());
        return CPPAN_FILES_EXPLAIN_FILE;
    }()); // goes first
    explain_executor.push([=]
    {
        if (!outdated)
            return;
        o << subject << ": " << name << "\n";
        o << "outdated\n";
        o << "reason = " << reason << "\n" << std::endl;
    });
}

File::File(const path &p, FileStorage &s)
    : fs(&s), file(p)
{
    if (file.empty())
        throw SW_RUNTIME_ERROR("Empty file");
    if (!fs)
        throw SW_RUNTIME_ERROR("Empty file storage");
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

void File::addImplicitDependency(const path &p)
{
    if (p.empty())
        return;
    registerSelf();
    File f(p, *fs);
    r->implicit_dependencies.emplace(p, f.r);
}

void File::addImplicitDependency(const Files &files)
{
    for (auto &p : files)
        addImplicitDependency(p);
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

std::optional<String> File::isChanged(const fs::file_time_type &t, bool throw_on_missing)
{
    return getFileRecord().isChanged(t, throw_on_missing);
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

FileData::FileData(const FileData &rhs)
{
    *this = rhs;
}

FileData &FileData::operator=(const FileData &rhs)
{
    last_write_time = rhs.last_write_time;
    //size = rhs.size;
    //hash = rhs.hash;
    //flags = rhs.flags;

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

    implicit_dependencies = rhs.implicit_dependencies;

    return *this;
}

void FileRecord::reset()
{
    //if (generator && generator->executed())
    {
        // do we need to reset changed in this case or not?
        //if (!generator.expired())
            data->generator.reset();
    }

    if (data)
        data->refreshed = FileData::RefreshType::Unrefreshed;
}

static auto get_lwt(const path &file)
{
    auto m = fs::last_write_time(file);
    return m;

    // C++20 does not have this issue
#if defined(_MSC_VER) && _MSVC_LANG <= 201703
    static const decltype(m) now = []
    {
        auto p = get_temp_filename();
        write_file(p, "");
        auto m = fs::last_write_time(p);
        fs::remove(p);
        return m;
    }();
#else
    static const auto now = decltype(m)::clock::now();
#endif
    if (m > now)
    {
        // file is changed during program execution
        // TODO: handle this case - use non static now()
        //auto d = std::chrono::duration_cast<std::chrono::milliseconds>(m - now).count();
        //LOG_WARN(logger, "File " + normalize_path(file) + " is from future, diff = +" + std::to_string(d) + " ms.");
    }

    return m;
}

/*void FileRecord::load()
{
    // this is load without refresh

    if (file.empty() || !data)
        return;
    if (!fs::exists(file))
        return;
    auto lwt = get_lwt(file);
    if (lwt < data->last_write_time)
        return;
    data->last_write_time = lwt;
    //size = fs::file_size(file);
    // do not calc hashes on the first run
    // we do this on the first mismatch
    //hash = sha256(file);

    // also update deps
    for (auto &[f, d] : implicit_dependencies)
    {
        if (d == this || !d)
            continue;
        if (d->isChanged())
            d->load();
    }

    fs->async_file_log(this);
}*/

void FileRecord::refresh()
{
    FileData::RefreshType r = FileData::RefreshType::Unrefreshed;
    if (!data || !data->refreshed.compare_exchange_strong(r, FileData::RefreshType::InProcess))
        return;

    // in any case refresh *all* deps
    // do it first, because we might exit early
    //if (!isGeneratedAtAll()) uncomment?
    /*for (auto &[f, d] : implicit_dependencies)
    {
        if (d == this)
            continue;
        d->refresh();
    }*/

    bool changed = false;
    if (!fs::exists(file))
    {
        //EXPLAIN_OUTDATED("file", true, "not found", file.u8string());
        data->last_write_time = decltype(data->last_write_time)();
        changed = true;
    }
    else
    {
        auto t = get_lwt(file);
        if (t > data->last_write_time)
        {
            /*if (data->last_write_time.time_since_epoch().count() != 0)
                EXPLAIN_OUTDATED("file", true, "last_write_time changed on disk from " +
                    std::to_string(data->last_write_time.time_since_epoch().count()) + " to " +
                    std::to_string(t.time_since_epoch().count()), file.u8string());
            else
                EXPLAIN_OUTDATED("file", true, "empty last_write_time", file.u8string());*/
            data->last_write_time = t;
            changed = true;
        }
        //else
            //data->last_write_time = t;
    }

    // register!
    writeToLog();

    data->refreshed = changed ? FileData::RefreshType::Changed : FileData::RefreshType::NotChanged;
}

bool FileRecord::isChanged()
{
    if (data->refreshed == FileData::RefreshType::Unrefreshed)
        refresh();
    else if (data->refreshed == FileData::RefreshType::InProcess)
    {
        // spinning
        while (data->refreshed == FileData::RefreshType::InProcess)
            ;
    }
    return data->refreshed == FileData::RefreshType::Changed;
}

bool FileRecord::isChangedWithDeps()
{
    // refresh all first
    for (auto &[f, d] : implicit_dependencies)
        d->isChanged();

    // explain inside
    if (isChanged())
        return true;

    FileRecord *cd = nullptr;
    bool c = std::any_of(implicit_dependencies.begin(), implicit_dependencies.end(),
        [&cd](const auto &p)
    {
        auto r = p.second->isChanged();
        if (r)
            cd = p.second;
        return r;
    });
    if (c)
    {
        /*EXPLAIN_OUTDATED("file", true, "changed because one of deps is changed: " +
            cd->file.u8string(), file.u8string());*/
        return true;
    }

    auto t = getMaxTime();
    if (t > data->last_write_time)
    {
        /*EXPLAIN_OUTDATED("file", true, "changed after checking deps max time. this lwt = " +
            std::to_string(data->last_write_time.time_since_epoch().count()) + ", dep lwt = " +
            std::to_string(t.time_since_epoch().count()), file.u8string());*/
        //data->last_write_time = t;
        return true;
    }

    //auto c = data->isChanged();
    //if (c)
        //fs->async_file_log(this);

    /*bool r = false;
    if (saved.compare_exchange_strong(r, true))
        fs->async_file_log(this);*/

    return false;
}

std::optional<String> FileRecord::isChanged(const fs::file_time_type &in, bool throw_on_missing)
{
    // we call this as refresh of all deps
    // explain inside
    isChangedWithDeps();

    // on missing direct file we fail immediately
    if (data->last_write_time.time_since_epoch().count() == 0)
    {
        if (throw_on_missing)
            throw SW_RUNTIME_ERROR("file " + normalize_path(file) + " is missing");
        return "file is missing";
    }

    // on missing implicit depedency we run command anyways
    for (auto &[f, d] : implicit_dependencies)
    {
        if (d->data->last_write_time.time_since_epoch().count() == 0)
            return "dependency " + normalize_path(d->file) + " is missing";
    }

    if (data->last_write_time > in)
    {
        return "file is newer";
    }

    for (auto &[f, d] : implicit_dependencies)
    {
        if (d->data->last_write_time > in)
        {
            return "dependency " + normalize_path(d->file) + " is newer";
            //EXPLAIN_OUTDATED("file", true, "implicit " + f.u8string() + " is newer", file.u8string());
        }
    }
    return {};
}

void FileRecord::setGenerator(const std::shared_ptr<builder::Command> &g, bool ignore_errors)
{
    if (!g)
        return;

    auto gold = data->generator.lock();
    if (!ignore_errors && gold && (gold != g &&
        !gold->isExecuted() &&
        !gold->maybe_unused &&
        gold->getHash() != g->getHash()))
    {
        String err;
        err += "Setting generator twice on file: " + file.u8string() + "\n";
        if (gold)
        {
            err += "first generator:\n " + gold->print() + "\n";
            err += "first generator hash:\n " + std::to_string(gold->getHash());
        }
        else
            err += "first generator is empty";
        err += "\n";
        if (g)
        {
            err += "second generator:\n " + g->print() + "\n";
            err += "second generator hash:\n " + std::to_string(g->getHash());
        }
        else
            err += "second generator is empty";
        throw SW_RUNTIME_ERROR(err);
    }
    data->generator = g;
    data->generated = true;
}

std::shared_ptr<builder::Command> FileRecord::getGenerator() const
{
    return data->generator.lock();
}

bool FileRecord::isGenerated() const
{
    return !!data->generator.lock();
}

fs::file_time_type FileRecord::getMaxTime() const
{
    auto m = data->last_write_time;
    // we check deps ONLY in case if current file is generated
    // otherwise, time will be adjusted during AST execution
    //if (isGeneratedAtAll())
    for (auto &[f, d] : implicit_dependencies)
    {
        if (d == this)
            continue;
        auto dm = d->data->last_write_time;
        if (dm > m)
        {
            m = dm;
            //EXPLAIN_OUTDATED("file", true, "implicit " + f.u8string() + " is newer", file.u8string());
        }
    }
    return m;
}

void FileRecord::writeToLog() const
{
    fs->async_file_log(this);
}

/*fs::file_time_type FileRecord::updateLwt()
{
    if (data->last_write_time.time_since_epoch().count() == 0)
        const_cast<FileRecord*>(this)->load(file);
    return data->last_write_time;
}*/

bool FileRecord::operator<(const FileRecord &r) const
{
    return data->last_write_time < r.data->last_write_time;
}

}
