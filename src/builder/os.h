// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "cppan_version.h"
#include "platform.h"

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

    // add Apple = macos/ios/tvos etc.
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

SW_BUILDER_API
String toString(OSType e);

SW_BUILDER_API
String toString(ArchType e);

SW_BUILDER_API
String toString(SubArchType e);

SW_BUILDER_API
String toString(EnvironmentType e);

SW_BUILDER_API
String toString(ObjectFormatType e);

struct OS
{
    OSType Type = OSType::UnknownOS;
    ArchType Arch = ArchType::UnknownArch;
    SubArchType SubArch = SubArchType::NoSubArch;
    EnvironmentType EnvironmentType = EnvironmentType::UnknownEnvironment;
    ObjectFormatType ObjectFormatType = ObjectFormatType::UnknownObjectFormat;
    Version Version;

    // TODO:
    bool support_dynamic_loading = true;
};

SW_BUILDER_API
OS detectOS();

}
