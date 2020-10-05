// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "source_file.h"

#include "command.h"
#include "build.h"
#include "target/native.h"

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "source_file");

// we can do global cache:
// [sourcedir][f] = files
// file_cache
// glob_cache

namespace sw
{

#ifdef _WIN32
static bool IsWindows7OrLater() {
    OSVERSIONINFOEX version_info =
    { sizeof(OSVERSIONINFOEX), 6, 1, 0, 0,{ 0 }, 0, 0, 0, 0, 0 };
    DWORDLONG comparison = 0;
    VER_SET_CONDITION(comparison, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(comparison, VER_MINORVERSION, VER_GREATER_EQUAL);
    return VerifyVersionInfo(
        &version_info, VER_MAJORVERSION | VER_MINORVERSION, comparison);
}

static Files enumerate_files1(const path &dir, bool recursive = true)
{
    Files files;
    // FindExInfoBasic is 30% faster than FindExInfoStandard.
    static bool can_use_basic_info = IsWindows7OrLater();
    // This is not in earlier SDKs.
    const FINDEX_INFO_LEVELS kFindExInfoBasic =
        static_cast<FINDEX_INFO_LEVELS>(1);
    FINDEX_INFO_LEVELS level =
        can_use_basic_info ? kFindExInfoBasic : FindExInfoStandard;
    WIN32_FIND_DATA ffd;
    HANDLE find_handle = FindFirstFileEx((dir.wstring() + L"\\*").c_str(), level, &ffd,
        FindExSearchNameMatch, NULL, 0);

    if (find_handle == INVALID_HANDLE_VALUE)
    {
        DWORD win_err = GetLastError();
        if (win_err == ERROR_FILE_NOT_FOUND || win_err == ERROR_PATH_NOT_FOUND)
            return files;
        return files;
    }
    do
    {
        if (wcscmp(ffd.cFileName, TEXT(".")) == 0 || wcscmp(ffd.cFileName, TEXT("..")) == 0)
            continue;
        // skip any links
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
            continue;
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (recursive)
            {
                auto f2 = enumerate_files1(dir / ffd.cFileName, recursive);
                files.insert(f2.begin(), f2.end());
            }
        }
        else
            files.insert(dir / ffd.cFileName);
    } while (FindNextFile(find_handle, &ffd));
    FindClose(find_handle);
    return files;
}
#endif

static Files enumerate_files_fast(const path &dir, bool recursive = true)
{
    return
#ifdef _WIN32
        enumerate_files1(dir, recursive);
#else
        enumerate_files(dir, recursive);
#endif
}

SourceFileStorage::SourceFileStorage(Target &t)
    : target(t)
{
}

SourceFileStorage::~SourceFileStorage()
{
}

void SourceFileStorage::addFile(const path &p, const std::shared_ptr<SourceFile> &f)
{
    source_files[p] = f;
    f->index = index++;
}

std::shared_ptr<SourceFile> SourceFileStorage::getFileInternal(const path &p) const
{
    auto i = source_files.find(p);
    if (i != source_files.end())
        return i->second;
    return {};
}

void SourceFileStorage::removeFile(const path &p)
{
    source_files.erase(p);
}

bool SourceFileStorage::hasFile(const path &p) const
{
    return source_files.find(p) != source_files.end();
}

void SourceFileStorage::add_unchecked(const path &file_in, bool skip)
{
    auto file = file_in;

    // ignore missing file when file is skipped and non local
    if (!check_absolute(file, !target.isLocal() && skip))
        return;

    auto f = getFileInternal(file);

    auto ext = file.extension().string();
    auto nt = target.as<NativeCompiledTarget*>();
    auto ho = nt && nt->HeaderOnly && nt->HeaderOnly.value();
    //if (!target.hasExtension(ext) || ho)
    {
        f = std::make_shared<SourceFile>(file);
        addFile(file, f);
        //f->created = false;
    }
    /*else
    {
        if (!f
            // || f->postponed
            )
        {
            if (!target.getProgram(ext))
            {
                // only unresolved dep for now
                //if (f && f->postponed)
                    //throw SW_RUNTIME_ERROR("Postponing postponed file");
                f = std::make_shared<SourceFile>(file);
                addFile(file, f);
                //f->postponed = true;
            }
            else
            {
                // program was provided
                auto p = target.findProgramByExtension(ext);
                auto f2 = f;
                auto p2 = dynamic_cast<FileToFileTransformProgram*>(p);
                if (!p2)
                    throw SW_RUNTIME_ERROR("Bad program type");
                f = p2->createSourceFile(target, file);
                addFile(file, f);
                if (f2
                    // && f2->postponed
                    )
                {
                    // retain some data
                    f->args = f2->args;
                    f->skip = f2->skip;
                }
            }
        }
    }*/
    if (autodetect)
        f->skip |= skip;
    else
        f->skip = skip;
}

void SourceFileStorage::add(const std::shared_ptr<SourceFile> &f)
{
    if (target.DryRun || !f)
        return;

    addFile(f->file, f);
}

void SourceFileStorage::add(const path &file)
{
    if (target.DryRun)
        return;

    add_unchecked(file);
}

void SourceFileStorage::add(const Files &Files)
{
    for (auto &f : Files)
        add(f);
}

void SourceFileStorage::add(const FileRegex &r)
{
    if (target.DryRun)
        return;

    add(target.SourceDir, r);
}

void SourceFileStorage::add(const path &root, const FileRegex &r)
{
    if (target.DryRun)
        return;

    auto r2 = r;
    r2.dir = root / r2.dir;
    add1(r2);
}

void SourceFileStorage::remove(const path &file)
{
    if (target.DryRun)
        return;

    add_unchecked(file, true);
}

void SourceFileStorage::remove(const Files &files)
{
    for (auto &f : files)
        remove(f);
}

void SourceFileStorage::remove(const FileRegex &r)
{
    if (target.DryRun)
        return;

    remove(target.SourceDir, r);
}

void SourceFileStorage::remove(const path &root, const FileRegex &r)
{
    if (target.DryRun)
        return;

    auto r2 = r;
    r2.dir = root / r2.dir;
    remove1(r2);
}

void SourceFileStorage::remove_exclude(const path &file)
{
    remove_full(file);
}

void SourceFileStorage::remove_exclude(const Files &files)
{
    for (auto &f : files)
        remove_full(f);
}

void SourceFileStorage::remove_exclude(const FileRegex &r)
{
    remove_exclude(target.SourceDir, r);
}

void SourceFileStorage::remove_exclude(const path &root, const FileRegex &r)
{
    if (target.DryRun)
        return;

    auto r2 = r;
    r2.dir = root / r2.dir;
    remove_full1(r2);
}

void SourceFileStorage::remove_full(const path &file)
{
    if (target.DryRun)
        return;

    auto F = file;
    // ignore missing file only when non local
    // nope, ignore always
    if (check_absolute(F, true/*!target.isLocal()*/))
        removeFile(F);
    else if (target.isLocal())
        LOG_WARN(logger, "excluded file is missing: " + to_string(normalize_path(file)));
}

void SourceFileStorage::add1(const FileRegex &r)
{
    op(r, &SourceFileStorage::add);
}

void SourceFileStorage::remove1(const FileRegex &r)
{
    op(r, &SourceFileStorage::remove);
}

void SourceFileStorage::remove_full1(const FileRegex &r)
{
    op(r, &SourceFileStorage::remove_full);
}

void SourceFileStorage::op(const FileRegex &r, Op func)
{
    auto dir = r.dir;
    if (!dir.is_absolute())
        dir = target.SourceDir / dir;
    auto root_s = to_string(normalize_path(dir));
    if (root_s.back() == '/')
        root_s.resize(root_s.size() - 1);
    auto &files = glob_cache[dir][r.recursive];
    if (files.empty())
        files = enumerate_files_fast(dir, r.recursive);

    bool matches = false;
    for (auto &f : files)
    {
        auto s = to_string(normalize_path(f));
        if (s.size() < root_s.size() + 1)
            continue; // file is in bdir or somthing like that
        if (s.find(root_s) != 0)
            continue;
        s = s.substr(root_s.size() + 1); // + 1 to skip first slash
        if (std::regex_match(s, r.r))
        {
            (this->*func)(f);
            matches = true;
        }
    }
    if (!matches && target.isLocal() && !target.AllowEmptyRegexes)
    {
        String err = target.getPackage().toString() + ": No files matching regex: " + r.getRegexString();
        if (target.getMainBuild().getSettings()["ignore_source_files_errors"] == "true")
        {
            LOG_INFO(logger, err);
            return;
        }
        throw SW_RUNTIME_ERROR(err);
    }
}

size_t SourceFileStorage::sizeKnown() const
{
    return std::count_if(begin(), end(),
        [](const auto &p) { return !p.second->skip; });
}

size_t SourceFileStorage::sizeSkipped() const
{
    return size() - sizeKnown();
}

SourceFile &SourceFileStorage::operator[](path F)
{
    static SourceFile sf("static_source_file");
    if (target.DryRun)
        return sf;
    check_absolute(F);
    auto f = getFileInternal(F);
    if (!f)
    {
        // here we may let other fibers progress until language is registered
        throw SW_RUNTIME_ERROR(target.getPackage().toString() + ": Empty source file: " + to_string(F.u8string()));
    }
    return *f;
}

SourceFileMap<SourceFile> SourceFileStorage::operator[](const FileRegex &r) const
{
    return enumerate_files(r, true);
}

bool SourceFileStorage::check_absolute(path &F, bool ignore_errors, bool *source_dir) const
{
    auto i = files_cache.find(F);
    bool found = i != files_cache.end();
    if (found)
        F = i->second;

    // apply EnforcementType::CheckFiles
    if (!F.is_absolute())
    {
        auto p = target.SourceDir / F;
        if (source_dir)
            *source_dir = true;
        if (!fs::exists(p))
        {
            p = target.BinaryDir / F;
            if (source_dir)
                *source_dir = false;
            if (!fs::exists(p))
            {
                if (!File(p, target.getFs()).isGeneratedAtAll())
                {
                    if (ignore_errors)
                        return false;
                    String err = target.getPackage().toString() + ": Cannot find source file: " + to_string((target.SourceDir / F).u8string());
                    if (target.getMainBuild().getSettings()["ignore_source_files_errors"] == "true")
                    {
                        LOG_INFO(logger, err);
                        return true;
                    }
                    throw SW_RUNTIME_ERROR(err);
                }
            }
        }
        files_cache[F] = p; // assign to old F
        F = p; // assign to F
    }
    else
    {
        if (!found && !fs::exists(F))
        {
            if (!File(F, target.getFs()).isGeneratedAtAll())
            {
                if (ignore_errors)
                    return false;
                String err = target.getPackage().toString() + ": Cannot find source file: " + to_string(F.u8string());
                if (target.getMainBuild().getSettings()["ignore_source_files_errors"] == "true")
                {
                    LOG_INFO(logger, err);
                    return true;
                }
                throw SW_RUNTIME_ERROR(err);
            }
        }
        if (source_dir)
        {
            // source file is checked
            if (is_under_root_by_prefix_path(F, target.SourceDir))
                *source_dir = true;
            else if (is_under_root_by_prefix_path(F, target.BinaryDir) || is_under_root_by_prefix_path(F, target.BinaryPrivateDir))
                *source_dir = false;
            else
            {
                // this is an error!
                throw SW_RUNTIME_ERROR(to_string(normalize_path(F)) + " is not under src or bin dir");

                // other path
                //LOG_DEBUG(logger, F << " is not under src or bin dir");
                //*source_dir = true;
            }
        }
        if (!found)
            files_cache[F] = F;
    }
    return true;
}

void SourceFileStorage::mergeFiles(const SourceFileStorage &v, const GroupSettings &s)
{
    auto &t = getTarget();
    auto nt = t.as<NativeCompiledTarget *>();
    if (!nt)
        return;
    for (auto &f : v)
         *nt += f.first;
}

void SourceFileStorage::merge(const SourceFileStorage &v, const GroupSettings &s)
{
    source_files.insert(v.begin(), v.end());
}

SourceFileMap<SourceFile>
SourceFileStorage::enumerate_files(const FileRegex &r, bool allow_empty) const
{
    auto dir = r.dir;
    if (!dir.is_absolute())
        dir = target.SourceDir / dir;
    auto root_s = to_string(normalize_path(dir));
    if (root_s.back() == '/')
        root_s.resize(root_s.size() - 1);

    std::unordered_map<path, std::shared_ptr<SourceFile>> files;
    for (auto &[p, f] : *this)
    {
        auto s = to_string(normalize_path(p));
        if (s.size() < root_s.size() + 1)
            continue; // file is in bdir or something like that
        if (s.find(root_s) != 0)
            continue;
        s = s.substr(root_s.size() + 1); // + 1 to skip first slash
        if (std::regex_match(s, r.r))
            files[p] = f;
    }
    if (!target.DryRun) // special case
    if (files.empty() && target.isLocal() && !target.AllowEmptyRegexes && !allow_empty)
    {
        String err = target.getPackage().toString() + ": No files matching regex: " + r.getRegexString();
        if (target.getMainBuild().getSettings()["ignore_source_files_errors"] == "true")
        {
            LOG_INFO(logger, err);
            return files;
        }
        throw SW_RUNTIME_ERROR(err);
    }
    return files;
}

void SourceFileStorage::clearGlobCache()
{
    glob_cache.clear();
    files_cache.clear();
}

SourceFile::SourceFile(const path &input)
    : file(input)
{
}

/*path SourceFile::getObjectFilename(const Target &t, const path &p)
{
    // target may push its files to outer packages,
    // so files must be concatenated with its target name
    // ^^^ wrong?
    // target push files, they'll use local definitions etc.
    return to_string(p.filename().u8string()) + "." + sha256(
        //t.pkg.toString() +
        to_string(p.u8string())).substr(0, 8);
}*/

bool SourceFile::isActive() const
{
    return /*created && */!skip /* && !isRemoved(f.first)*/;
}

/*NativeSourceFile::NativeSourceFile(const NativeCompiler &c, const path &input, const path &o)
    : SourceFile(input)
    , compiler(c.clone())
    , output(o)
{
    getCompiler().setSourceFile(input, output);
}

NativeSourceFile::NativeSourceFile(const NativeSourceFile &rhs)
    : SourceFile(rhs)
{
    output = rhs.output;
    compiler = rhs.compiler->clone();
}

NativeSourceFile::~NativeSourceFile()
{
}

NativeCompiler &NativeSourceFile::getCompiler() const
{
    if (!compiler)
        throw SW_RUNTIME_ERROR("Compiler was not set");
    return static_cast<NativeCompiler &>(*compiler);
}

void NativeSourceFile::setOutputFile(const path &o)
{
    output = o;
    getCompiler().setSourceFile(file, output);
}

void NativeSourceFile::setOutputFile(const Target &t, const path &input, const path &output_dir)
{
    setOutputFile(output_dir / getObjectFilename(t, input));
}

path NativeSourceFile::getObjectFilename(const Target &t, const path &p) const
{
    return SourceFile::getObjectFilename(t, p) += getCompiler().getObjectExtension(t.getBuildSettings().TargetOS);
}

std::shared_ptr<builder::Command> NativeSourceFile::getCommand(const Target &t) const
{
    auto cmd = getCompiler().getCommand(t);
    for (auto &d : dependencies)
    {
        if (d)
            cmd->dependencies.insert(d->getCommand(t));
    }
    return cmd;
}*/

/*RcToolSourceFile::RcToolSourceFile(const RcTool &c, const path &input, const path &o)
    : SourceFile(input)
    , compiler(c.clone())
    , output(o)
{
    getCompiler().setSourceFile(input);
    getCompiler().setOutputFile(output);
}

std::shared_ptr<builder::Command> RcToolSourceFile::getCommand(const Target &t) const
{
    auto cmd = getCompiler().getCommand(t);
    return cmd;
}

RcTool &RcToolSourceFile::getCompiler() const
{
    if (!compiler)
        throw SW_RUNTIME_ERROR("Compiler was not set");
    return static_cast<RcTool &>(*compiler);
}*/

}
