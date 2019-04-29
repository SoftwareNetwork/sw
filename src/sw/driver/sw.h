// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

// builder stuff
#include "jumppad.h"
#include "module.h"
#include "solution_build.h"
#include "suffix.h"
#include "target/all.h"
#include "compiler_helpers.h"

#include <sw/builder/sw_context.h>
#include <sw/manager/storage.h>

// support stuff
#include <boost/algorithm/string.hpp>

// precise usings and namespaces

// namespaces
using namespace sw::literals;
using namespace sw::source;

namespace cmd = sw::cmd;
namespace builder = sw::builder;
namespace vs = sw::vs;
namespace cl = sw::cl;

// general
using sw::SwContext;
using sw::Build;
using sw::Solution;
using sw::Checker;
using sw::Test;

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
using sw::TargetBase;
using sw::Target;
using sw::ProjectTarget;
using sw::DirectoryTarget;
using sw::NativeTarget;
using sw::NativeExecutedTarget;
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

// disable custom pragma warnings
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4068) // unknown #pragma
#endif

//#ifdef _MSC_VER
#include "sw1.h"
//#endif
