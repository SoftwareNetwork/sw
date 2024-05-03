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

#include "file.h"

#include "command.h"
#include "file_storage.h"

#include <sw/manager/settings.h>

#include <primitives/executor.h>

#include <fstream>
#include <sstream>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "file");

#define SW_EXPLAIN_FILE ".sw/misc/explain.txt"

namespace sw
{

void explainMessage(const String &subject, bool outdated, const String &reason, const String &name)
{
    static Executor explain_executor("explain executor", 1);
    static std::ofstream o([]()
    {
        fs::create_directories(path(SW_EXPLAIN_FILE).parent_path());
        return SW_EXPLAIN_FILE;
    }()); // goes first
    explain_executor.push([=]
    {
        if (!outdated)
            return;
        auto print = [&subject, &name, &reason](auto &o)
        {
            o << subject << ": " << name << "\n";
            o << "outdated\n";
            o << "reason = " << reason << "\n" << std::endl;
        };
        print(o);
        if (sw::Settings::get_user_settings().gExplainOutdatedToTrace)
        {
            std::ostringstream ss;
            print(ss);
            LOG_TRACE(logger, ss.str());
        }
    });
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

    // if we copy data during refresh() we get bad state
    // FIXME: later we must delete file data
    if (refreshed == FileData::RefreshType::InProcess)
        refreshed = FileData::RefreshType::Unrefreshed;

    return *this;
}

void FileData::reset()
{
    generator.reset();
    refreshed = FileData::RefreshType::Unrefreshed;
}

File::File(const path &p, FileStorage &fs)
    : file(p)
{
    if (file.empty())
        throw SW_RUNTIME_ERROR("Empty file");
    data = &fs.registerFile(file);
}

path File::getPath() const
{
    return file;
}

FileData &File::getFileData()
{
    return *data;
}

const FileData &File::getFileData() const
{
    return *data;
}

void FileData::refresh(const path &file)
{
    FileData::RefreshType r = FileData::RefreshType::Unrefreshed;
    if (!refreshed.compare_exchange_strong(r, FileData::RefreshType::InProcess))
        return;

    bool changed = false;
    auto s = fs::status(file);
    if (s.type() != fs::file_type::regular)
    {
        if (s.type() != fs::file_type::not_found)
            LOG_TRACE(logger, "checking for non-regular file: " << file);
        // we skip non regular files at the moment
        last_write_time = fs::file_time_type::min();
        changed = true;
    }
    else
    {
        auto t = fs::last_write_time(file);
        if (t > last_write_time)
        {
            last_write_time = t;
            changed = true;
        }
    }

    refreshed = changed ? FileData::RefreshType::Changed : FileData::RefreshType::NotChanged;
}

bool File::isChanged() const
{
    while (data->refreshed < FileData::RefreshType::NotChanged)
        data->refresh(file);
    return data->refreshed == FileData::RefreshType::Changed;
}

std::optional<String> File::isChanged(const fs::file_time_type &in, bool throw_on_missing)
{
    isChanged();
    if (data->last_write_time == fs::file_time_type::min())
        return "file is missing";
    if (data->last_write_time > in)
    {
        // if you see equal times after conversion to time_t,
        // it means that lwt resolution is higher
        return "file is newer than command time (" +
            std::to_string(file_time_type2time_t(data->last_write_time)) + " > " +
            std::to_string(file_time_type2time_t(in)) + ")";
    }
    return {};
}

bool File::isGenerated() const
{
    return !!data->generator.lock();
}

bool File::isGeneratedAtAll() const
{
    return data->generated;
}

void File::setGenerated(bool g)
{
    data->generated = g;
}

void File::setGenerator(const std::shared_ptr<builder::Command> &g, bool ignore_errors)
{
    if (!g)
        return;

    auto gold = data->generator.lock();
    auto same_command = gold && (gold != g &&
                                 !gold->isExecuted() &&
                                 gold->getHash() != g->getHash());
    if (!ignore_errors && same_command)
    {
        String err;
        err += "Setting generator twice on file: " + to_string(file) + "\n";
        if (gold)
        {
            err += "first generator:\n " + gold->name + "\n";
            err += " " + gold->print() + "\n";
            err += "first generator hash:\n " + std::to_string(gold->getHash());
        }
        else
            err += "first generator is empty";
        err += "\n";
        if (g)
        {
            err += "second generator:\n " + g->name + "\n";
            err += " " + g->print() + "\n";
            err += "second generator hash:\n " + std::to_string(g->getHash());
        }
        else
            err += "second generator is empty";
        throw SW_RUNTIME_ERROR(err);
    }
    // use first command
    if (!same_command)
    {
        data->generator = g;
        data->generated = true;
    }
}

std::shared_ptr<builder::Command> File::getGenerator() const
{
    return data->generator.lock();
}

}
