// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "platform.h"

#include <sw/manager/version.h>

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

    // add apple's tvos/watchos etc.
    AIX,
    Android,
    BSD_OS,
    Cygwin,
    FreeBSD,
    HP_UX,
    IOS,
    IRIX,
    Linux,
    GNU_kFreeBSD,
    Macos,
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

SW_BUILDER_API
OSType OSTypeFromStringCaseI(const String &s);

SW_BUILDER_API
ArchType archTypeFromStringCaseI(const String &s);

struct SW_BUILDER_API OS
{
    OSType Type = OSType::UnknownOS;
    ArchType Arch = ArchType::UnknownArch;
    SubArchType SubArch = SubArchType::NoSubArch;
    EnvironmentType EnvironmentType1 = EnvironmentType::UnknownEnvironment;
    ObjectFormatType ObjectFormatType1 = ObjectFormatType::UnknownObjectFormat;
    ::primitives::version::Version Version;

    // TODO:
    bool support_dynamic_loading = true;

    bool is(OSType t) const { return Type == t; }
    //bool isApple() const { return Type == t; } // macos/ios/tvos/watchos etc.
    bool is(ArchType t) const { return Arch == t; }
    bool is(SubArchType t) const { return SubArch == t; }
    bool is(EnvironmentType t) const { return EnvironmentType1 == t; }
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
};

struct SW_BUILDER_API OsSdk
{
    // root to sdks
    //  example: c:\\Program Files (x86)\\Windows Kits
    path Root;

    // sdk dir in root
    // win: 7.0 7.0A, 7.1, 7.1A, 8, 8.1, 10 ...
    // osx: 10.12, 10.13, 10.14 ...
    // android: 1, 2, 3, ..., 28
    path Version; // make string?

                  // windows10:
                  // 10.0.10240.0, 10.0.17763.0 ...
    path BuildNumber;

    OsSdk() = default;
    OsSdk(const OS &);
    OsSdk(const OsSdk &) = default;
    OsSdk &operator=(const OsSdk &) = default;

    path getPath(const path &subdir = {}) const;
    String getWindowsTargetPlatformVersion() const;
    void setAndroidApiVersion(int v);

    //bool operator<(const SDK &) const;
    //bool operator==(const SDK &) const;
};

SW_BUILDER_API
String getWin10KitDirName();

SW_BUILDER_API
const OS &getHostOS();

// hidden
namespace detail
{

SW_BUILDER_API
bool isHostCygwin();

}

}
