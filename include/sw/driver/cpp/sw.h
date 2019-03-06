// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

// builder stuff
#include <solution.h>
#include <suffix.h>
#include <jumppad.h>
#include <compiler_helpers.h>
#include <module.h>
#include <target/all.h>

// support stuff
#include <boost/algorithm/string.hpp>

//using namespace sw;
//using namespace sw::driver::cpp;

// maybe with api?
// using namespace sw::vN; // where N - version of sw api

// precise usings and namespaces

// namespaces
using namespace sw::literals;
using namespace sw::source;

namespace cmd = sw::cmd;
namespace builder = sw::builder;
namespace vs = sw::vs;
namespace cl = sw::cl;

// general
using sw::Build;
using sw::Solution;
using sw::Checker;
using sw::Test;

using sw::driver::cpp::Command;

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

using sw::NativeLanguage;
using sw::NativeSourceFile;
using sw::NativeLinkerOptions;

// TODO: remove
using sw::WithSourceFileStorage;
using sw::WithoutSourceFileStorage;
using sw::WithNativeOptions;
using sw::WithoutNativeOptions;
//

using sw::CommandLineOption;
//using sw::CommandLineOptions;

using sw::CPPLanguageStandard;

// functions
using sw::toString;

// disable custom pragma warnings
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4068) // unknown #pragma
#endif

#ifdef _MSC_VER
#include "sw1.h"
#endif
