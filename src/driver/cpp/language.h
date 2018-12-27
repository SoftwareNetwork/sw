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

template <class Compiler>
struct NativeLanguage1 : Language,
    CompiledLanguage<Compiler>
{
    NativeLanguage1() = default;
    NativeLanguage1(const NativeLanguage1 &rhs)
        : Language(rhs)
    {
        if (rhs.compiler)
            this->compiler = std::dynamic_pointer_cast<Compiler>(rhs.compiler->clone());
    }
};

struct SW_DRIVER_CPP_API NativeLanguage2 : NativeLanguage1<NativeCompiler>
{
    path getOutputFile(const path &input, const Target &t) const;
};

template <class T>
struct SimpleNativeLanguageFactory : NativeLanguage2
{
    std::shared_ptr<SourceFile> createSourceFile(const Target &t, const path &input) const override
    {
        return std::make_shared<T>(t, (NativeCompiler*)compiler.get(), input, getOutputFile(input, t));
    }

    std::shared_ptr<Language> clone() const override
    {
        return std::make_shared<SimpleNativeLanguageFactory<T>>(*this);
    }
};

using NativeLanguage = SimpleNativeLanguageFactory<NativeSourceFile>;

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

}
