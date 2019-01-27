void configure(Solution &b)
{
    auto &s = b.addCustomSolution();

    s.Settings.TargetOS.Type = OSType::Macos;
    s.Settings.Native.CompilerType = CompilerType::Clang;

    {
        {
            auto Librarian = std::make_shared<GNULibrarian>();
            Librarian->Type = LinkerType::GNU;
            Librarian->file = "llvm-ar";
            Librarian->Extension = s.Settings.TargetOS.getStaticLibraryExtension();
            s.registerProgram("org.gnu.binutils.ar", Librarian);
        }

        {
            NativeLinkerOptions LOpts;
            LOpts.System.LinkLibraries.push_back("c++");
            LOpts.System.LinkLibraries.push_back("c++fs");

            auto Linker = std::make_shared<GNULinker>();
            Linker->Type = LinkerType::GNU;
            Linker->file = "clang";
            Linker->use_start_end_groups = false;
            *Linker = LOpts;
            s.registerProgram("org.LLVM.clang.ld", Linker);

            auto cmd = Linker->createCommand();
            //cmd->args.push_back("-flavor ld64.lld");
            //cmd->args.push_back("-sdk_version 10.14");
            cmd->args.push_back("-target");
            cmd->args.push_back("x86_64-apple-macosx10.14.0");
            cmd->args.push_back("-isysroot");
            cmd->args.push_back("d:/dev/cygwin64/home/egorp/osxcross/target/SDK/MacOSX10.14.sdk");
            //cmd->args.push_back("-cxx-isystem");
            //cmd->args.push_back("d:/dev/cygwin64/home/egorp/osxcross/target/SDK/MacOSX10.14.sdk/usr/include/c++");
            cmd->args.push_back("-Wl,-sdk_version");
            cmd->args.push_back("-Wl,10.14");
            cmd->args.push_back("-fuse-ld=lld");
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
            C->file = "clang";
            *C = COpts;
            L->compiler = C;
            s.registerProgramAndLanguage("org.LLVM.clang", C, L);

            auto cmd = C->createCommand();
            cmd->args.push_back("-target");
            cmd->args.push_back("x86_64-apple-macosx10.14.0");
            cmd->args.push_back("-isysroot");
            cmd->args.push_back("d:/dev/cygwin64/home/egorp/osxcross/target/SDK/MacOSX10.14.sdk");
        }

        // CPP
        {
            auto L = std::make_shared<NativeLanguage>();
            L->CompiledExtensions = getCppSourceFileExtensions();

            auto C = std::make_shared<GNUCompiler>();
            C->Type = CompilerType::GNU;
            C->file = "clang++";
            *C = COpts;
            L->compiler = C;
            s.registerProgramAndLanguage("org.LLVM.clangpp", C, L);

            auto cmd = C->createCommand();
            cmd->args.push_back("-target");
            cmd->args.push_back("x86_64-apple-macosx10.14.0");
            cmd->args.push_back("-isysroot");
            cmd->args.push_back("d:/dev/cygwin64/home/egorp/osxcross/target/SDK/MacOSX10.14.sdk");
            cmd->args.push_back("-cxx-isystem");
            cmd->args.push_back("d:/dev/cygwin64/home/egorp/osxcross/target/SDK/c++/v1");
        }
    }
}
