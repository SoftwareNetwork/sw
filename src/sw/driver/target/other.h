// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "native1.h"

namespace sw
{

// Ada

struct SW_DRIVER_CPP_API AdaTarget : Target
    , NativeTargetOptionsGroup
{
    std::shared_ptr<AdaCompiler> compiler;

    AdaTarget(TargetBase &parent, const PackageId &);

    SW_TARGET_USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    bool init() override;
    DependenciesType gatherDependencies() const override { return NativeTargetOptionsGroup::gatherDependencies(); }
    Files gatherAllFiles() const override { return NativeTargetOptionsGroup::gatherAllFiles(); }

private:
    Commands getCommands1() const override;
};

struct SW_DRIVER_CPP_API AdaExecutable : AdaTarget
{
    using Base = AdaTarget;
    using Base::Base;
};

// C#

struct SW_DRIVER_CPP_API CSharpTarget : Target
    , NativeTargetOptionsGroup
{
    std::shared_ptr<CSharpCompiler> compiler;

    CSharpTarget(TargetBase &parent, const PackageId &);

    SW_TARGET_USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    TargetType getType() const override { return TargetType::CSharpLibrary; }

    bool init() override;
    DependenciesType gatherDependencies() const override { return NativeTargetOptionsGroup::gatherDependencies(); }
    Files gatherAllFiles() const override { return NativeTargetOptionsGroup::gatherAllFiles(); }

private:
    Commands getCommands1() const override;
};

struct SW_DRIVER_CPP_API CSharpExecutable : CSharpTarget
{
    using Base = CSharpTarget;
    using Base::Base;

    TargetType getType() const override { return TargetType::CSharpExecutable; }
};

// Rust

struct SW_DRIVER_CPP_API RustTarget : Target
    , NativeTargetOptionsGroup
{
    std::shared_ptr<RustCompiler> compiler;

    RustTarget(TargetBase &parent, const PackageId &);

    SW_TARGET_USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    TargetType getType() const override { return TargetType::RustLibrary; }

    bool init() override;
    DependenciesType gatherDependencies() const override { return NativeTargetOptionsGroup::gatherDependencies(); }
    Files gatherAllFiles() const override { return NativeTargetOptionsGroup::gatherAllFiles(); }

private:
    Commands getCommands1() const override;
};

struct SW_DRIVER_CPP_API RustExecutable : RustTarget
{
    using Base = RustTarget;
    using Base::Base;
    TargetType getType() const override { return TargetType::RustExecutable; }
};

// Go

struct SW_DRIVER_CPP_API GoTarget : Target
    , NativeTargetOptionsGroup
{
    std::shared_ptr<GoCompiler> compiler;

    GoTarget(TargetBase &parent, const PackageId &);

    SW_TARGET_USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    TargetType getType() const override { return TargetType::GoLibrary; }

    bool init() override;
    DependenciesType gatherDependencies() const override { return NativeTargetOptionsGroup::gatherDependencies(); }
    Files gatherAllFiles() const override { return NativeTargetOptionsGroup::gatherAllFiles(); }

private:
    Commands getCommands1() const override;
};

struct SW_DRIVER_CPP_API GoExecutable : GoTarget
{
    using Base = GoTarget;
    using Base::Base;
    TargetType getType() const override { return TargetType::GoExecutable; }
};

// Java

struct SW_DRIVER_CPP_API JavaTarget : Target
    , NativeTargetOptionsGroup
{
    std::shared_ptr<JavaCompiler> compiler;

    JavaTarget(TargetBase &parent, const PackageId &);

    SW_TARGET_USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    TargetType getType() const override { return TargetType::JavaLibrary; }

    bool init() override;
    DependenciesType gatherDependencies() const override { return NativeTargetOptionsGroup::gatherDependencies(); }
    Files gatherAllFiles() const override { return NativeTargetOptionsGroup::gatherAllFiles(); }

private:
    Commands getCommands1() const override;
};

struct SW_DRIVER_CPP_API JavaExecutable : JavaTarget
{
    using Base = JavaTarget;
    using Base::Base;
    TargetType getType() const override { return TargetType::JavaExecutable; }
};

// Kotlin

struct SW_DRIVER_CPP_API KotlinTarget : Target
    , NativeTargetOptionsGroup
{
    std::shared_ptr<KotlinCompiler> compiler;

    KotlinTarget(TargetBase &parent, const PackageId &);

    SW_TARGET_USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    TargetType getType() const override { return TargetType::KotlinLibrary; }

    bool init() override;
    DependenciesType gatherDependencies() const override { return NativeTargetOptionsGroup::gatherDependencies(); }
    Files gatherAllFiles() const override { return NativeTargetOptionsGroup::gatherAllFiles(); }

private:
    Commands getCommands1() const override;
};

struct SW_DRIVER_CPP_API KotlinExecutable : KotlinTarget
{
    using Base = KotlinTarget;
    using Base::Base;
    TargetType getType() const override { return TargetType::KotlinExecutable; }
};

// D

struct SW_DRIVER_CPP_API DTarget : NativeTarget
    , NativeTargetOptionsGroup
{
    std::shared_ptr<DCompiler> compiler;

    DTarget(TargetBase &parent, const PackageId &);

    SW_TARGET_USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    TargetType getType() const override { return TargetType::DLibrary; }

    bool init() override;
    DependenciesType gatherDependencies() const override { return NativeTargetOptionsGroup::gatherDependencies(); }
    Files gatherAllFiles() const override { return NativeTargetOptionsGroup::gatherAllFiles(); }

private:
    Commands getCommands1() const override;

    //
    bool isStaticLibrary() const override { return false; }
    NativeLinker *getSelectedTool() const override;
};

struct SW_DRIVER_CPP_API DLibrary : DTarget
{
    using Base = DTarget;
    using Base::Base;
};

struct SW_DRIVER_CPP_API DStaticLibrary : DLibrary
{
    using Base = DLibrary;
    using Base::Base;
    bool init() override;
    TargetType getType() const override { return TargetType::DStaticLibrary; }

    bool isStaticLibrary() const override { return true; }
};

struct SW_DRIVER_CPP_API DSharedLibrary : DLibrary
{
    using Base = DLibrary;
    using Base::Base;
    bool init() override;
    TargetType getType() const override { return TargetType::DSharedLibrary; }
};

struct SW_DRIVER_CPP_API DExecutable : DTarget
{
    using Base = DTarget;
    using Base::Base;
    bool init() override;
    TargetType getType() const override { return TargetType::DExecutable; }
};

// Pascal

struct SW_DRIVER_CPP_API PascalTarget : Target
    , NativeTargetOptionsGroup
{
    std::shared_ptr<PascalCompiler> compiler;

    PascalTarget(TargetBase &parent, const PackageId &);

    SW_TARGET_USING_ASSIGN_OPS(NativeTargetOptionsGroup);

    bool init() override;
    DependenciesType gatherDependencies() const override { return NativeTargetOptionsGroup::gatherDependencies(); }
    Files gatherAllFiles() const override { return NativeTargetOptionsGroup::gatherAllFiles(); }

private:
    Commands getCommands1() const override;
};

struct SW_DRIVER_CPP_API PascalExecutable : PascalTarget
{
    using Base = PascalTarget;
    using Base::Base;
};

// Python

#define SW_TARGET_ADD_DEPENDENCIES(target, add_dep_function) \
private:                                                     \
    ASSIGN_WRAPPER_SIMPLE(add, PythonLibrary);               \
                                                             \
public:                                                      \
    ASSIGN_TYPES_NO_REMOVE(Target)                           \
    ASSIGN_TYPES_NO_REMOVE(PackageId)                        \
    ASSIGN_TYPES_NO_REMOVE(DependencyPtr)                    \
    ASSIGN_TYPES_NO_REMOVE(UnresolvedPackage)                \
    ASSIGN_TYPES_NO_REMOVE(UnresolvedPackages)               \
                                                             \
    DependencyPtr operator+(const ITarget &t)                \
    {                                                        \
        auto d = std::make_shared<Dependency>(t);            \
        add(d);                                              \
        return d;                                            \
    }                                                        \
    DependencyPtr operator+(const DependencyPtr &d)          \
    {                                                        \
        add(d);                                              \
        return d;                                            \
    }                                                        \
    DependencyPtr operator+(const PackageId &pkg)            \
    {                                                        \
        auto d = std::make_shared<Dependency>(pkg);          \
        add(d);                                              \
        return d;                                            \
    }                                                        \
    DependencyPtr operator+(const UnresolvedPackage &pkg)    \
    {                                                        \
        auto d = std::make_shared<Dependency>(pkg);          \
        add(d);                                              \
        return d;                                            \
    }                                                        \
    void add(const Target &t)                                \
    {                                                        \
        add(std::make_shared<Dependency>(t));                \
    }                                                        \
    void add(const DependencyPtr &t)                         \
    {                                                        \
        addSourceDependency(t);                              \
    }                                                        \
    void add(const UnresolvedPackage &t)                     \
    {                                                        \
        add(std::make_shared<Dependency>(t));                \
    }                                                        \
    void add(const UnresolvedPackages &t)                    \
    {                                                        \
        for (auto &d : t)                                    \
            add(d);                                          \
    }                                                        \
    void add(const PackageId &p)                             \
    {                                                        \
        add(std::make_shared<Dependency>(p));                \
    }

struct SW_DRIVER_CPP_API PythonLibrary : Target
    , SourceFileTargetOptions
{
    PythonLibrary(TargetBase &parent, const PackageId &);

    using Target::operator+=;
    using Target::operator=;
    using Target::add;
    SW_TARGET_USING_ASSIGN_OPS(SourceFileTargetOptions);

    bool init() override;
    Files gatherAllFiles() const override;

    SW_TARGET_ADD_DEPENDENCIES(PythonLibrary, addSourceDependency)
};

}
