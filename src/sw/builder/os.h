// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (C) 2017-2020 Egor Pugin

#pragma once

#include "platform.h"

#include <sw/support/version.h>

#include <optional>

namespace sw
{

// Some system headers or GCC predefined macros conflict with identifiers in
// this file.  Undefine them here.
#undef NetBSD
#undef mips
#undef sparc

// from Modules/CMakeDetermineSystem.cmake
enum class OSType
{
    UnknownOS,

    AIX,
    Android,
    BSD_OS,
    Cygwin,
    FreeBSD,
    HP_UX,
    IRIX,
    Linux,
    GNU_kFreeBSD,
    NetBSD,
    OpenBSD,
    OFS1,
    SCO_OpenServer5,
    SCO_UnixWare7,
    SCO_UnixWare_pre7,
    SCO_XENIX,
    Solaris,
    SunOS,
    Tru64,
    Ultrix,
    Windows,
    WindowsCE,

    // add apple's tvos/watchos etc.
    Darwin,
    Macos,
    IOS,

    Mingw, // mingw32, mingw64, msys, msys2, msys64?
};

// from llvm/ADT/Triple.h
enum class ArchType
{
    UnknownArch,

    arm,            // ARM (little endian): arm, armv.*, xscale
    armeb,          // ARM (big endian): armeb
    aarch64,        // AArch64 (little endian): aarch64
    aarch64_be,     // AArch64 (big endian): aarch64_be
    avr,            // AVR: Atmel AVR microcontroller
    bpfel,          // eBPF or extended BPF or 64-bit BPF (little endian)
    bpfeb,          // eBPF or extended BPF or 64-bit BPF (big endian)
    hexagon,        // Hexagon: hexagon
    mips,           // MIPS: mips, mipsallegrex
    mipsel,         // MIPSEL: mipsel, mipsallegrexel
    mips64,         // MIPS64: mips64
    mips64el,       // MIPS64EL: mips64el
    msp430,         // MSP430: msp430
    nios2,          // NIOSII: nios2
    ppc,            // PPC: powerpc
    ppc64,          // PPC64: powerpc64, ppu
    ppc64le,        // PPC64LE: powerpc64le
    r600,           // R600: AMD GPUs HD2XXX - HD6XXX
    amdgcn,         // AMDGCN: AMD GCN GPUs
    riscv32,        // RISC-V (32-bit): riscv32
    riscv64,        // RISC-V (64-bit): riscv64
    sparc,          // Sparc: sparc
    sparcv9,        // Sparcv9: Sparcv9
    sparcel,        // Sparc: (endianness = little). NB: 'Sparcle' is a CPU variant
    systemz,        // SystemZ: s390x
    tce,            // TCE (http://tce.cs.tut.fi/): tce
    tcele,          // TCE little endian (http://tce.cs.tut.fi/): tcele
    thumb,          // Thumb (little endian): thumb, thumbv.*
    thumbeb,        // Thumb (big endian): thumbeb
    x86,            // X86: i[3-9]86
    x86_64,         // X86-64: amd64, x86_64
    xcore,          // XCore: xcore
    nvptx,          // NVPTX: 32-bit
    nvptx64,        // NVPTX: 64-bit
    le32,           // le32: generic little-endian 32-bit CPU (PNaCl)
    le64,           // le64: generic little-endian 64-bit CPU (PNaCl)
    amdil,          // AMDIL
    amdil64,        // AMDIL with 64-bit pointers
    hsail,          // AMD HSAIL
    hsail64,        // AMD HSAIL with 64-bit pointers
    spir,           // SPIR: standard portable IR for OpenCL 32-bit version
    spir64,         // SPIR: standard portable IR for OpenCL 64-bit version
    kalimba,        // Kalimba: generic kalimba
    shave,          // SHAVE: Movidius vector VLIW processors
    lanai,          // Lanai: Lanai 32-bit
    wasm32,         // WebAssembly with 32-bit pointers
    wasm64,         // WebAssembly with 64-bit pointers
    renderscript32, // 32-bit RenderScript
    renderscript64, // 64-bit RenderScript
};

// from llvm/ADT/Triple.h
enum class SubArchType
{
    NoSubArch,

    ARMSubArch_v8_2a,
    ARMSubArch_v8_1a,
    ARMSubArch_v8,
    ARMSubArch_v8r,
    ARMSubArch_v8m_baseline,
    ARMSubArch_v8m_mainline,
    ARMSubArch_v7,
    ARMSubArch_v7em,
    ARMSubArch_v7m,
    ARMSubArch_v7s,
    ARMSubArch_v7k,
    ARMSubArch_v7ve,
    ARMSubArch_v6,
    ARMSubArch_v6m,
    ARMSubArch_v6k,
    ARMSubArch_v6t2,
    ARMSubArch_v5,
    ARMSubArch_v5te,
    ARMSubArch_v4t,

    KalimbaSubArch_v3,
    KalimbaSubArch_v4,
    KalimbaSubArch_v5
};

// from llvm/ADT/Triple.h
enum class EnvironmentType
{
    UnknownEnvironment,

    GNU,
    GNUABI64,
    GNUEABI,
    GNUEABIHF,
    GNUX32,
    CODE16,
    EABI,
    EABIHF,
    Android,
    Musl,
    MuslEABI,
    MuslEABIHF,

    MSVC,
    Itanium,
    Cygnus,
    AMDOpenCL,
    CoreCLR,
    OpenCL,
};

// from llvm/ADT/Triple.h
enum class ObjectFormatType
{
    UnknownObjectFormat,

    COFF,
    ELF,
    MachO,
    Wasm,
};

enum class ShellType
{
    UnknownShell,

    Batch,
    Bat = Batch,
    Shell,
    Sh = Shell,
};

SW_BUILDER_API
String toString(OSType e);

SW_BUILDER_API
String toString(ArchType e);

SW_BUILDER_API
String toStringWindows(ArchType e);

SW_BUILDER_API
String toString(SubArchType e);

SW_BUILDER_API
String toString(EnvironmentType e);

SW_BUILDER_API
String toString(ObjectFormatType e);

SW_BUILDER_API
String toTripletString(OSType e);

SW_BUILDER_API
String toTripletString(ArchType e);

SW_BUILDER_API
String toTripletString(SubArchType e);

struct SW_BUILDER_API OS
{
    OSType Type = OSType::UnknownOS;
    ArchType Arch = ArchType::UnknownArch;
    SubArchType SubArch = SubArchType::NoSubArch;
    EnvironmentType EnvType = EnvironmentType::UnknownEnvironment;
    ObjectFormatType ObjectFormatType1 = ObjectFormatType::UnknownObjectFormat;
    std::optional<::primitives::version::Version> Version;

    // TODO:
    bool support_dynamic_loading = true;

    bool is(OSType t) const { return Type == t; }
    bool isApple() const;
    bool is(ArchType t) const { return Arch == t; }
    bool is(SubArchType t) const { return SubArch == t; }
    bool is(EnvironmentType t) const { return EnvType == t; }
    bool is(ObjectFormatType t) const { return ObjectFormatType1 == t; }

    // rename?
    bool canRunTargetExecutables(const OS &target_os) const;

    String getExecutableExtension() const;
    String getStaticLibraryExtension() const;
    String getSharedLibraryExtension() const;
    String getObjectFileExtension() const;
    String getLibraryPrefix() const;

    String getShellExtension() const;
    ShellType getShellType() const;

    bool operator<(const OS &rhs) const;
    bool operator==(const OS &rhs) const;

    static bool isMingwShell();
};

SW_BUILDER_API
const OS &getHostOS();

}
