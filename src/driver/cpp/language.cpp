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
#include <target/native.h>

#include <primitives/hash.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "language");

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
    LOG_DEBUG(logger, "registering program: " + pp.toString() + ", path: " + p->file.u8string());
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

LanguagePtr LanguageStorage::activateLanguage(const PackagePath &pp)
{
    auto v = user_defined_languages[pp];
    if (v.empty())
        return {};
    if (!v.empty_releases())
        return activateLanguage({ pp, v.rbegin_releases()->first }, true);
    return activateLanguage({ pp, v.rbegin()->first }, true);
}

LanguagePtr LanguageStorage::activateLanguage(const PackageId &pkg, bool exact_version)
{
    auto v = user_defined_languages[pkg.ppath];
    if (v.empty())
        return {};
    auto L = v.find(pkg.version);
    if (L == v.end())
    {
        if (exact_version)
            return {};
        auto i = primitives::version::findBestMatch(v.rbegin(), v.rend(), pkg.version, true);
        if (i == v.rend())
            return {};
        L = i.base();
        L--;
    }
    for (auto &l : L->second->CompiledExtensions)
        extensions[l] = pkg;
    return L->second;
}

LanguagePtr LanguageStorage::getLanguage(const PackagePath &pp) const
{
    auto i = user_defined_languages.find(pp);
    if (i == user_defined_languages.end(pp))
        return {};
    auto &v = i->second;
    if (v.empty())
        return {};
    if (!v.empty_releases())
        return getLanguage({ pp, v.rbegin_releases()->first }, true);
    return getLanguage({ pp, v.rbegin()->first }, true);
}

LanguagePtr LanguageStorage::getLanguage(const PackageId &pkg, bool exact_version) const
{
    auto vi = user_defined_languages.find(pkg.ppath);
    if (vi == user_defined_languages.end(pkg.ppath))
        return {};
    auto &v = vi->second;
    auto L = v.find(pkg.version);
    if (L == v.end())
    {
        if (exact_version)
            return {};
        auto i = primitives::version::findBestMatch(v.rbegin(), v.rend(), pkg.version, true);
        if (i == v.rend())
            return {};
        L = i.base();
        L--;
    }
    return L->second;
}

std::shared_ptr<Program> LanguageStorage::getProgram(const PackagePath &pp) const
{
    auto i = registered_programs.find(pp);
    if (i == registered_programs.end(pp))
        return {};
    auto &v = i->second;
    if (v.empty())
        return {};
    if (!v.empty_releases())
        return getProgram({ pp, v.rbegin_releases()->first }, true);
    return getProgram({ pp, v.rbegin()->first }, true);
}

std::shared_ptr<Program> LanguageStorage::getProgram(const PackageId &pkg, bool exact_version) const
{
    auto vi = registered_programs.find(pkg.ppath);
    if (vi == registered_programs.end(pkg.ppath))
        return {};
    auto &v = vi->second;
    auto L = v.find(pkg.version);
    if (L == v.end())
    {
        if (exact_version)
            return {};
        auto i = primitives::version::findBestMatch(v.rbegin(), v.rend(), pkg.version, true);
        if (i == v.rend())
            return {};
        L = i.base();
        L--;
    }
    return L->second;
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
