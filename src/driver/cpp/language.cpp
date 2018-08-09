// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <language.h>

#include <source_file.h>
#include <target.h>

#include <primitives/hash.h>

namespace sw
{

static LanguageMap getLanguagesImpl()
{
    LanguageMap languages;

    // use base class aggregation when available in vs
    {
        auto L = std::make_shared<ASMLanguage>();
        L->Type = LanguageType::ASM;
        L->CompiledExtensions = {
#ifdef CPPAN_OS_WINDOWS
            ".asm"
#else
            ".s", ".S"
#endif
        };
        L->NonCompiledExtensions = { ".i", ".h", ".H" };
        languages[L->Type] = L;
    }

    {
        auto L = std::make_shared<CLanguage>();
        L->Type = LanguageType::C;
        L->CompiledExtensions = { ".c" };
        L->NonCompiledExtensions = { ".h", ".H" };
        languages[L->Type] = L;
    }

    {
        auto L = std::make_shared<CPPLanguage>();
        L->Type = LanguageType::CPP;
        L->CompiledExtensions = { ".cpp", ".cxx", ".c++", ".cc", ".CPP", ".C++", ".CXX", ".C", ".CC" };
        L->NonCompiledExtensions = { ".hpp", ".hxx", ".h++", ".hh", ".HPP", ".H++", ".HXX", ".H",  ".HH", ".ixx", ".ipp", ".txx", ".ixx" };
        languages[L->Type] = L;
    }

    // obj c - ".hm",

    return languages;
}

const LanguageMap &getLanguages()
{
    static const LanguageMap languages = getLanguagesImpl();
    return languages;
}

static String getObjectFilename(const Target *t, const path &p)
{
    // target may push its files to outer packages,
    // so files must be concatenated with its target name
    return sha256(t->pkg.target_name + p.u8string()).substr(0, 8) + "." + p.filename().u8string();
}

LanguageStorage::~LanguageStorage()
{
    //for (auto &[t, l] : languages)
        //delete l;
}

void LanguageStorage::addLanguage(LanguageType L)
{
    auto &lang = languages[L] = getLanguages().find(L)->second->clone();
    for (auto &l : lang->CompiledExtensions)
        extensions[l] = lang;
}

void LanguageStorage::addLanguage(const std::vector<LanguageType> &L)
{
    for (auto &l : L)
        addLanguage(l);
}

void LanguageStorage::setLanguage(LanguageType L)
{
    languages.clear();
    addLanguage(L);
}

void LanguageStorage::setLanguage(const std::vector<LanguageType> &L)
{
    languages.clear();
    for (auto &l : L)
        addLanguage(l);
}

void LanguageStorage::removeLanguage(LanguageType L)
{
    for (auto &l : getLanguages().find(L)->second->CompiledExtensions)
        extensions.erase(l);
    languages.erase(L);
}

void LanguageStorage::removeLanguage(const std::vector<LanguageType> &L)
{
    for (auto &l : L)
        removeLanguage(l);
}

std::shared_ptr<Language> ASMLanguage::clone() const
{
    return std::make_shared<ASMLanguage>(*this);
}

std::shared_ptr<SourceFile> ASMLanguage::createSourceFile(const path &input, const Target *t) const
{
    auto nt = (NativeExecutedTarget*)t;
    compiler->merge(*nt);

    auto o = t->BinaryDir.parent_path() / "obj" / (getObjectFilename(t, input) + compiler->getObjectExtension());
    o = fs::absolute(o);
    return std::make_shared<ASMSourceFile>(input, o, compiler.get());
}

std::shared_ptr<Language> CLanguage::clone() const
{
    return std::make_shared<CLanguage>(*this);
}

std::shared_ptr<SourceFile> CLanguage::createSourceFile(const path &input, const Target *t) const
{
    auto nt = (NativeExecutedTarget*)t;
    compiler->merge(*nt);

    auto o = t->BinaryDir.parent_path() / "obj" / (getObjectFilename(t, input) + compiler->getObjectExtension());
    o = fs::absolute(o);
    return std::make_shared<CSourceFile>(input, o, compiler.get());
}

std::shared_ptr<Language> CPPLanguage::clone() const
{
    return std::make_shared<CPPLanguage>(*this);
}

std::shared_ptr<SourceFile> CPPLanguage::createSourceFile(const path &input, const Target *t) const
{
    auto nt = (NativeExecutedTarget*)t;
    compiler->merge(*nt);

    auto o = t->BinaryDir.parent_path() / "obj" / (getObjectFilename(t, input) + compiler->getObjectExtension());
    o = fs::absolute(o);
    return std::make_shared<CPPSourceFile>(input, o, compiler.get());
}

}
