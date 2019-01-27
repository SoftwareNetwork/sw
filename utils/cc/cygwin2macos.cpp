void configure(Solution &b)
{
    auto &s = b.addCustomSolution();

    s.Settings.TargetOS.Type = OSType::Macos;
    s.Settings.Native.CompilerType = CompilerType::GNU;

    {
        {
            auto Librarian = std::make_shared<GNULibrarian>();
            Librarian->Type = LinkerType::GNU;
            Librarian->file = "x86_64-apple-darwin15-ar";
            Librarian->Extension = s.Settings.TargetOS.getStaticLibraryExtension();
            s.registerProgram("org.gnu.binutils.ar", Librarian);
        }

        {
            NativeLinkerOptions LOpts;
            LOpts.System.LinkLibraries.push_back("stdc++");
            LOpts.System.LinkLibraries.push_back("stdc++fs");

            auto Linker = std::make_shared<GNULinker>();
            Linker->Type = LinkerType::GNU;
            Linker->file = "o64-gcc";
            Linker->use_start_end_groups = false;
            *Linker = LOpts;
            s.registerProgram("org.gnu.gcc.ld", Linker);
        }

        NativeCompilerOptions COpts;

        // ASM
        {
            auto L = std::make_shared<NativeLanguage>();
            L->CompiledExtensions = { ".s", ".S" };

            auto C = std::make_shared<GNUASMCompiler>();
            C->Type = CompilerType::GNU;
            C->file = "x86_64-apple-darwin15-as";
            *C = COpts;
            L->compiler = C;
            s.registerProgramAndLanguage("org.gnu.gcc.as", C, L);
        }

        // C
        {
            auto L = std::make_shared<NativeLanguage>();
            L->CompiledExtensions = { ".c" };

            auto C = std::make_shared<GNUCompiler>();
            C->Type = CompilerType::GNU;
            C->file = "o64-gcc";
            *C = COpts;
            L->compiler = C;
            s.registerProgramAndLanguage("org.gnu.gcc.gcc", C, L);
        }

        // CPP
        {
            auto L = std::make_shared<NativeLanguage>();
            L->CompiledExtensions = getCppSourceFileExtensions();

            auto C = std::make_shared<GNUCompiler>();
            C->Type = CompilerType::GNU;
            C->file = "o64-g++";
            *C = COpts;
            L->compiler = C;
            s.registerProgramAndLanguage("org.gnu.gcc.gpp", C, L);
        }
    }
}
