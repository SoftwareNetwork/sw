/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef _SW_H_
#define _SW_H_

#include "build.h"
#include "command.h"
#include "functions.h"
#include "suffix.h"
#include "target/all.h"
#include "compiler/compiler_helpers.h"

#include <sw/builder/jumppad.h>
#include <sw/core/sw_context.h> // needed for execute commands
#include <sw/manager/storage.h>

// support stuff
#include <boost/algorithm/string.hpp>

// some macros
#define SW_CPP_DRIVER_API_VERSION 1

// precise usings and namespaces

// namespaces
using namespace sw::literals;
using namespace sw::source;

namespace cmd = sw::cmd;
namespace builder = sw::builder;
namespace vs = sw::vs;
namespace cl = sw::cl;

// general
//using Build = sw::SimpleBuild;
using sw::Build;
using Solution = Build;
using sw::Checker;
//using sw::Test;

using sw::builder::CommandSequence;
using sw::driver::Command;

// options
using sw::File;
using sw::FileRegex;
using sw::IncludeDirectory;
using sw::Definition;
using sw::DefinitionsType;
using sw::LinkLibrary;
using sw::Dependency;
using sw::DependencyPtr;

using sw::NativeCompilerOptions;

// pkgs
using sw::PackageId;
using sw::PackagePath;
using sw::Version;
//using sw::UnresolvedPackages;

// targets
using sw::PredefinedTarget; // from core
//
using sw::TargetBase;
using sw::Target;
using sw::ProjectTarget;
using sw::DirectoryTarget;
using sw::NativeTarget;
using sw::NativeCompiledTarget;
using NativeExecutedTarget = NativeCompiledTarget; // old
using sw::LibraryTarget;
using sw::StaticLibraryTarget;
using sw::SharedLibraryTarget;
using sw::ExecutableTarget;

// new aliases
using Project = ProjectTarget; // deprecate?
using Library = LibraryTarget;
using StaticLibrary = StaticLibraryTarget;
using SharedLibrary = SharedLibraryTarget;
using Executable = ExecutableTarget;

// other langs
using sw::CSharpTarget;
using sw::CSharpExecutable;

using sw::DLibrary;
using sw::DStaticLibrary;
using sw::DSharedLibrary;
using sw::DExecutable;

using sw::FortranTarget;
using sw::FortranExecutable;

using sw::GoTarget;
using sw::GoExecutable;

using sw::JavaTarget;
using sw::JavaExecutable;

using sw::KotlinTarget;
using sw::KotlinExecutable;

using sw::RustTarget;
using sw::RustExecutable;

using sw::ValaLibrary;
using sw::ValaStaticLibrary;
using sw::ValaSharedLibrary;
using sw::ValaExecutable;

//
using sw::ConfigureFlags;
using sw::PrecompiledHeader;

// enums
using sw::OSType;
using sw::ArchType;
using sw::CompilerType;
using sw::LinkerType;
using sw::LibraryType;
using sw::ConfigurationType;
using sw::TargetScope;
using sw::InheritanceType;

// command line options
//using sw::Program;
using sw::Compiler;
//using sw::NativeCompiler;
using sw::VisualStudioCompiler;
using sw::VisualStudioASMCompiler;
using sw::VisualStudioLinker;
using sw::GNUCompiler;
using sw::GNULibrarian;
using sw::GNULinker;

using sw::NativeSourceFile;
using sw::NativeLinkerOptions;

//

using sw::CommandLineOption;
//using sw::CommandLineOptions;

// language standards
using sw::CLanguageStandard;
using sw::CPPLanguageStandard;

#define STD_MACRO(x, p) using sw::detail::p##x;
#include <sw/driver/target/std.inl>
#undef STD_MACRO

// functions
using sw::toString;
using sw::downloadFile;

// disable custom pragma warnings
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4068) // unknown #pragma
#endif

//#ifdef _MSC_VER
//#include "sw1.h"
//#endif

#endif
