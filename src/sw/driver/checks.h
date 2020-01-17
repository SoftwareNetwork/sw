// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "options.h"
#include "types.h"

#include <sw/manager/package.h>
#include <sw/builder/command.h>

#include <list>
#include <unordered_map>

// native

namespace sw
{

struct Build;
struct SwBuild;
struct SwContext;
struct Checker;
struct CheckSet;
struct ChecksStorage;
struct NativeCompiledTarget;

namespace builder
{
struct Command;
}

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
    Declaration,
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

struct SW_DRIVER_CPP_API Check : CommandNode
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
    mutable std::optional<CheckValue> Value;

    // symbol name (function, include, c/cxx source etc.)
    // or source code
    // or whatever
    String data;

    CheckSet *check_set = nullptr;
    mutable bool requires_manual_setup = false;
    mutable path executable; // for cc copying

    Check() = default;
    Check(const Check &) = delete;
    Check &operator=(const Check &) = delete;
    virtual ~Check();

    // for comparison
    virtual size_t getHash() const;

    String getName(bool short_name = false) const override;
    String getData() const { return data; }
    bool isChecked() const;
    std::vector<std::shared_ptr<Check>> gatherDependencies();
    void execute() override;
    void prepare() override {}
    void addInputOutputDeps() {} // compat with command
    std::optional<String> getDefinition() const;
    std::optional<String> getDefinition(const String &d) const;
    virtual String getSourceFileContents() const = 0;
    virtual CheckType getType() const = 0;
    void clean() const;

    bool lessDuringExecution(const CommandNode &rhs) const override;

protected:
    virtual void run() const {}
    path getOutputFilename() const;
    Build setupSolution(SwBuild &b, const path &f) const;
    TargetSettings getSettings() const;
    virtual void setupTarget(NativeCompiledTarget &t) const;

    [[nodiscard]]
    bool execute(SwBuild &) const;

private:
    mutable std::vector<std::shared_ptr<builder::Command>> commands; // for cleanup
    mutable path uniq_name;

private:
    const path &getUniqueName() const;
};

using CheckPtr = std::shared_ptr<Check>;

struct SW_DRIVER_CPP_API FunctionExists : Check
{
    FunctionExists(const String &f, const String &def = "");

    void run() const override;
    String getSourceFileContents() const override;
    CheckType getType() const override { return CheckType::Function; }

protected:
    FunctionExists() = default;
};

struct SW_DRIVER_CPP_API IncludeExists : Check
{
    IncludeExists(const String &i, const String &def = "");

    void run() const override;
    String getSourceFileContents() const override;
    CheckType getType() const override { return CheckType::Include; }
};

struct SW_DRIVER_CPP_API TypeSize : Check
{
    TypeSize(const String &t, const String &def = "");

    void run() const override;
    String getSourceFileContents() const override;
    CheckType getType() const override { return CheckType::Type; }
};

struct SW_DRIVER_CPP_API TypeAlignment : Check
{
    TypeAlignment(const String &t, const String &def = "");

    void run() const override;
    String getSourceFileContents() const override;
    CheckType getType() const override { return CheckType::TypeAlignment; }
};

// If the symbol is a type, enum value, or intrinsic it will not be recognized
// (consider using :module : `CheckTypeSize` or :module : `CheckCSourceCompiles`).
struct SW_DRIVER_CPP_API SymbolExists : Check
{
    SymbolExists(const String &s, const String &def = "");

    void run() const override;
    String getSourceFileContents() const override;
    CheckType getType() const override { return CheckType::Symbol; }
};

struct SW_DRIVER_CPP_API DeclarationExists : Check
{
    DeclarationExists(const String &d, const String &def = "");

    void run() const override;
    String getSourceFileContents() const override;
    CheckType getType() const override { return CheckType::Declaration; }
};

struct SW_DRIVER_CPP_API StructMemberExists : Check
{
    String struct_;
    String member;

    StructMemberExists(const String &struct_, const String &member, const String &def = "");

    void run() const override;
    size_t getHash() const override;
    String getSourceFileContents() const override;
    CheckType getType() const override { return CheckType::StructMember; }
};

struct SW_DRIVER_CPP_API LibraryFunctionExists : FunctionExists
{
    String library;
    String function;

    LibraryFunctionExists(const String &library, const String &function, const String &def = "");

    size_t getHash() const override;
    CheckType getType() const override { return CheckType::LibraryFunction; }

private:
    void setupTarget(NativeCompiledTarget &t) const override;
};

struct SW_DRIVER_CPP_API SourceCompiles : Check
{
    SourceCompiles(const String &def, const String &source);

    void run() const override;
    String getSourceFileContents() const override;
    CheckType getType() const override { return CheckType::SourceCompiles; }
};

struct SW_DRIVER_CPP_API SourceLinks : Check
{
    SourceLinks(const String &def, const String &source);

    void run() const override;
    String getSourceFileContents() const override;
    CheckType getType() const override { return CheckType::SourceLinks; }
};

struct SW_DRIVER_CPP_API SourceRuns : Check
{
    SourceRuns(const String &def, const String &source);

    void run() const override;
    String getSourceFileContents() const override;
    CheckType getType() const override { return CheckType::SourceRuns; }
};

struct CheckSet;

struct SW_DRIVER_CPP_API CheckSet1
{
    String name;
    const NativeCompiledTarget *t = nullptr;
    std::unordered_map<String, CheckPtr> check_values;

    // we store all checks first, because they are allowed to have post setup
    // so we cant calculate hash after ctor
    // list is to keep iterators alive
    // make public, so it is possible to batch add prefixes, for example
    // for (auto &[s, check] : s.checks)
    //   check->Prefixes.insert("U_");
    std::list<CheckPtr> all;

    template <class T, class ... Args>
    std::shared_ptr<T> add(Args && ... args)
    {
        auto t = std::make_shared<T>(std::forward<Args>(args)...);
        t->check_set = (CheckSet*)this;
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

    FunctionExists &checkFunctionExists(const String &function, bool cpp = false);
    FunctionExists &checkFunctionExists(const String &function, const String &def, bool cpp = false);

    Check &checkIncludeExists(const String &function, bool cpp = false);
    Check &checkIncludeExists(const String &function, const String &def, bool cpp = false);

    Check &checkLibraryExists(const String &library, bool cpp = false);
    Check &checkLibraryExists(const String &library, const String &def, bool cpp = false);

    Check &checkLibraryFunctionExists(const String &library, const String &function, bool cpp = false);
    Check &checkLibraryFunctionExists(const String &library, const String &function, const String &def, bool cpp = false);

    Check &checkSymbolExists(const String &symbol, bool cpp = false);
    Check &checkSymbolExists(const String &symbol, const String &def, bool cpp = false);

    Check &checkStructMemberExists(const String &s, const String &member, bool cpp = false);
    Check &checkStructMemberExists(const String &s, const String &member, const String &def, bool cpp = false);

    Check &checkDeclarationExists(const String &decl, bool cpp = false);
    Check &checkDeclarationExists(const String &decl, const String &def, bool cpp = false);

    Check &checkTypeSize(const String &type, bool cpp = false);
    Check &checkTypeSize(const String &type, const String &def, bool cpp = false);

    Check &checkTypeAlignment(const String &type, bool cpp = false);
    Check &checkTypeAlignment(const String &type, const String &def, bool cpp = false);

    Check &checkSourceCompiles(const String &def, const String &src, bool cpp = false);
    Check &checkSourceLinks(const String &def, const String &src, bool cpp = false);
    Check &checkSourceRuns(const String &def, const String &src, bool cpp = false);

    auto begin() { return all.begin(); }
    auto end() { return all.end(); }

    auto begin() const { return all.begin(); }
    auto end() const { return all.end(); }

private:
    // set's checks
    std::unordered_map<size_t /* hash */, CheckPtr> checks;
    // prevents recursive checking on complex queries, complex settings
    // in crosscompilation tasks and environments
    //std::mutex m;

    friend struct Checker;
    friend struct CheckSet;
};

struct SW_DRIVER_CPP_API CheckSet : CheckSet1
{
    Checker &checker;

    CheckSet(Checker &checker);
    CheckSet(const CheckSet &) = delete;
    CheckSet &operator=(const CheckSet &) = delete;

    void prepareChecksForUse();
    void performChecks(const TargetSettings &);
};

struct SW_DRIVER_CPP_API Checker
{
    SwBuild &swbld;

    /// child sets
    std::unordered_map<String /* set name */, std::shared_ptr<CheckSet>> sets;

    Checker(SwBuild &swbld);

    CheckSet &addSet(const String &name);

private:
    // all checks are stored here
    std::unordered_map<size_t /* hash */, CheckPtr> checks;
};

}
