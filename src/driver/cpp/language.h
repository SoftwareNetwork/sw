// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <compiler.h>

#include <primitives/filesystem.h>

#include <memory>

namespace sw
{

struct NativeTarget;
struct Target;
struct SourceFile;

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

using LanguageMap = std::unordered_map<LanguageType, std::shared_ptr<Language>>;

SW_DRIVER_CPP_API
const LanguageMap &getLanguages();

struct SW_DRIVER_CPP_API LanguageStorage
{
    LanguageMap languages;
    std::unordered_map<String, std::shared_ptr<Language>> extensions;

    ~LanguageStorage();

    // languages
#define LANG_FUNC(f)                     \
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
#undef LANG_FUNC
};

struct SW_DRIVER_CPP_API Language : Node
{
    LanguageType Type = LanguageType::UnspecifiedLanguage;
    StringSet CompiledExtensions;
    StringSet NonCompiledExtensions;
    bool IsCompiled = true;
    bool IsLinked = true;

    virtual ~Language() = default;

    virtual std::shared_ptr<Language> clone() const = 0;
    virtual std::shared_ptr<SourceFile> createSourceFile(const path &input, const Target *t) const = 0;

    bool operator<(const Language &Rhs) const { return Type < Rhs.Type; }
    bool operator==(const Language &Rhs) const { return Type == Rhs.Type; }
    bool operator==(const LanguageType &Rhs) const { return Type == Rhs; }
};

template <class T>
struct CompiledLanguage
{
    std::shared_ptr<T> compiler;
};

template <class T>
struct LibrarianLanguage
{
    std::shared_ptr<T> librarian;
};

template <class T>
struct LinkedLanguage
{
    std::shared_ptr<T> linker;
};

template <class Compiler>
struct SW_DRIVER_CPP_API NativeLanguage : Language,
    CompiledLanguage<Compiler>,
    LibrarianLanguage<NativeLinker>,
    LinkedLanguage<NativeLinker>
{
    NativeLanguage() = default;
    NativeLanguage(const NativeLanguage &rhs)
        : Language(rhs)
    {
        if (rhs.compiler)
            this->compiler = std::dynamic_pointer_cast<Compiler>(rhs.compiler->clone());
        if (rhs.librarian)
            this->librarian = std::static_pointer_cast<NativeLinker>(rhs.librarian->clone());
        if (rhs.linker)
            this->linker = std::static_pointer_cast<NativeLinker>(rhs.linker->clone());
    }
    virtual ~NativeLanguage() = default;
};

struct ASMLanguage : NativeLanguage<ASMCompiler>
{
    virtual ~ASMLanguage() = default;
    std::shared_ptr<Language> clone() const override;
    std::shared_ptr<SourceFile> createSourceFile(const path &input, const Target *t) const override;
};

struct CLanguage : NativeLanguage<CCompiler>
{
    virtual ~CLanguage() = default;
    std::shared_ptr<Language> clone() const override;
    std::shared_ptr<SourceFile> createSourceFile(const path &input, const Target *t) const override;
};

struct CPPLanguage : NativeLanguage<CPPCompiler>
{
    virtual ~CPPLanguage() = default;
    std::shared_ptr<Language> clone() const override;
    std::shared_ptr<SourceFile> createSourceFile(const path &input, const Target *t) const override;
};

}
