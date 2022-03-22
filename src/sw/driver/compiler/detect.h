// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "../build.h"
#include "../program.h"

#include <sw/builder/command.h>
#include <sw/core/sw_context.h>

#define DETECT_ARGS ::sw::Build &b
#define DETECT_ARGS_PASS b
#define DETECT_ARGS_PASS_TO_LAMBDA &b
#define DETECT_ARGS_PASS_FIRST_CALL(ctx) (::sw::SwContext&)(ctx)
#define DETECT_ARGS_PASS_FIRST_CALL_SIMPLE DETECT_ARGS_PASS_FIRST_CALL(getContext())

namespace sw
{

struct BuildSettings;
namespace vs { enum class RuntimeLibraryType; }

struct PredefinedProgramTarget : PredefinedTarget, PredefinedProgram
{
    using PredefinedTarget::PredefinedTarget;
};

struct SimpleProgram : Program
{
    using Program::Program;

    std::unique_ptr<Program> clone() const override { return std::make_unique<SimpleProgram>(*this); }
    std::shared_ptr<builder::Command> getCommand() const override
    {
        if (!cmd)
        {
            cmd = std::make_shared<builder::Command>();
            cmd->setProgram(file);
        }
        return cmd;
    }

private:
    mutable std::shared_ptr<builder::Command> cmd;
};

struct SW_DRIVER_CPP_API ProgramDetector
{
    ProgramDetector();

    // combined function for users
    void detectProgramsAndLibraries(DETECT_ARGS);

    String getMsvcPrefix(const path &program) const;

    static PredefinedProgramTarget &addProgram(DETECT_ARGS, const PackageName &, const PackageSettings &, const Program &);

    // actually should be PackagePath?
    // we do not want to use PackagePath here, because some entry points may detect specially versioned packages
    // e.g., org.llvm.clang-10 will look for clang-10/clang++-10 only
    using DetectablePackageEntryPointKey = UnresolvedPackageName;
    using DetectablePackageEntryPoint = std::function<void(Build &)>;
    using DetectablePackageMultiEntryPoints = std::unordered_multimap<DetectablePackageEntryPointKey, DetectablePackageEntryPoint>;
    using DetectablePackageEntryPoints = DetectablePackageMultiEntryPoints;
    static DetectablePackageEntryPoints getDetectablePackages();

    template <class T>
    static T &addTarget(DETECT_ARGS, const PackageName &id, const PackageSettings &ts)
    {
        log_msg_detect_target("Detected target: " + id.toString() + ": " + ts.toString());

        auto t = std::make_unique<T>(PackageIdFull{ id, ts });
        auto p = t.get();
        static_cast<ExtendedBuild &>(b).addTarget(std::move(t));
        return *p;
    }

    static vs::RuntimeLibraryType getMsvcLibraryType(const BuildSettings &bs);
    static String getMsvcLibraryName(const String &base, const BuildSettings &bs);

    bool hasVsInstances() const { return !getVSInstances().empty(); }

private:
    struct VSInstance
    {
        path root;
        PackageVersion version;
    };
    using VSInstances = VersionMap<VSInstance>;

    struct MsvcInstance
    {
        VSInstance i;
        path root;
        path compiler;
        path idir;
        path host_root;
        String target;
        ArchType target_arch;

        //
        PackageVersion cl_exe_version;
        String msvc_prefix;

        MsvcInstance(const VSInstance &);

        void process(DETECT_ARGS);
        bool has_no_target_libdir() const;
        bool is_vs15plus() const;
    };

    mutable VSInstances vsinstances1;
    std::map<path, String> msvc_prefixes;

    static VSInstances gatherVSInstances();
    VSInstances &getVSInstances() const;
    static void log_msg_detect_target(const String &m);
    String getMsvcPrefix(builder::detail::ResolvableCommand c);
    auto &getMsvcIncludePrefixes() { return msvc_prefixes; }
    const auto &getMsvcIncludePrefixes() const { return msvc_prefixes; }

    DetectablePackageMultiEntryPoints detectMsvc();
    DetectablePackageMultiEntryPoints detectMsvc15Plus();
    DetectablePackageMultiEntryPoints detectMsvc14AndOlder();
    DetectablePackageMultiEntryPoints detectWindowsSdk();
    DetectablePackageMultiEntryPoints detectMsvcCommon(const MsvcInstance &);

    ProgramDetector::DetectablePackageMultiEntryPoints detectWindowsClang();
    void detectIntelCompilers(DETECT_ARGS);
    ProgramDetector::DetectablePackageMultiEntryPoints detectWindowsCompilers();
    void detectNonWindowsCompilers(DETECT_ARGS);

#define DETECT(x) void detect##x##Compilers(DETECT_ARGS);
#include "detect.inl"
#undef DETECT
};

ProgramDetector &getProgramDetector();

}
