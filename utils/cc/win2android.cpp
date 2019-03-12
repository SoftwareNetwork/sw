void configure(Build &b)
{
    auto &s = b.addCustomSolution();

    s.Settings.TargetOS.Type = OSType::Android;
    s.Settings.TargetOS.Arch = ArchType::aarch64;
    s.Settings.Native.CompilerType = CompilerType::Clang;
    s.Settings.Native.SDK.setAndroidApiVersion(24);

    {
        {
            auto Librarian = std::make_shared<GNULibrarian>();
            Librarian->Type = LinkerType::GNU;
            Librarian->file = "d:/dev/android/sdk/ndk-bundle/toolchains/llvm/prebuilt/windows-x86_64/bin/aarch64-linux-android-ar.exe";
            Librarian->Extension = s.Settings.TargetOS.getStaticLibraryExtension();
            s.registerProgram("org.gnu.binutils.ar", Librarian);
        }

        {
            NativeLinkerOptions LOpts;
            LOpts.System.LinkLibraries.push_back("c++");
            //LOpts.System.LinkLibraries.push_back("c++fs");

            auto Linker = std::make_shared<GNULinker>();
            Linker->Type = LinkerType::GNU;
            Linker->file = "d:/dev/android/sdk/ndk-bundle/toolchains/llvm/prebuilt/windows-x86_64/bin/aarch64-linux-android28-clang.cmd";
            Linker->use_start_end_groups = false;
            *Linker = LOpts;
            s.registerProgram("org.LLVM.clang.ld", Linker);
        }

        NativeCompilerOptions COpts;

        // ASM
        /*{
            auto L = std::make_shared<NativeLanguage>();
            L->CompiledExtensions = { ".s", ".S" };

            auto C = std::make_shared<GNUASMCompiler>();
            C->Type = CompilerType::GNU;
            C->file = "x86_64-apple-darwin15-as";
            *C = COpts;
            L->compiler = C;
            s.registerProgramAndLanguage("org.gnu.gcc.as", C, L);
        }*/

        // C
        {
            auto L = std::make_shared<NativeLanguage>();
            L->CompiledExtensions = { ".c" };

            auto C = std::make_shared<GNUCompiler>();
            C->Type = CompilerType::GNU;
            C->file = "d:/dev/android/sdk/ndk-bundle/toolchains/llvm/prebuilt/windows-x86_64/bin/clang.exe";
            *C = COpts;
            L->compiler = C;
            s.registerProgramAndLanguage("org.LLVM.clang", C, L);

            auto cmd = C->createCommand();
            cmd->args.push_back("-fno-addrsig");
        }

        // CPP
        {
            auto L = std::make_shared<NativeLanguage>();
            L->CompiledExtensions = sw::getCppSourceFileExtensions();

            auto C = std::make_shared<GNUCompiler>();
            C->Type = CompilerType::GNU;
            C->file = "d:/dev/android/sdk/ndk-bundle/toolchains/llvm/prebuilt/windows-x86_64/bin/clang++.exe";
            *C = COpts;
            L->compiler = C;
            s.registerProgramAndLanguage("org.LLVM.clangpp", C, L);

            auto cmd = C->createCommand();
            cmd->args.push_back("-fno-addrsig");
            cmd->args.push_back("-stdlib=libc++");
        }
    }
}
