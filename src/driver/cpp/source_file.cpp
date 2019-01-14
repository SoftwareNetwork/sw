// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "source_file.h"

#include "command.h"
#include "solution.h"

#include <language.h>
#include <target.h>

#include <primitives/sw/settings.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "source_file");

static cl::opt<bool> ignore_source_files_errors("ignore-source-files-errors", cl::desc("Useful for debugging"));

namespace sw
{

#ifdef _WIN32
bool IsWindows7OrLater() {
    OSVERSIONINFOEX version_info =
    { sizeof(OSVERSIONINFOEX), 6, 1, 0, 0,{ 0 }, 0, 0, 0, 0, 0 };
    DWORDLONG comparison = 0;
    VER_SET_CONDITION(comparison, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(comparison, VER_MINORVERSION, VER_GREATER_EQUAL);
    return VerifyVersionInfo(
        &version_info, VER_MAJORVERSION | VER_MINORVERSION, comparison);
}

Files enumerate_files1(const path &dir, bool recursive = true)
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

Files enumerate_files_fast(const path &dir, bool recursive = true)
{
    return
#ifdef _WIN32
        enumerate_files1(dir, recursive);
#else
        enumerate_files(dir, recursive);
#endif
}

SourceFileStorage::SourceFileStorage()
{
}

SourceFileStorage::~SourceFileStorage()
{
}

Program *SourceFileStorage::findProgramByExtension(const String &ext) const
{
    auto pi = findPackageIdByExtension(ext);
    if (!pi)
        return nullptr;
    auto &pkg = pi.value();
    auto p = target->registered_programs.find(pkg);
    if (p == target->registered_programs.end(pkg))
    {
        p = target->getSolution()->registered_programs.find(pkg);
        if (p == target->getSolution()->registered_programs.end(pkg))
            return nullptr;
    }
    return p->second.get();
}

std::optional<PackageId> SourceFileStorage::findPackageIdByExtension(const String &ext) const
{
    auto e = target->findPackageIdByExtension(ext);
    if (!e)
        e = target->getSolution()->findPackageIdByExtension(ext);
    return e;
}

Language *SourceFileStorage::findLanguageByPackageId(const PackageId &p) const
{
    auto i = target->getLanguage(p);
    if (!i)
    {
        i = target->getSolution()->getLanguage(p);
        if (!i)
            return nullptr;
    }
    return i.get();
}

Language *SourceFileStorage::findLanguageByExtension(const String &ext) const
{
    auto e = findPackageIdByExtension(ext);
    if (!e)
        return {};
    return findLanguageByPackageId(e.value());
}

void SourceFileStorage::add_unchecked(const path &file_in, bool skip)
{
    auto file = file_in;
    if (!check_absolute(file, skip))
        return;

    auto f = this->SourceFileMapThis::operator[](file);

    auto ext = file.extension().string();
    auto p = findPackageIdByExtension(ext);
    auto nt = target->as<NativeExecutedTarget>();
    auto ho = nt && nt->HeaderOnly && nt->HeaderOnly.value();
    if (!p || ho)
    {
        f = this->SourceFileMapThis::operator[](file) = std::make_shared<SourceFile>(*target, file);
        f->created = false;
    }
    else
    {
        if (!f || f->postponed)
        {
            auto program = p.value();
            auto i = findLanguageByPackageId(program);
            if (!i)
            {
                //if (f && f->postponed)
                    //throw SW_RUNTIME_ERROR("Postponing postponed file");
                f = this->SourceFileMapThis::operator[](file) = std::make_shared<SourceFile>(*target, file);
                f->postponed = true;
            }
            else
            {
                auto f2 = f;
                auto L = i->clone(); // clone program here
                f = this->SourceFileMapThis::operator[](file) = L->createSourceFile(*target, file);
                if (f2 && f2->postponed)
                {
                    // retain some data
                    f->args = f2->args;
                    f->skip = f2->skip;
                }
            }

            // but maybe we create dummy file?
            //auto L = e->second->clone(); // clone program here
            //f = this->SourceFileMapThis::operator[](file) = L->createSourceFile(file, target);
        }
    }
    if (autodetect)
        f->skip |= skip;
    else
        f->skip = skip;
}

void SourceFileStorage::add(const path &file)
{
    if (target->PostponeFileResolving)
    {
        file_ops.push_back({ file, true });
        return;
    }

    add_unchecked(file);
}

void SourceFileStorage::add(const Files &Files)
{
    for (auto &f : Files)
        add(f);
}

void SourceFileStorage::add(const FileRegex &r)
{
    if (target->PostponeFileResolving)
    {
        file_ops.push_back({ r, true });
        return;
    }

    add(target->SourceDir, r);
}

void SourceFileStorage::add(const path &root, const FileRegex &r)
{
    if (target->PostponeFileResolving)
    {
        auto r2 = r;
        r2.dir = root / r2.dir;
        file_ops.push_back({ r2, true });
        return;
    }

    auto r2 = r;
    r2.dir = root / r2.dir;
    add1(r2);
}

void SourceFileStorage::remove(const path &file)
{
    if (target->PostponeFileResolving)
    {
        file_ops.push_back({ file, false });
        return;
    }

    add_unchecked(file, true);
}

void SourceFileStorage::remove(const Files &files)
{
    for (auto &f : files)
        remove(f);
}

void SourceFileStorage::remove(const FileRegex &r)
{
    if (target->PostponeFileResolving)
    {
        file_ops.push_back({ r, false });
        return;
    }

    remove(target->SourceDir, r);
}

void SourceFileStorage::remove(const path &root, const FileRegex &r)
{
    if (target->PostponeFileResolving)
    {
        auto r2 = r;
        r2.dir = root / r2.dir;
        file_ops.push_back({ r2, false });
        return;
    }

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
    remove_exclude(target->SourceDir, r);
}

void SourceFileStorage::remove_exclude(const path &root, const FileRegex &r)
{
    auto r2 = r;
    r2.dir = root / r2.dir;
    remove_full1(r2);
}

void SourceFileStorage::remove_full(const path &file)
{
    auto F = file;
    if (check_absolute(F, true))
        erase(F);
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
        dir = target->SourceDir / dir;
    auto root_s = normalize_path(dir);
    if (root_s.back() == '/')
        root_s.resize(root_s.size() - 1);
    auto &files = glob_cache[dir][r.recursive];
    if (files.empty())
        files = enumerate_files_fast(dir, r.recursive);
    bool matches = false;
    for (auto &f : files)
    {
        auto s = normalize_path(f);
        s = s.substr(root_s.size() + 1); // + 1 to skip first slash
        if (std::regex_match(s, r.r))
        {
            (this->*func)(f);
            matches = true;
        }
    }
    // some libs may declare common regex for changing files in generic manner
    // this check will fail for them
    // reconsider
    // apply EnforcementType::CheckRegexes
    //if (!matches)
        //throw SW_RUNTIME_ERROR("No files matches regex");
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
    static SourceFile sf(*target, "static_source_file");
    if (target->PostponeFileResolving)
        return sf;
    check_absolute(F);
    auto f = this->SourceFileMapThis::operator[](F);
    if (!f)
    {
        // here we may let other fibers progress until language is registered
        throw SW_RUNTIME_ERROR("Empty source file: " + F.u8string());
    }
    return *f;
}

SourceFileMap<SourceFile> SourceFileStorage::operator[](const FileRegex &r) const
{
    return enumerate_files(r);
}

void SourceFileStorage::resolve()
{
    target->PostponeFileResolving = false;

    for (auto &op : file_ops)
    {
        if (op.add)
        {
            switch (op.op.index())
            {
            case 0:
                add(std::get<0>(op.op));
                break;
            case 1:
                add1(std::get<1>(op.op));
                break;
            }
        }
        else
        {
            switch (op.op.index())
            {
            case 0:
                remove(std::get<0>(op.op));
                break;
            case 1:
                remove1(std::get<1>(op.op));
                break;
            }
        }
    }
}

bool SourceFileStorage::check_absolute(path &F, bool ignore_errors) const
{
    // apply EnforcementType::CheckFiles
    if (!F.is_absolute())
    {
        auto p = target->SourceDir / F;
        if (!fs::exists(p))
        {
            p = target->BinaryDir / F;
            if (!fs::exists(p))
            {
                if (!File(p, *target->getSolution()->fs).isGeneratedAtAll())
                {
                    if (ignore_errors)
                        return false;
                    String err = "Cannot find source file: " + (target->SourceDir / F).u8string();
                    if (ignore_source_files_errors)
                    {
                        LOG_INFO(logger, err);
                        return true;
                    }
                    throw SW_RUNTIME_ERROR(err);
                }
            }
        }
        F = fs::absolute(p);
    }
    else
    {
        if (!fs::exists(F))
        {
            if (!File(F, *target->getSolution()->fs).isGeneratedAtAll())
            {
                if (ignore_errors)
                    return false;
                String err = "Cannot find source file: " + F.u8string();
                if (ignore_source_files_errors)
                {
                    LOG_INFO(logger, err);
                    return true;
                }
                throw SW_RUNTIME_ERROR(err);
            }
        }
    }
    return true;
}

void SourceFileStorage::merge(const SourceFileStorage &v, const GroupSettings &s)
{
    for (auto &s : v)
    {
        auto f = this->SourceFileMapThis::operator[](s.first);
        if (!f)
            add(s.first);
    }
    //this->SourceFileMapThis::operator[](s.first) = s.second->clone();
}

SourceFileMap<SourceFile>
SourceFileStorage::enumerate_files(const FileRegex &r) const
{
    auto dir = r.dir;
    if (!dir.is_absolute())
        dir = target->SourceDir / dir;
    auto root_s = normalize_path(dir);
    if (root_s.back() == '/')
        root_s.resize(root_s.size() - 1);

    std::unordered_map<path, std::shared_ptr<SourceFile>> files;
    for (auto &[p, f] : *this)
    {
        auto s = normalize_path(p);
        s = s.substr(root_s.size() + 1); // + 1 to skip first slash
        if (std::regex_match(s, r.r))
            files[p] = f;
    }
    // apply EnforcementType::CheckRegexes
    //if (files.empty())
        //throw SW_RUNTIME_ERROR("No files matches regex");
    return files;
}

SourceFile::SourceFile(const Target &t, const path &input)
    : File(input, *t.getSolution()->fs)
{
}

String SourceFile::getObjectFilename(const TargetBase &t, const path &p)
{
    // target may push its files to outer packages,
    // so files must be concatenated with its target name
    return p.filename().u8string() + "." + sha256(t.pkg.toString() + p.u8string()).substr(0, 8);
}

bool SourceFile::isActive() const
{
    return created && !skip /* && !isRemoved(f.first)*/;
}

NativeSourceFile::NativeSourceFile(const Target &t, NativeCompiler *c, const path &input, const path &o)
    : SourceFile(t, input)
    , compiler(c ? std::static_pointer_cast<NativeCompiler>(c->clone()) : nullptr)
    , output(o, *t.getSolution()->fs)
{
    compiler->setSourceFile(input, output.file);
}

NativeSourceFile::NativeSourceFile(const NativeSourceFile &rhs)
    : SourceFile(rhs)
{
    output = rhs.output;
    compiler = std::static_pointer_cast<NativeCompiler>(rhs.compiler->clone());
}

NativeSourceFile::~NativeSourceFile()
{
}

void NativeSourceFile::setOutputFile(const path &o)
{
    output.file = o;
    compiler->setSourceFile(file, output.file);
}

void NativeSourceFile::setOutputFile(const TargetBase &t, const path &input, const path &output_dir)
{
    setOutputFile(output_dir / getObjectFilename(t, input));
}

String NativeSourceFile::getObjectFilename(const TargetBase &t, const path &p)
{
    return SourceFile::getObjectFilename(t, p) + compiler->getObjectExtension();
}

std::shared_ptr<builder::Command> NativeSourceFile::getCommand(const TargetBase &t) const
{
    auto cmd = compiler->getCommand(t);
    for (auto &d : dependencies)
        cmd->dependencies.insert(d->getCommand(t));
    return cmd;
}

RcToolSourceFile::RcToolSourceFile(const Target &t, RcTool *c, const path &input, const path &o)
    : SourceFile(t, input)
    , compiler(c ? std::static_pointer_cast<RcTool>(c->clone()) : nullptr)
    , output(o, *t.getSolution()->fs)
{
    compiler->setSourceFile(input);
    compiler->setOutputFile(output.file);
}

std::shared_ptr<builder::Command> RcToolSourceFile::getCommand(const TargetBase &t) const
{
    auto cmd = compiler->getCommand(t);
    return cmd;
}

}
