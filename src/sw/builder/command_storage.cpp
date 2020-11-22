/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "command_storage.h"

#include "file_storage.h"
#include "sw_context.h"

#include <boost/thread/lock_types.hpp>
#include <primitives/emitter.h>
#include <primitives/executor.h>
#include <primitives/date_time.h>
#include <primitives/debug.h>
#include <primitives/exceptions.h>
#include <primitives/lock.h>
#include <primitives/symbol.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "db_file");

#define COMMAND_DB_FORMAT_VERSION 8

namespace sw
{

static path getCurrentModuleName()
{
    return primitives::getModuleNameForSymbol(primitives::getCurrentModuleSymbol());
}

static String getCurrentModuleNameHash()
{
    return shorten_hash(blake2b_512(to_string(getCurrentModuleName().u8string())), 12);
}

static path getDir(const path &root)
{
    return root / "db";
}

static path getCommandsDbFilename(const path &root)
{
    return getDir(root) / std::to_string(COMMAND_DB_FORMAT_VERSION) / "commands.bin";
}

static path getCommandsLogFileName(const path &root)
{
    auto cfg = shorten_hash(blake2b_512(getCurrentModuleNameHash()), 12);
    return getDir(root) / std::to_string(COMMAND_DB_FORMAT_VERSION) / ("cmd_log_" + cfg + ".bin");
}

template <class T>
static void write_int(std::vector<uint8_t> &vec, T val)
{
    auto sz = vec.size();
    vec.resize(sz + sizeof(val));
    memcpy(&vec[sz], &val, sizeof(val));
}

static void write_str(std::vector<uint8_t> &vec, const String &val)
{
    auto sz = val.size() + 1;
    //write_int(vec, sz);
    auto vsz = vec.size();
    vec.resize(vsz + sz);
    memcpy(&vec[vsz], &val[0], sz);
}

Files CommandRecord::getImplicitInputs(detail::Storage &s) const
{
    Files files;
    for (auto &h : implicit_inputs)
    {
        boost::upgrade_lock lk(s.m_file_storage_by_hash);
        auto i = s.file_storage_by_hash.find(h);
        if (i == s.file_storage_by_hash.end())
            throw SW_RUNTIME_ERROR("no such file");
        auto p = i->second;
        lk.unlock();
        if (!p.empty())
            files.insert(p);
    }
    return files;
}

void CommandRecord::setImplicitInputs(const Files &files, detail::Storage &s)
{
    implicit_inputs.clear(); // clear first!

    for (auto &f : files)
    {
        auto str = normalize_path(f);
        auto h = std::hash<path>()(str);
        implicit_inputs.insert(h);

        boost::upgrade_lock lk(s.m_file_storage_by_hash);
        auto i = s.file_storage_by_hash.find(h);
        if (i == s.file_storage_by_hash.end())
        {
            boost::upgrade_to_unique_lock lk2(lk);
            s.file_storage_by_hash[h] = str;
        }
    }
}

FileDb::FileDb(const SwBuilderContext &swctx)
    : swctx(swctx)
{
}

static auto file_hash(const path &p) { return std::hash<path>()(p); }

void FileDb::write(std::vector<uint8_t> &v, const CommandRecord &f, const detail::Storage &s)
{
    v.clear();

    if (f.hash == 0)
        return;

    //if (!std::is_trivially_copyable_v<decltype(f.mtime)>)
        //throw SW_RUNTIME_ERROR("x");

    write_int(v, f.hash);
    write_int(v, f.mtime);

    auto n = f.implicit_inputs.size();
    write_int(v, n);
    for (auto &h : f.implicit_inputs)
    {
        boost::upgrade_lock lk(s.m_file_storage_by_hash);
        auto i = s.file_storage_by_hash.find(h);
        if (i == s.file_storage_by_hash.end())
            throw SW_RUNTIME_ERROR("no such file");
        auto p = i->second;
        lk.unlock();
        write_int(v, file_hash(normalize_path(p)));
    }
}

static String getFilesSuffix()
{
    return ".files";
}

static void load(const path &fn, Files &files, std::unordered_map<size_t, path> &files2, ConcurrentCommandStorage &commands)
{
    // files
    auto fn_with_suffix = path(fn) += getFilesSuffix();
    if (fs::exists(fn_with_suffix))
    {
        primitives::BinaryStream b;
        b.load(fn_with_suffix);
        while (!b.eof())
        {
            size_t sz; // record size
            b.read(sz);
            if (!b.has(sz))
            {
                fs::resize_file(fn_with_suffix, b.index() - sizeof(sz));
                break; // record is in bad shape
            }

            if (sz == 0)
                continue;

            // file
            String s;
            b.read(s);
            auto p = fs::u8path(s);
            files.insert(p);

            files2[file_hash(p)] = p;
        }
    }

    // commands
    if (fs::exists(fn))
    {
        primitives::BinaryStream b;
        b.load(fn);
        while (!b.eof())
        {
            size_t sz; // record size
            b.read(sz);
            if (!b.has(sz))
            {
                // truncate
                fs::resize_file(fn, b.index() - sizeof(sz));
                break; // record is in bad shape
            }

            if (sz == 0)
                continue;

            size_t h;
            b.read(h);

            auto r = commands.insert(h);
            r.first->hash = h;

            //if (!std::is_trivially_copyable_v<decltype(r.first->mtime)>)
                //throw SW_RUNTIME_ERROR("x");

            b.read(r.first->mtime);

            size_t n;
            b.read(n);
            r.first->implicit_inputs.reserve(n);
            while (n--)
            {
                b.read(h);
                auto &f = files2[h];
                if (!f.empty())
                {
                    //r.first->implicit_inputs.insert(files2[h]);
                    r.first->implicit_inputs.insert(h);
                }
            }
        }
    }
}

void FileDb::load(Files &files, std::unordered_map<size_t, path> &files2, ConcurrentCommandStorage &commands, const path &root) const
{
    sw::load(getCommandsDbFilename(root), files, files2, commands);
    sw::load(getCommandsLogFileName(root), files, files2, commands);
}

void FileDb::save(const Files &files, const detail::Storage &s, ConcurrentCommandStorage &commands, const path &root) const
{
    std::vector<uint8_t> v;

    // files
    {
        primitives::BinaryStream b(10'000'000); // reserve amount
        for (auto &f : files)
        {
            auto s = to_string(normalize_path(f));
            auto sz = s.size() + 1;
            b.write(sz);
            b.write(s);
        }
        if (!b.empty())
        {
            auto p = getCommandsDbFilename(root) += getFilesSuffix();
            fs::create_directories(p.parent_path());
            b.save(p);
        }
    }

    // commands
    {
        primitives::BinaryStream b(10'000'000); // reserve amount
        for (const auto &[k, r] : commands)
        {
            write(v, r, s);
            auto sz = v.size();
            b.write(sz);
            b.write(v.data(), v.size());
        }
        if (!b.empty())
        {
            auto p = getCommandsDbFilename(root);
            fs::create_directories(p.parent_path());
            b.save(p);
        }
    }

    error_code ec;
    fs::remove(getCommandsLogFileName(root), ec);
    fs::remove(getCommandsLogFileName(root) += getFilesSuffix(), ec);
}

detail::FileHolder::FileHolder(const path &fn)
    : /*lk(fn)
    , */f(fn, "ab")
    , fn(fn)
{
    // goes first
    // but maybe remove?
    //if (setvbuf(f.getHandle(), NULL, _IONBF, 0) != 0)
    //throw RUNTIME_EXCEPTION("Cannot disable log buffering");

    // Opening a file in append mode doesn't set the file pointer to the file's
    // end on Windows. Do that explicitly.
    fseek(f.getHandle(), 0, SEEK_END);
}

detail::FileHolder::~FileHolder()
{
    f.close();

    error_code ec; // remove ec? but multiple processes may be writing into this log? or not?
    fs::remove(fn, ec);
}

CommandStorage::CommandStorage(const SwBuilderContext &swctx, const path &root)
    : swctx(swctx)
    , root(root)
    , fdb(swctx)
{
    //lock = getLock();
    load(); // load early
}

CommandStorage::~CommandStorage()
{
    save();
}

void CommandStorage::async_command_log(const CommandRecord &r)
{
    static std::vector<uint8_t> v;

    changed = true;
    add_user();
    swctx.getFileStorageExecutor().push([this, &r]
    {
        auto &s = getInternalStorage();

        {
            // write record to vector v
            fdb.write(v, r, s);

            auto &l = s.getCommandLog(swctx, root);
            auto sz = v.size();
            fwrite(&sz, sizeof(sz), 1, l.f.getHandle());
            fwrite(&v[0], sz, 1, l.f.getHandle());
            fflush(l.f.getHandle());
        }

        {
            auto &l = s.getFileLog(swctx, root);
            for (auto &f : r.getImplicitInputs(s))
            {
                auto r = s.file_storage.insert(f);
                if (!r.second)
                    continue;
                auto s = to_string(normalize_path(f));
                auto sz = s.size() + 1;
                fwrite(&sz, sizeof(sz), 1, l.f.getHandle());
                fwrite(&s[0], sz, 1, l.f.getHandle());
                fflush(l.f.getHandle());
            }
        }

        free_user();
    });
}

void CommandStorage::add_user()
{
    //++n_users;
}

void CommandStorage::free_user()
{
    //--n_users;
    //if (n_users == 0)
        closeLogs(); // reduce number of open fds
}

void detail::Storage::closeLogs()
{
    commands.reset();
    files.reset();
}

void CommandStorage::closeLogs()
{
    s.closeLogs();
    //save();
}

void CommandStorage::save()
{
    if (!changed)
        return;
    if (saved)
        return;
    // and save at the end
    try
    {
        save1();
        saved = true;
    }
    catch (std::exception &e)
    {
        LOG_ERROR(logger, "Error during command db save: " << e.what());
    }
    lock.reset();
}

detail::FileHolder &detail::Storage::getCommandLog(const SwBuilderContext &swctx, const path &root)
{
    if (!commands)
        commands = std::make_unique<FileHolder>(getCommandsLogFileName(root));
    return *commands;
}

detail::FileHolder &detail::Storage::getFileLog(const SwBuilderContext &swctx, const path &root)
{
    if (!files)
        files = std::make_unique<FileHolder>(getCommandsLogFileName(root) += getFilesSuffix());
    return *files;
}

void CommandStorage::load()
{
    fdb.load(s.file_storage, s.file_storage_by_hash, s.storage, root);
}

void CommandStorage::save1()
{
    fdb.save(s.file_storage, s, s.storage, root);
}

ConcurrentCommandStorage &CommandStorage::getStorage()
{
    return getInternalStorage().storage;
}

detail::Storage &CommandStorage::getInternalStorage()
{
    return s;
}

std::pair<CommandRecord *, bool> CommandStorage::insert(size_t hash)
{
    return getStorage().insert(hash);
}

path CommandStorage::getLockFileName() const
{
    return root / "build";
}

std::unique_ptr<ScopedFileLock> CommandStorage::getLock() const
{
    return std::make_unique<ScopedFileLock>(getLockFileName());
}

}
