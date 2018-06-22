// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "source_file.h"

#include "command.h"

#include <language.h>
#include <target.h>

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

void SourceFileStorage::add_unchecked(const path &file, bool skip)
{
    auto f = this->SourceFileMapThis::operator[](file);

    auto ext = file.extension().string();
    auto e = target->extensions.find(ext);
    if (e == target->extensions.end() ||
        (((NativeExecutedTarget*)target)->HeaderOnly && ((NativeExecutedTarget*)target)->HeaderOnly.value()))
    {
        f = this->SourceFileMapThis::operator[](file) = std::make_shared<SourceFile>(file);
        f->created = false;
    }
    else
    {
        if (!f)
        {
            auto L = e->second->clone(); // clone compiler here
            f = this->SourceFileMapThis::operator[](file) = L->createSourceFile(file, target);
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

    auto f = file;
    check_absolute(f);
    add_unchecked(f);
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

    auto f = file;
    if (check_absolute(f, true))
        add_unchecked(f, true);
}

void SourceFileStorage::remove(const Files &Files)
{
    for (auto &f : Files)
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

void SourceFileStorage::add1(const FileRegex &r)
{
    op(r, &SourceFileStorage::add);
}

void SourceFileStorage::remove1(const FileRegex &r)
{
    op(r, &SourceFileStorage::remove);
}

void SourceFileStorage::op(const FileRegex &r, Op func)
{
    auto dir = r.dir;
    if (!dir.is_absolute())
        dir = target->SourceDir / dir;
    auto root_s = normalize_path(dir);
    if (root_s.back() == '/')
        root_s.resize(root_s.size() - 1);
    for (auto &f : enumerate_files_fast(dir, r.recursive))
    {
        auto s = normalize_path(f);
        s = s.substr(root_s.size() + 1); // + 1 to skip first slash
        if (std::regex_match(s, r.r))
            (this->*func)(f);
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
    if (target->PostponeFileResolving)
        return sf;
    check_absolute(F);
    return *this->SourceFileMapThis::operator[](F);
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

/*void SourceFileStorage::resolveRemoved()
{
    for (auto &p : pp)
    {
        for (auto &f : p.to_remove.files)
            remove(f);
        for (auto &f : p.to_remove.files_regex)
            remove(f);
        for (auto &f : p.to_remove.files_regex_root)
            remove(f.first, f.second);
    }
}*/

void SourceFileStorage::startAssignOperation()
{
    //if (!pp.back().empty())
        //pp.emplace_back();
}

bool SourceFileStorage::check_absolute(path &F, bool ignore_errors) const
{
    if (fs::exists(F))
    {
        if (!F.is_absolute())
            F = fs::absolute(F);
    }
    else
    {
        if (!F.is_absolute())
        {
            auto p = target->SourceDir / F;
            if (!fs::exists(p))
            {
                p = target->BinaryDir / F;
                if (!fs::exists(p))
                {
                    if (!File(p).isGenerated())
                    {
                        if (ignore_errors)
                            return false;
                        throw std::runtime_error("Cannot find source file: " + p.string());
                    }
                }
            }
            F = fs::absolute(p);
        }
        else
        {
            if (!fs::exists(F))
            {
                if (!File(F).isGenerated())
                {
                    if (ignore_errors)
                        return false;
                    throw std::runtime_error("Cannot find source file: " + F.string());
                }
            }
        }
    }
    return true;
}

void SourceFileStorage::merge(const SourceFileStorage &v, const GroupSettings &s)
{
    for (auto &s : v)
        this->SourceFileMapThis::operator[](s.first) = s.second->clone();
}

SourceFile::SourceFile(const path &input)
    : File(input)
{
}

NativeSourceFile::NativeSourceFile(const path &input, const path &o, NativeCompiler *c)
    : SourceFile(input), compiler(c ? c->clone() : nullptr)
{
    output.file = o;
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

void NativeSourceFile::setSourceFile(const path &input, const path &o)
{
    file = input;
    setOutputFile(o);
}

void NativeSourceFile::setOutputFile(const path &o)
{
    output.file = o;
    compiler->setSourceFile(file, output.file);
}

std::shared_ptr<builder::Command> NativeSourceFile::getCommand() const
{
    auto cmd = compiler->getCommand();
    for (auto &d : dependencies)
        cmd->dependencies.insert(d->getCommand());
    return cmd;
}

Files NativeSourceFile::getGeneratedDirs() const
{
    return compiler->getGeneratedDirs();
}

ASMSourceFile::ASMSourceFile(const path &input, const path &o, ASMCompiler *c)
    : NativeSourceFile(input, o, c)
{
    compiler->setSourceFile(file, output.file);
}

std::shared_ptr<SourceFile> ASMSourceFile::clone() const
{
    return std::make_shared<ASMSourceFile>(*this);
}

CSourceFile::CSourceFile(const path &input, const path &o, CCompiler *c)
    : NativeSourceFile(input, o, c)
{
    compiler->setSourceFile(file, output.file);
}

std::shared_ptr<SourceFile> CSourceFile::clone() const
{
    return std::make_shared<CSourceFile>(*this);
}

CPPSourceFile::CPPSourceFile(const path &input, const path &o, CPPCompiler *c)
    : NativeSourceFile(input, o, c)
{
    compiler->setSourceFile(file, output.file);
}

std::shared_ptr<SourceFile> CPPSourceFile::clone() const
{
    return std::make_shared<CPPSourceFile>(*this);
}

}
