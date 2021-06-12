// SPDX-License-Identifier: AGPL-3.0-only

#ifndef GENERATOR2
#define GENERATOR2(x) GENERATOR(x, #x)
#define GENERATOR2_DEFINED
#endif

// everything
GENERATOR2(Batch)
GENERATOR2(CMake)
GENERATOR(CompilationDatabase, "Compilation Database")
GENERATOR(FastBuild, "Fast Build")
GENERATOR2(Make)
GENERATOR2(NMake)
GENERATOR2(Ninja)
GENERATOR(RawBootstrapBuild, "Raw Bootstrap Build")
GENERATOR2(QMake)
GENERATOR2(Shell)
// sw
GENERATOR(SwExecutionPlan, "Sw Execution Plan")
GENERATOR(SwBuildDescription, "Sw Build Description")
// IDE
GENERATOR(CodeBlocks, "Code Blocks")
GENERATOR(VisualStudio, "Visual Studio")
GENERATOR2(Xcode)
// qt creator?

#ifdef GENERATOR2_DEFINED
#undef GENERATOR2
#undef GENERATOR2_DEFINED
#endif
