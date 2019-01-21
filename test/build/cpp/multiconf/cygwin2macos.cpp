void configure(Solution &b)
{
    auto &s = b.addSolution();
    s.prepareForCustomToolchain();

    s.Settings.TargetOS.Type = OSType::Macos;
    s.Settings.Native.CompilerType = CompilerType::Clang;

    {
        {
            auto Librarian = std::make_shared<GNULibrarian>();
            Librarian->Type = LinkerType::GNU;
            Librarian->file = "/cygdrive/d/dev/cppan2/client/1/cc/macos/osxcross/target/bin/x86_64-apple-darwin15-ar";
            Librarian->Extension = s.Settings.TargetOS.getStaticLibraryExtension();
            s.registerProgram("org.gnu.binutils.ar", Librarian);
        }

        {
            auto Linker = std::make_shared<GNULinker>();

            Linker->rdynamic = false;
            Linker->PositionIndependentCode = false;
            Linker->Type = LinkerType::GNU;
            Linker->file = "/cygdrive/d/dev/cppan2/client/1/cc/macos/osxcross/target/bin/x86_64-apple-darwin15-ld";
            //*Linker = LOpts;
            s.registerProgram("org.LLVM.clang.ld", Linker);

            // tune
            auto c = Linker->createCommand();
            c->args.push_back("-syslibroot");
            c->args.push_back("/cygdrive/d/dev/cppan2/client/1/cc/macos/osxcross/target/SDK/MacOSX10.11.sdk");
            c->args.push_back("-lcrt1.10.5.o");
            c->args.push_back("-lstdc++");
            c->args.push_back("-lSystem");
            c->args.push_back("-lgcc_s.10.5");
        }

        NativeCompilerOptions COpts;

        // C
        {
            auto L = std::make_shared<NativeLanguage>();
            L->CompiledExtensions = { ".c" };

            auto C = std::make_shared<GNUCompiler>();
            C->Type = CompilerType::Clang;
            C->file = "/cygdrive/d/dev/cppan2/client/1/cc/macos/osxcross/target/bin/o64-clang";
            *C = COpts;
            L->compiler = C;
            s.registerProgramAndLanguage("org.LLVM.clang", C, L);
        }

        // CPP
        {
            auto L = std::make_shared<NativeLanguage>();
            L->CompiledExtensions = getCppSourceFileExtensions();

            auto C = std::make_shared<GNUCompiler>();
            C->Type = CompilerType::Clang;
            C->file = "/cygdrive/d/dev/cppan2/client/1/cc/macos/osxcross/target/bin/o64-clang++";
            *C = COpts;
            L->compiler = C;
            s.registerProgramAndLanguage("org.LLVM.clangpp", C, L);
        }
    }
}

void build(Solution &s)
{
    auto &t1 = s.add<Executable>("test");
    t1 += "src/main.cpp";
    //t1 += "org.sw.demo.boost.log-develop"_dep;

    auto &t2 = s.addExecutable("test2");
    t2 += "src/main2.cpp";

    auto &t3 = s.addExecutable("test3");
    t3.CPPVersion = CPPLanguageStandard::CPP11;
    t3 += "src/main3.cpp";

    auto &t4 = s.addExecutable("test4");
    {
        auto c = t4.addCommand();
        c << cmd::prog(t3)
            << cmd::std_out("main4.cpp");
    }

    //auto &t3 = s.addExecutable("test3");
    //t3 += "src/a/main3.cpp";
}
