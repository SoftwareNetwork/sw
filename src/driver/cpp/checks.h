// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "language.h"
#include "types.h"

#include <package.h>

#include <list>
#include <unordered_map>

// native

namespace sw
{

struct Checker;
struct CheckSet;
struct ChecksStorage;

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

    size_t getHash() const;
};

// setting this to int64 make errors on variable operations
//           int64_t * int = error no property conversion
// t.Variables["SIZEOF_UNSIGNED_LONG"] * 8;
//
// to solve this add property conversions
//using CheckValue = int64_t;
using CheckValue = int;

struct SW_DRIVER_CPP_API Check : CommandData<Check>
{
    // every check has its definition to be added to compilation process
    // e.g. HAVE_STDINT_H
    // also with definitions (for types etc.)
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
    mutable optional<CheckValue> Value;

    CheckSet *check_set = nullptr;

    // symbol name (function, include, c/cxx source etc.)
    // or source code
    // or whatever
    String data;

    Check() = default;
    Check(const Check &) = delete;
    Check &operator=(const Check &) = delete;
    virtual ~Check() = default;

    // for comparison
    virtual size_t getHash() const;

    bool isChecked() const;
    std::vector<std::shared_ptr<Check>> gatherDependencies();
    void execute() override;
    void prepare() override {}
    void addInputOutputDeps() {} // compat with command
    std::optional<String> getDefinition() const;
    std::optional<String> getDefinition(const String &d) const;
    virtual String getSourceFileContents() const = 0;

    bool lessDuringExecution(const Check &rhs) const;

protected:
    virtual void run() const {}
    path getOutputFilename() const;
    Solution setupSolution(const path &f) const;
};

using CheckPtr = std::shared_ptr<Check>;

struct SW_DRIVER_CPP_API FunctionExists : Check
{
    FunctionExists(const String &f, const String &def = "");

    void run() const override;
    String getSourceFileContents() const override;
};

struct SW_DRIVER_CPP_API IncludeExists : Check
{
    IncludeExists(const String &i, const String &def = "");

    void run() const override;
    String getSourceFileContents() const override;
};

struct SW_DRIVER_CPP_API TypeSize : Check
{
    TypeSize(const String &t, const String &def = "");

    void run() const override;
    String getSourceFileContents() const override;
};

struct SW_DRIVER_CPP_API TypeAlignment : Check
{
    TypeAlignment(const String &t, const String &def = "");

    void run() const override;
    String getSourceFileContents() const override;
};

// If the symbol is a type, enum value, or intrinsic it will not be recognized
// (consider using :module : `CheckTypeSize` or :module : `CheckCSourceCompiles`).
struct SW_DRIVER_CPP_API SymbolExists : Check
{
    SymbolExists(const String &s, const String &def = "");

    void run() const override;
    String getSourceFileContents() const override;
};

struct SW_DRIVER_CPP_API DeclarationExists : Check
{
    DeclarationExists(const String &d, const String &def = "");

    void run() const override;
    String getSourceFileContents() const override;
};

struct SW_DRIVER_CPP_API StructMemberExists : Check
{
    String struct_;
    String member;

    StructMemberExists(const String &struct_, const String &member, const String &def = "");

    void run() const override;
    size_t getHash() const override;
    String getSourceFileContents() const override;
};

struct SW_DRIVER_CPP_API LibraryFunctionExists : Check
{
    String library;
    String function;

    LibraryFunctionExists(const String &library, const String &function, const String &def = "");

    void run() const override;
    size_t getHash() const override;
    String getSourceFileContents() const override;
};

struct SW_DRIVER_CPP_API SourceCompiles : Check
{
    SourceCompiles(const String &def, const String &source);

    void run() const override;
    String getSourceFileContents() const override;
};

struct SW_DRIVER_CPP_API SourceLinks : Check
{
    SourceLinks(const String &def, const String &source);

    void run() const override;
    String getSourceFileContents() const override;
};

struct SW_DRIVER_CPP_API SourceRuns : Check
{
    SourceRuns(const String &def, const String &source);

    void run() const override;
    String getSourceFileContents() const override;
};

struct SW_DRIVER_CPP_API CheckSet
{
    Checker &checker;
    std::unordered_map<String, CheckPtr> check_values;

    // we store all checks first, because they are allowed to have post setup
    // so we cant calculate hash after ctor
    // list is to keep iterators alive
    // make public, so it is possible to batch add prefixes, for example
    // for (auto &[s, check] : s.checks)
    //   check->Prefixes.insert("U_");
    std::list<CheckPtr> all;

    CheckSet(Checker &checker);

    template <class T, class ... Args>
    std::shared_ptr<T> add(Args && ... args)
    {
        auto t = std::make_shared<T>(std::forward<Args>(args)...);
        t->check_set = this;
        all.push_back(t);
        return t;
    }

    template <class T, class ... Args>
    std::shared_ptr<T> get(Args && ... args)
    {
        auto t = std::make_shared<T>(std::forward<Args>(args)...);
        auto i = checks.find(t->getHash());
        if (i == checks.end())
            throw SW_RUNTIME_ERROR("Missing check: " + *t->Definitions.begin());
        return std::static_pointer_cast<T>(i->second);
    }

    void prepareChecksForUse();

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

private:
    // set's checks
    std::unordered_map<size_t /* hash */, CheckPtr> checks;

    friend struct Checker;
};

struct SW_DRIVER_CPP_API Checker
{
    const struct Solution *solution = nullptr;

    /// child sets
    std::unordered_map<PackageVersionGroupNumber, std::unordered_map<String /* set name */, CheckSet>> sets;

    /// some unique identification of current module
    PackageVersionGroupNumber current_gn = 0;

    Checker();

    CheckSet &addSet(const String &name);
    void performChecks(path checks_results_dir);

private:
    // all checks are stored here
    std::unordered_map<size_t /* hash */, CheckPtr> checks;

    std::unique_ptr<ChecksStorage> checksStorage;
};

}
