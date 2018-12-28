// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <compiler.h>
#include <language_type.h>
#include <source_file.h>

#include <primitives/filesystem.h>

namespace sw
{

struct Target;
struct SourceFile;

// factory?
// actually language is something like rules
struct SW_DRIVER_CPP_API Language : Node
{
    //LanguageType Type = LanguageType::UnspecifiedLanguage;
    StringSet CompiledExtensions;

    virtual ~Language() = default;

    virtual std::shared_ptr<Language> clone() const = 0;
    virtual std::shared_ptr<SourceFile> createSourceFile(const Target &t, const path &input) const = 0;
};

template <class T>
struct CompiledLanguage
{
    std::shared_ptr<T> compiler;
};

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4661)
#endif

template <class SF, class C>
struct NativeLanguageBase : Language,
    CompiledLanguage<C>
{
    NativeLanguageBase() = default;
    NativeLanguageBase(const NativeLanguageBase &rhs)
        : Language(rhs)
    {
        if (rhs.compiler)
            this->compiler = std::dynamic_pointer_cast<C>(rhs.compiler->clone());
    }

    path getOutputFile(const path &input, const Target &t) const;

    std::shared_ptr<Language> clone() const override
    {
        return std::make_shared<NativeLanguageBase<SF, C>>(*this);
    }

    std::shared_ptr<SourceFile> createSourceFile(const Target &t, const path &input) const override
    {
        return std::make_shared<SF>(t, (C*)this->compiler.get(), input, getOutputFile(input, t));
    }
};

#if __clang__ && defined(_MSC_VER)
#error "clang-cl does not work here, find workaround!"
#elif defined(_MSC_VER)
#define SW_EXTERN_TEMPLATE_RAW(_type_, _extern_, _api_) \
    _extern_ template class _api_ _type_
//#define SW_EXTERN_TEMPLATE(_type_, _api_) \
    //SW_EXTERN_TEMPLATE_RAW(_type_, (CONCATENATE(_api_, _EXTERN)), _api_)
#else
#define SW_EXTERN_TEMPLATE_RAW(_type_, _extern_, _api_) \
    template class _type_
#define SW_EXTERN_TEMPLATE(_type_, _api_) \
    SW_EXTERN_TEMPLATE_RAW(_type_, , _api_)
#endif

#if __clang__ && defined(_MSC_VER)
#error "clang-cl does not work here, find workaround!"
#elif defined(_MSC_VER)
//SW_DRIVER_CPP_API_EXTERN
template struct SW_DRIVER_CPP_API NativeLanguageBase<NativeSourceFile, NativeCompiler>;
//SW_DRIVER_CPP_API_EXTERN
template struct SW_DRIVER_CPP_API NativeLanguageBase<RcToolSourceFile, RcTool>;
#else
template struct NativeLanguageBase<NativeSourceFile, NativeCompiler>;
template struct NativeLanguageBase<RcToolSourceFile, RcTool>;
#endif

using NativeLanguage = NativeLanguageBase<NativeSourceFile, NativeCompiler>;
using RcToolLanguage = NativeLanguageBase<RcToolSourceFile, RcTool>;

#if defined(_MSC_VER)
//#pragma warning(pop)
#endif

struct SW_DRIVER_CPP_API CSharpLanguage : Language
{
    std::shared_ptr<CSharpCompiler> compiler;

    std::shared_ptr<Language> clone() const override;
    std::shared_ptr<SourceFile> createSourceFile(const Target &t, const path &input) const override;
};

struct SW_DRIVER_CPP_API RustLanguage : Language
{
    std::shared_ptr<RustCompiler> compiler;

    std::shared_ptr<Language> clone() const override;
    std::shared_ptr<SourceFile> createSourceFile(const Target &t, const path &input) const override;
};

struct SW_DRIVER_CPP_API GoLanguage : Language
{
    std::shared_ptr<GoCompiler> compiler;

    std::shared_ptr<Language> clone() const override;
    std::shared_ptr<SourceFile> createSourceFile(const Target &t, const path &input) const override;
};

struct SW_DRIVER_CPP_API FortranLanguage : Language
{
    std::shared_ptr<FortranCompiler> compiler;

    std::shared_ptr<Language> clone() const override;
    std::shared_ptr<SourceFile> createSourceFile(const Target &t, const path &input) const override;
};

}
