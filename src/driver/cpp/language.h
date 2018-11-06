// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <compiler.h>

#include <primitives/filesystem.h>

namespace sw
{

struct TargetBase;
struct Target;
struct NativeTarget;
struct SourceFile;
struct Dependency;
using DependencyPtr = std::shared_ptr<Dependency>;

enum class LanguageType
{
    UnspecifiedLanguage,

    Ada,
    ASM,
    Basic,
    C,
    COBOL,
    CPP,
    CSharp,
    CUDA,
    D,
    Dart,
    Erlang,
    FSharp,
    Fortran,
    Go,
    Haskell,
    Java,
    JavaScript,
    Kotlin,
    Lisp,
    Lua,
    Matlab,
    ObjectiveC,
    ObjectiveCPP,
    OCaml,
    OpenCL,
    Pascal,
    Perl,
    PHP,
    Prolog,
    Python,
    R,
    Ruby,
    Rust,
    Scala,
    Scheme,
    Swift,
};

struct Language;
using LanguagePtr = std::shared_ptr<Language>;
//using LanguageMap = std::unordered_map<LanguageType, LanguagePtr>;

//SW_DRIVER_CPP_API
//LanguageMap getLanguages();

struct SW_DRIVER_CPP_API LanguageStorage
{
    //LanguageMap languages;
    std::map<String, PackageId> extensions;
    std::unordered_map<PackagePath, std::map<Version, LanguagePtr>> user_defined_languages; // main languages!!! (UDL)
    std::unordered_map<PackagePath, std::map<Version, std::shared_ptr<Program>>> registered_programs; // main program storage

    virtual ~LanguageStorage();

    void registerProgramAndLanguage(const PackagePath &pp, const std::shared_ptr<Program> &, const LanguagePtr &L);
    void registerProgramAndLanguage(const PackageId &t, const std::shared_ptr<Program> &, const LanguagePtr &L);
    void registerProgramAndLanguage(const TargetBase &t, const std::shared_ptr<Program> &, const LanguagePtr &L);

    void registerProgram(const PackagePath &pp, const std::shared_ptr<Program> &);
    void registerProgram(const PackageId &pp, const std::shared_ptr<Program> &);
    void registerProgram(const TargetBase &t, const std::shared_ptr<Program> &);

    //void registerLanguage(const LanguagePtr &L); // allow unnamed UDLs?
    void registerLanguage(const PackageId &pkg, const LanguagePtr &L);
    void registerLanguage(const TargetBase &t, const LanguagePtr &L);

    void setExtensionLanguage(const String &ext, const UnresolvedPackage &p); // main
    void setExtensionLanguage(const String &ext, const LanguagePtr &p); // wrappers
    void setExtensionLanguage(const String &ext, const DependencyPtr &p); // wrappers

    bool activateLanguage(const PackagePath &pp); // latest ver
    bool activateLanguage(const PackageId &pkg);

    LanguagePtr getLanguage(const PackagePath &pp) const; // latest ver
    LanguagePtr getLanguage(const PackageId &pkg) const;

    std::shared_ptr<Program> getProgram(const PackagePath &pp) const; // latest ver
    std::shared_ptr<Program> getProgram(const PackageId &pkg) const;

    Program *findProgramByExtension(const String &ext) const;
    Language *findLanguageByExtension(const String &ext) const;
    optional<PackageId> findPackageIdByExtension(const String &ext) const;

    // languages
/*#define LANG_FUNC(f)                     \
    template <class... Args>             \
    void f(LanguageType L, Args... Ls)   \
    {                                    \
        f(L);                            \
        if constexpr (sizeof...(Ls) > 0) \
            f(Ls...);                    \
    }                                    \
    void f(LanguageType L);              \
    void f(const std::vector<LanguageType> &L)
    LANG_FUNC(addLanguage);
    LANG_FUNC(setLanguage); // clear langs and set only specified langs
    LANG_FUNC(removeLanguage); // do we need set and remove funcs?
#undef LANG_FUNC*/
};

// factory?
// actually language is something like rules
struct SW_DRIVER_CPP_API Language : Node
{
    //LanguageType Type = LanguageType::UnspecifiedLanguage;
    StringSet CompiledExtensions;
    //StringSet NonCompiledExtensions; // remove?
    bool IsCompiled = true; // move to native?
    bool IsLinked = true; // move to native?

    virtual ~Language() = default;

    virtual LanguagePtr clone() const = 0;
    virtual std::shared_ptr<SourceFile> createSourceFile(const path &input, const Target *t) const = 0;

    //bool operator<(const Language &Rhs) const { return Type < Rhs.Type; }
    //bool operator==(const Language &Rhs) const { return Type == Rhs.Type; }
    //bool operator==(const LanguageType &Rhs) const { return Type == Rhs; }
};

template <class T>
struct CompiledLanguage
{
    std::shared_ptr<T> compiler;
};

/*template <class T>
struct LibrarianLanguage
{
    std::shared_ptr<T> librarian;
};

template <class T>
struct LinkedLanguage
{
    std::shared_ptr<T> linker;
};*/

template <class Compiler>
struct SW_DRIVER_CPP_API NativeLanguage1 : Language,
    CompiledLanguage<Compiler>//,
    //LibrarianLanguage<NativeLinker>,
    //LinkedLanguage<NativeLinker>
{
    NativeLanguage1() = default;
    NativeLanguage1(const NativeLanguage1 &rhs)
        : Language(rhs)
    {
        if (rhs.compiler)
            this->compiler = std::dynamic_pointer_cast<Compiler>(rhs.compiler->clone());
        /*if (rhs.librarian)
            this->librarian = std::static_pointer_cast<NativeLinker>(rhs.librarian->clone());
        if (rhs.linker)
            this->linker = std::static_pointer_cast<NativeLinker>(rhs.linker->clone());*/
    }
    virtual ~NativeLanguage1() = default;
};

struct SW_DRIVER_CPP_API NativeLanguage : NativeLanguage1<Compiler>
{
    virtual ~NativeLanguage() = default;
    LanguagePtr clone() const override;
    std::shared_ptr<SourceFile> createSourceFile(const path &input, const Target *t) const override;
};

/*struct ASMLanguage : NativeLanguage1<ASMCompiler>
{
    virtual ~ASMLanguage() = default;
    LanguagePtr clone() const override;
    std::shared_ptr<SourceFile> createSourceFile(const path &input, const Target *t) const override;
};

struct CLanguage : NativeLanguage1<CCompiler>
{
    virtual ~CLanguage() = default;
    LanguagePtr clone() const override;
    std::shared_ptr<SourceFile> createSourceFile(const path &input, const Target *t) const override;
};

struct CPPLanguage : NativeLanguage1<CPPCompiler>
{
    virtual ~CPPLanguage() = default;
    LanguagePtr clone() const override;
    std::shared_ptr<SourceFile> createSourceFile(const path &input, const Target *t) const override;
};*/

}
