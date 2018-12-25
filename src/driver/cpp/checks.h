// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "language.h"
#include "types.h"

#include <unordered_map>

// native

namespace sw
{

struct Checker;

enum class CheckType
{
    Function,
    Include,
    Type,
    TypeAlignment,
    Library,
    LibraryFunction,
    Symbol,
    StructMember,
    SourceCompiles,
    SourceLinks,
    SourceRuns,
    Decl,
    Custom,

    Max,
};

struct SW_DRIVER_CPP_API CheckParameters
{
    DefinitionsType Definitions;
    Strings Includes;
    PathOptionsType IncludeDirectories;
    PathOptionsType Libraries;
    StringSet Options;
    bool cpp = false;
};

struct SW_DRIVER_CPP_API Check : CommandData<Check>
{
    // every check has its definition to be added to compilation process
    // e.g. HAVE_STDINT_H
    String Definition;

    // Additional definitions (for types etc.)
    StringSet Definitions;

    // Additional prefixes (for types etc.)
    StringSet Prefixes;

    // by default do not define symbol if it has value 0
    // otherwise define as HAVE_SOMETHING=0
    bool DefineIfZero = false;

    // all checks could be C or CPP
    bool CPP = false;

    // all checks have their parameters
    CheckParameters Parameters;

    // result value
    mutable int Value = 0;

    Checker *checker = nullptr;

    // symbol name (function, include, c/cxx source etc.)
    // or source code
    // or whatever
    String data;

    Check() = default;
    Check(const Check &) = delete;
    Check &operator=(const Check &) = delete;
    virtual ~Check() = default;

    virtual void init() {}
    // for comparison
    virtual size_t getHash() const { return 0; }
    bool isChecked() const;
    void updateDependencies();
    void execute() override;
    void prepare() override {}
    void addInputOutputDeps() {} // compat with command
    std::optional<String> getDefinition() const;
    std::optional<String> getDefinition(const String &d) const;

    bool lessDuringExecution(const Check &rhs) const;

protected:
    virtual void run() const {}
};

using Checks = std::unordered_map<String, std::shared_ptr<Check>>;

struct SW_DRIVER_CPP_API FunctionExists : Check
{
    FunctionExists(const String &f, const String &def = "");

    void run() const override;
};

struct SW_DRIVER_CPP_API IncludeExists : Check
{
    IncludeExists(const String &i, const String &def = "");

    void run() const override;
};

struct SW_DRIVER_CPP_API TypeSize : Check
{
    TypeSize(const String &t, const String &def = "");

    void init() override;
    void run() const override;
};

struct SW_DRIVER_CPP_API TypeAlignment : Check
{
    TypeAlignment(const String &t, const String &def = "");

    void init() override;
    void run() const override;
};

// If the symbol is a type, enum value, or intrinsic it will not be recognized
// (consider using :module : `CheckTypeSize` or :module : `CheckCSourceCompiles`).
struct SW_DRIVER_CPP_API SymbolExists : Check
{
    SymbolExists(const String &s, const String &def = "");

    void run() const override;
};

struct SW_DRIVER_CPP_API DeclarationExists : Check
{
    DeclarationExists(const String &d, const String &def = "");

    void init() override;
    void run() const override;
};

struct SW_DRIVER_CPP_API StructMemberExists : Check
{
    String s;
    String member;

    StructMemberExists(const String &s, const String &member, const String &def = "");

    void run() const override;
};

struct SW_DRIVER_CPP_API LibraryFunctionExists : Check
{
    String library;
    String function;

    LibraryFunctionExists(const String &library, const String &function, const String &def = "");

    void run() const override;
};

struct SW_DRIVER_CPP_API SourceCompiles : Check
{
    SourceCompiles(const String &def, const String &source);

    void run() const override;
};

struct SW_DRIVER_CPP_API SourceLinks : Check
{
    SourceLinks(const String &def, const String &source);

    void run() const override;
};

struct SW_DRIVER_CPP_API SourceRuns : Check
{
    SourceRuns(const String &def, const String &source);

    void run() const override;
};

struct SW_DRIVER_CPP_API CheckSet
{
    Checker *checker = nullptr;
    Checks checks;

    template <class T, class ... Args>
    std::shared_ptr<T> add(Args && ... args);

    FunctionExists &checkFunctionExists(const String &function, LanguageType L = LanguageType::C);
    FunctionExists &checkFunctionExists(const String &function, const String &def, LanguageType L = LanguageType::C);

    Check &checkIncludeExists(const String &function, LanguageType L = LanguageType::C);
    Check &checkIncludeExists(const String &function, const String &def, LanguageType L = LanguageType::C);

    Check &checkLibraryExists(const String &library, LanguageType L = LanguageType::C);
    Check &checkLibraryExists(const String &library, const String &def, LanguageType L = LanguageType::C);

    Check &checkLibraryFunctionExists(const String &library, const String &function, LanguageType L = LanguageType::C);
    Check &checkLibraryFunctionExists(const String &library, const String &function, const String &def, LanguageType L = LanguageType::C);

    Check &checkSymbolExists(const String &symbol, LanguageType L = LanguageType::C);
    Check &checkSymbolExists(const String &symbol, const String &def, LanguageType L = LanguageType::C);

    Check &checkStructMemberExists(const String &s, const String &member, LanguageType L = LanguageType::C);
    Check &checkStructMemberExists(const String &s, const String &member, const String &def, LanguageType L = LanguageType::C);

    Check &checkDeclarationExists(const String &decl, LanguageType L = LanguageType::C);
    Check &checkDeclarationExists(const String &decl, const String &def, LanguageType L = LanguageType::C);

    Check &checkTypeSize(const String &type, LanguageType L = LanguageType::C);
    Check &checkTypeSize(const String &type, const String &def, LanguageType L = LanguageType::C);

    Check &checkTypeAlignment(const String &type, LanguageType L = LanguageType::C);
    Check &checkTypeAlignment(const String &type, const String &def, LanguageType L = LanguageType::C);

    Check &checkSourceCompiles(const String &def, const String &src, LanguageType L = LanguageType::C);
    Check &checkSourceLinks(const String &def, const String &src, LanguageType L = LanguageType::C);
    Check &checkSourceRuns(const String &def, const String &src, LanguageType L = LanguageType::C);

    // custom check

protected:
    bool is_checker = false;
};

struct SW_DRIVER_CPP_API Checker : CheckSet
{
    const struct Solution *solution = nullptr;
    std::unordered_map<String, CheckSet> sets;

    Checker()
    {
        is_checker = true;
        checker = this;
    }

    CheckSet &addSet(const String &name)
    {
        auto p = sets.emplace(name, CheckSet());
        p.first->second.checker = this;
        return p.first->second;
    }
};

template <class T, class ... Args>
std::shared_ptr<T> CheckSet::add(Args && ... args)
{
    auto t = std::make_shared<T>(std::forward<Args>(args)...);
    if (checker)
    {
        auto i = checker->checks.find(t->Definition);
        if (i != checker->checks.end())
        {
            if (!is_checker)
            {
                auto i2 = checks.find(i->second->Definition);
                if (i2 == checks.end())
                    checks.emplace(i->second->Definition, i->second);
                // check before emplace?
                for (auto &d : i->second->Definitions)
                    checks.emplace(d, i->second);
            }
            return std::static_pointer_cast<T>(i->second);
        }
    }
    t->checker = checker;
    t->init();
    if (!is_checker)
    {
        checks.emplace(t->Definition, t);
        for (auto &d : t->Definitions)
            checks.emplace(d, t);
    }
    checker->checks.emplace(t->Definition, t);
    for (auto &d : t->Definitions)
        checker->checks.emplace(d, t);
    return t;
}

}
