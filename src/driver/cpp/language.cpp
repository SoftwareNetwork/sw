// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <language.h>
#include <language_storage.h>

#include <dependency.h>
#include <source_file.h>
#include <solution.h>

#include <primitives/hash.h>

namespace sw
{

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

void LanguageStorage::registerLanguage(const PackageId &pkg, const LanguagePtr &L)
{
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
        //throw SW_RUNTIME_ERROR("No packages for this language");

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
    if (v == user_defined_languages.end(pp) || v->second.empty())
        return {};
    return getLanguage({ pp, v->second.rbegin()->first });
}

LanguagePtr LanguageStorage::getLanguage(const PackageId &pkg) const
{
    auto v = user_defined_languages.find(pkg);
    if (v == user_defined_languages.end(pkg))
        return {};
    return v->second;
}

std::shared_ptr<Program> LanguageStorage::getProgram(const PackagePath &pp) const
{
    auto v = registered_programs.find(pp);
    if (v == registered_programs.end(pp) || v->second.empty())
        return {};
    return getProgram({ pp, v->second.rbegin()->first });
}

std::shared_ptr<Program> LanguageStorage::getProgram(const PackageId &pkg) const
{
    auto v = registered_programs.find(pkg);
    if (v == registered_programs.end(pkg))
        return {};
    return v->second;
}

Language *LanguageStorage::findLanguageByExtension(const String &ext) const
{
    auto pi = findPackageIdByExtension(ext);
    if (!pi)
        return nullptr;
    return getLanguage(pi.value()).get();
}

Program *LanguageStorage::findProgramByExtension(const String &ext) const
{
    auto pi = findPackageIdByExtension(ext);
    if (!pi)
        return nullptr;
    return getProgram(pi.value()).get();
}

std::optional<PackageId> LanguageStorage::findPackageIdByExtension(const String &ext) const
{
    auto e = extensions.find(ext);
    if (e == extensions.end())
        return {};
    return e->second;
}

template <class SF, class C>
path NativeLanguageBase<SF, C>::getOutputFile(const path &input, const Target &t) const
{
    auto o = t.BinaryDir.parent_path() / "obj" / (SourceFile::getObjectFilename(t, input) + this->compiler->getObjectExtension());
    o = fs::absolute(o);
    return o;
}

template struct NativeLanguageBase<NativeSourceFile, NativeCompiler>;
template struct NativeLanguageBase<RcToolSourceFile, RcTool>;

std::shared_ptr<Language> CSharpLanguage::clone() const
{
    return std::make_shared<CSharpLanguage>(*this);
}

std::shared_ptr<SourceFile> CSharpLanguage::createSourceFile(const Target &t, const path &input) const
{
    return std::make_shared<CSharpSourceFile>(t, input);
}

std::shared_ptr<Language> RustLanguage::clone() const
{
    return std::make_shared<RustLanguage>(*this);
}

std::shared_ptr<SourceFile> RustLanguage::createSourceFile(const Target &t, const path &input) const
{
    return std::make_shared<RustSourceFile>(t, input);
}

std::shared_ptr<Language> GoLanguage::clone() const
{
    return std::make_shared<GoLanguage>(*this);
}

std::shared_ptr<SourceFile> GoLanguage::createSourceFile(const Target &t, const path &input) const
{
    return std::make_shared<GoSourceFile>(t, input);
}

std::shared_ptr<Language> FortranLanguage::clone() const
{
    return std::make_shared<FortranLanguage>(*this);
}

std::shared_ptr<SourceFile> FortranLanguage::createSourceFile(const Target &t, const path &input) const
{
    return std::make_shared<FortranSourceFile>(t, input);
}

std::shared_ptr<Language> JavaLanguage::clone() const
{
    return std::make_shared<JavaLanguage>(*this);
}

std::shared_ptr<SourceFile> JavaLanguage::createSourceFile(const Target &t, const path &input) const
{
    return std::make_shared<JavaSourceFile>(t, input);
}

std::shared_ptr<Language> KotlinLanguage::clone() const
{
    return std::make_shared<KotlinLanguage>(*this);
}

std::shared_ptr<SourceFile> KotlinLanguage::createSourceFile(const Target &t, const path &input) const
{
    return std::make_shared<KotlinSourceFile>(t, input);
}

std::shared_ptr<Language> DLanguage::clone() const
{
    return std::make_shared<DLanguage>(*this);
}

std::shared_ptr<SourceFile> DLanguage::createSourceFile(const Target &t, const path &input) const
{
    return std::make_shared<DSourceFile>(t, input);
}

}
