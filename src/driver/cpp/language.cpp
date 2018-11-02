// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <language.h>

#include <source_file.h>
#include <solution.h>
#include <target.h>

#include <dependency.h>

#include <primitives/hash.h>

namespace sw
{

/*static LanguageMap getLanguagesImpl()
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

LanguageMap getLanguages()
{
    return getLanguagesImpl();
}*/

LanguageStorage::~LanguageStorage()
{
}

void LanguageStorage::registerProgramAndLanguage(const PackagePath &pp, const std::shared_ptr<Program> &p, const LanguagePtr &L)
{
    registerProgramAndLanguage({ pp, p->getVersion() }, p, L);
}

void LanguageStorage::registerProgramAndLanguage(const PackageId &pkg, const std::shared_ptr<Program> &p, const LanguagePtr &L)
{
    registerProgram(pkg, p);
    registerLanguage(pkg, L);
}

void LanguageStorage::registerProgramAndLanguage(const TargetBase &t, const std::shared_ptr<Program> &p, const LanguagePtr &L)
{
    registerProgramAndLanguage(t.pkg, p, L);
}

void LanguageStorage::registerProgram(const PackagePath &pp, const std::shared_ptr<Program> &p)
{
    registerProgram({ pp,p->getVersion() }, p);
}

void LanguageStorage::registerProgram(const PackageId &pp, const std::shared_ptr<Program> &p)
{
    auto &p2 = registered_programs[pp.ppath][pp.version] = p;
    if (auto t = dynamic_cast<TargetBase*>(this); t)
        p2->fs = t->getSolution()->fs;
}

void LanguageStorage::registerProgram(const TargetBase &t, const std::shared_ptr<Program> &p)
{
    registerProgram(t.pkg, p);
}

/*void LanguageStorage::registerLanguage(const LanguagePtr &L)
{
    // phantom pkg
    // hash of exts? probably no
    registerLanguage("loc.sw.lang" + std::to_string((size_t)L.get()), L);
}*/

void LanguageStorage::registerLanguage(const PackageId &pkg, const LanguagePtr &L)
{
    /*for (auto &e : L->CompiledExtensions)
        extensions[e] = pkg;*/
    user_defined_languages[pkg.ppath][pkg.version] = L;
}

void LanguageStorage::registerLanguage(const TargetBase &t, const LanguagePtr &L)
{
    registerLanguage(t.pkg, L);
}

void LanguageStorage::setExtensionLanguage(const String &ext, const UnresolvedPackage &p)
{
    // late resolve version
    extensions[ext] = p.resolve();

    // add a dependency to current target
    if (auto t = dynamic_cast<NativeExecutedTarget*>(this); t)
        (*t + extensions[ext])->Dummy = true;
}

void LanguageStorage::setExtensionLanguage(const String &ext, const LanguagePtr &L)
{
    auto &pkg = extensions[ext];
    if (pkg.empty())
    {
        // add phantom pkg instead?
        //throw std::runtime_error("No packages for this language");

        pkg = "loc.sw.lang" + std::to_string((size_t)L.get());
    }
    user_defined_languages[pkg.ppath][pkg.version] = L;

    // add a dependency to current target
    if (auto t = dynamic_cast<NativeExecutedTarget*>(this); t)
        (*t + extensions[ext])->Dummy = true;
}

void LanguageStorage::setExtensionLanguage(const String &ext, const DependencyPtr &d)
{
    extensions[ext] = d->getResolvedPackage();

    // add a dependency to current target
    if (auto t = dynamic_cast<NativeExecutedTarget*>(this); t)
        (*t + extensions[ext])->Dummy = true;
}

bool LanguageStorage::activateLanguage(const PackagePath &pp)
{
    auto v = user_defined_languages[pp];
    if (v.empty())
        return false;
    return activateLanguage({ pp, v.rbegin()->first });
}

bool LanguageStorage::activateLanguage(const PackageId &pkg)
{
    auto v = user_defined_languages[pkg.ppath];
    if (v.empty())
        return false;
    auto L = v[pkg.version];
    if (!L)
        return false;
    for (auto &l : L->CompiledExtensions)
        extensions[l] = pkg;
    return true;
}

LanguagePtr LanguageStorage::getLanguage(const PackagePath &pp) const
{
    auto v = user_defined_languages.find(pp);
    if (v == user_defined_languages.end() || v->second.empty())
        return {};
    return getLanguage({ pp, v->second.rbegin()->first });
}

LanguagePtr LanguageStorage::getLanguage(const PackageId &pkg) const
{
    auto v = user_defined_languages.find(pkg.ppath);
    if (v == user_defined_languages.end() || v->second.empty())
        return {};
    auto v2 = v->second.find(pkg.version);
    if (v2 == v->second.end())
        return {};
    return v2->second;
}

std::shared_ptr<Program> LanguageStorage::getProgram(const PackagePath &pp) const
{
    auto v = registered_programs.find(pp);
    if (v == registered_programs.end() || v->second.empty())
        return {};
    return getProgram({ pp, v->second.rbegin()->first });
}

std::shared_ptr<Program> LanguageStorage::getProgram(const PackageId &pkg) const
{
    auto v = registered_programs.find(pkg.ppath);
    if (v == registered_programs.end() || v->second.empty())
        return {};
    auto v2 = v->second.find(pkg.version);
    if (v2 == v->second.end())
        return {};
    return v2->second;
}

Program *LanguageStorage::findProgramByExtension(const String &ext) const
{
    auto pi = findPackageIdByExtension(ext);
    if (!pi)
        return nullptr;
    return getProgram(pi.value()).get();
}

optional<PackageId> LanguageStorage::findPackageIdByExtension(const String &ext) const
{
    auto e = extensions.find(ext);
    if (e == extensions.end())
        return {};
    return e->second;
}

/*void LanguageStorage::addLanguage(LanguageType L)
{
    /*auto &lang = languages[L] = languages.find(L)->second->clone();
    for (auto &l : lang->CompiledExtensions)
        extensions[l] = user_defined_languages[lang];
}

void LanguageStorage::addLanguage(const std::vector<LanguageType> &L)
{
    for (auto &l : L)
        addLanguage(l);
}

void LanguageStorage::setLanguage(LanguageType L)
{
    //languages.clear();
    addLanguage(L);
}

void LanguageStorage::setLanguage(const std::vector<LanguageType> &L)
{
    //languages.clear();
    for (auto &l : L)
        addLanguage(l);
}

void LanguageStorage::removeLanguage(LanguageType L)
{
    for (auto &l : languages.find(L)->second->CompiledExtensions)
        extensions.erase(l);
    languages.erase(L);
}

void LanguageStorage::removeLanguage(const std::vector<LanguageType> &L)
{
    for (auto &l : L)
        removeLanguage(l);
}*/

std::shared_ptr<Language> NativeLanguage::clone() const
{
    return std::make_shared<NativeLanguage>(*this);
}

std::shared_ptr<SourceFile> NativeLanguage::createSourceFile(const path &input, const Target *t) const
{
    auto nt = (NativeExecutedTarget*)t;
    //compiler->merge(*nt);

    auto o = t->BinaryDir.parent_path() / "obj" / (SourceFile::getObjectFilename(*t, input) + compiler->getObjectExtension());
    o = fs::absolute(o);
    return std::make_shared<NativeSourceFile>(input, *t->getSolution()->fs, o, (NativeCompiler*)compiler.get());
}

/*std::shared_ptr<Language> ASMLanguage::clone() const
{
    return std::make_shared<ASMLanguage>(*this);
}

std::shared_ptr<SourceFile> ASMLanguage::createSourceFile(const path &input, const Target *t) const
{
    auto nt = (NativeExecutedTarget*)t;
    //compiler->merge(*nt); // why here? we merge in Target after everything resolved

    auto o = t->BinaryDir.parent_path() / "obj" / (SourceFile::getObjectFilename(*t, input) + compiler->getObjectExtension());
    o = fs::absolute(o);
    return std::make_shared<ASMSourceFile>(input, *t->getSolution()->fs, o, compiler.get());
}

std::shared_ptr<Language> CLanguage::clone() const
{
    return std::make_shared<CLanguage>(*this);
}

std::shared_ptr<SourceFile> CLanguage::createSourceFile(const path &input, const Target *t) const
{
    auto nt = (NativeExecutedTarget*)t;
    //compiler->merge(*nt);

    auto o = t->BinaryDir.parent_path() / "obj" / (SourceFile::getObjectFilename(*t, input) + compiler->getObjectExtension());
    o = fs::absolute(o);
    return std::make_shared<CSourceFile>(input, *t->getSolution()->fs, o, compiler.get());
}

std::shared_ptr<Language> CPPLanguage::clone() const
{
    return std::make_shared<CPPLanguage>(*this);
}

std::shared_ptr<SourceFile> CPPLanguage::createSourceFile(const path &input, const Target *t) const
{
    auto nt = (NativeExecutedTarget*)t;
    //compiler->merge(*nt);

    auto o = t->BinaryDir.parent_path() / "obj" / (SourceFile::getObjectFilename(*t, input) + compiler->getObjectExtension());
    o = fs::absolute(o);
    return std::make_shared<CPPSourceFile>(input, *t->getSolution()->fs, o, compiler.get());
}*/

}
