#pragma sw require header pub.egorpugin.primitives.tools.embedder-master
#pragma sw require header org.sw.demo.google.grpc.grpc_cpp_plugin
#pragma sw require header org.sw.demo.lexxmark.winflexbison.bison-master

void configure(Build &s)
{
    //auto ss = s.createSettings();
    //ss.Native.LibrariesType = LibraryType::Static;
    //s.addSettings(ss);

#ifndef SW_CPP_DRIVER_API_VERSION
    s.registerCallback([](auto &t, auto cbt)
    {
        if (cbt != sw::CallbackType::CreateTarget)
            return;
        if (0
            || t.getPackage() == PackageId{ "pub.egorpugin.primitives.source-master" }
            || t.getPackage() == PackageId{ "pub.egorpugin.primitives.version-master" }
            || t.getPackage() == PackageId{ "pub.egorpugin.primitives.command-master" }
            || t.getPackage() == PackageId{ "pub.egorpugin.primitives.filesystem-master" }
            )
        {
            auto &nt = dynamic_cast<NativeExecutedTarget &>(t);
            nt.ExportIfStatic = true;
        }
    });

    if (s.isConfigSelected("cygwin2macos"))
        s.loadModule("utils/cc/cygwin2macos.cpp").call<void(Solution&)>("configure", s);
    else if (s.isConfigSelected("win2macos"))
        s.loadModule("utils/cc/win2macos.cpp").call<void(Solution&)>("configure", s);
    else if (s.isConfigSelected("win2android"))
        s.loadModule("utils/cc/win2android.cpp").call<void(Solution&)>("configure", s);
#endif

    //s.getSettings().Native.ConfigurationType = ConfigurationType::ReleaseWithDebugInformation;
    //s.getSettings().Native.CompilerType = CompilerType::ClangCl;
    //s.getSettings().Native.CompilerType = CompilerType::Clang;
}

void build(Solution &s)
{
    auto &p = s.addProject("sw.client", "0.3.1");
    p += Git("https://github.com/SoftwareNetwork/sw", "", "master");

    auto &support = p.addTarget<StaticLibraryTarget>("support");
    {
        support.CPPVersion = CPPLanguageStandard::CPP17;
        support += "src/sw/support/.*"_rr;
        support.Public +=
            "pub.egorpugin.primitives.http-master"_dep,
            "pub.egorpugin.primitives.hash-master"_dep,
            "pub.egorpugin.primitives.command-master"_dep,
            "pub.egorpugin.primitives.log-master"_dep,
            "pub.egorpugin.primitives.executor-master"_dep,
            "pub.egorpugin.primitives.symbol-master"_dep,
            "org.sw.demo.boost.property_tree"_dep,
            "org.sw.demo.boost.stacktrace"_dep;
        support.ApiName = "SW_SUPPORT_API";
        if (support.getSettings().TargetOS.Type == OSType::Windows)
        {
            support.Protected += "_CRT_SECURE_NO_WARNINGS"_d;
            support.Public += "UNICODE"_d;
        }
        if (support.getSettings().TargetOS.Type == OSType::Macos)
            support.Public += "BOOST_STACKTRACE_GNU_SOURCE_NOT_REQUIRED"_def;
    }

    auto &protos = p.addTarget<StaticLibraryTarget>("protos");
    {
        protos.CPPVersion = CPPLanguageStandard::CPP17;
        protos += "src/sw/protocol/.*"_rr;
        protos.Public +=
            "org.sw.demo.google.grpc.grpcpp"_dep,
            "pub.egorpugin.primitives.templates-master"_dep,
            "pub.egorpugin.primitives.log-master"_dep;
        for (auto &[p, _] : protos["src/sw/protocol/.*\\.proto"_rr])
            gen_grpc("org.sw.demo.google.protobuf-3"_dep, "org.sw.demo.google.grpc.grpc_cpp_plugin"_dep, protos, p, true);
    }

    auto &manager = p.addTarget<LibraryTarget>("manager");
    {
        manager.ApiName = "SW_MANAGER_API";
        manager.ExportIfStatic = true;
        manager.CPPVersion = CPPLanguageStandard::CPP17;
        manager.Public += "BOOST_DLL_USE_STD_FS"_def;

        manager.Public += support, protos,
            "pub.egorpugin.primitives.date_time-master"_dep,
            "pub.egorpugin.primitives.db.sqlite3-master"_dep,
            "pub.egorpugin.primitives.lock-master"_dep,
            "pub.egorpugin.primitives.pack-master"_dep,
            "pub.egorpugin.primitives.source-master"_dep,
            "pub.egorpugin.primitives.sw.settings-master"_dep,
            "pub.egorpugin.primitives.version-master"_dep,
            "pub.egorpugin.primitives.yaml-master"_dep,
            "org.sw.demo.nlohmann.json-3"_dep,
            "org.sw.demo.boost.variant"_dep,
            "org.sw.demo.boost.dll"_dep,
            "org.sw.demo.rbock.sqlpp11_connector_sqlite3-develop"_dep
            ;

        manager.Public -= "pub.egorpugin.primitives.win32helpers-master"_dep;
        if (manager.getSettings().TargetOS.Type == OSType::Windows)
            manager.Public += "pub.egorpugin.primitives.win32helpers-master"_dep;

        manager += "src/sw/manager/.*"_rr;
        manager.Public.Definitions["VERSION_MAJOR"] += std::to_string(manager.getPackage().version.getMajor());
        manager.Public.Definitions["VERSION_MINOR"] += std::to_string(manager.getPackage().version.getMinor());
        manager.Public.Definitions["VERSION_PATCH"] += std::to_string(manager.getPackage().version.getPatch());
        embed("pub.egorpugin.primitives.tools.embedder-master"_dep, manager, "src/sw/manager/inserts/inserts.cpp.in");
        gen_sqlite2cpp("pub.egorpugin.primitives.tools.sqlpp11.sqlite2cpp-master"_dep,
            manager, manager.SourceDir / "src/sw/manager/inserts/packages_db_schema.sql", "db_packages.h", "db::packages");

        /*PrecompiledHeader pch;
        if (!s.Variables["SW_SELF_BUILD"])
        {
            pch.header = manager.SourceDir / "src/sw/manager/pch.h";
            pch.force_include_pch = true;
            //manager.addPrecompiledHeader(pch);
        }*/
    }

    auto &tools = p.addDirectory("tools");
    auto &self_builder = tools.addTarget<ExecutableTarget>("self_builder");
    self_builder.PackageDefinitions = true;
    self_builder.CPPVersion = CPPLanguageStandard::CPP17;
    self_builder += "src/sw/tools/self_builder.cpp";
    self_builder +=
        manager,
        "pub.egorpugin.primitives.emitter-master"_dep,
        "pub.egorpugin.primitives.sw.main-master"_dep;

    auto &cl_generator = tools.addTarget<ExecutableTarget>("cl_generator");
    cl_generator.PackageDefinitions = true;
    cl_generator.CPPVersion = CPPLanguageStandard::CPP17;
    cl_generator += "src/sw/tools/cl_generator.*"_rr;
    cl_generator +=
        "pub.egorpugin.primitives.emitter-master"_dep,
        "pub.egorpugin.primitives.yaml-master"_dep,
        "pub.egorpugin.primitives.main-master"_dep;

    auto &create_git_rev = tools.addTarget<ExecutableTarget>("create_git_rev");
    create_git_rev.CPPVersion = CPPLanguageStandard::CPP17;
    create_git_rev += "src/sw/tools/create_git_rev.*"_rr;
    create_git_rev +=
        "pub.egorpugin.primitives.command-master"_dep,
        "pub.egorpugin.primitives.sw.main-master"_dep;

    auto &builder = p.addTarget<LibraryTarget>("builder");
    {
        builder.ApiName = "SW_BUILDER_API";
        builder.ExportIfStatic = true;
        builder.CPPVersion = CPPLanguageStandard::CPP17;
        builder += "src/sw/builder/.*"_rr;
        builder.Public += manager, "org.sw.demo.preshing.junction-master"_dep,
            "org.sw.demo.boost.graph"_dep,
            "org.sw.demo.boost.serialization"_dep,
            "org.sw.demo.microsoft.gsl-*"_dep,
            "pub.egorpugin.primitives.emitter-master"_dep;
        //if (!s.Variables["SW_SELF_BUILD"])
        {
            /*PrecompiledHeader pch;
            pch.header = "src/sw/builder/pch.h";
            pch.force_include_pch = true;*/
            //builder.addPrecompiledHeader(pch);
        }
    }

    auto &core = p.addTarget<LibraryTarget>("core");
    {
        core.ApiName = "SW_CORE_API";
        core.ExportIfStatic = true;
        core.CPPVersion = CPPLanguageStandard::CPP17;
        core.Public += builder;
        core += "src/sw/core/.*"_rr;
        if (core.getSettings().TargetOS.Type == OSType::Windows)
            core += "OleAut32.lib"_slib;
    }

    auto &cpp_driver = p.addTarget<LibraryTarget>("driver.cpp");
    {
        cpp_driver.ApiName = "SW_DRIVER_CPP_API";
        cpp_driver.ExportIfStatic = true;
        cpp_driver.CPPVersion = CPPLanguageStandard::CPP17;
        cpp_driver.Public += core,
            "pub.egorpugin.primitives.patch-master"_dep,
            "org.sw.demo.boost.assign"_dep,
            "org.sw.demo.boost.bimap"_dep,
            "org.sw.demo.boost.uuid"_dep;
        cpp_driver += "src/sw/driver/.*"_rr;
        cpp_driver -= "src/sw/driver/inserts/.*"_rr;
        embed("pub.egorpugin.primitives.tools.embedder-master"_dep, cpp_driver, "src/sw/driver/inserts/inserts.cpp.in");
        gen_flex_bison("org.sw.demo.lexxmark.winflexbison-master"_dep, cpp_driver, "src/sw/driver/bazel/lexer.ll", "src/sw/driver/bazel/grammar.yy");
        if (cpp_driver.getCompilerType() == CompilerType::MSVC)
            cpp_driver.CompileOptions.push_back("-bigobj");
        if (cpp_driver.getSettings().TargetOS.Type == OSType::Windows)
            cpp_driver += "dbghelp.lib"_slib;
        //else if (s.getSettings().Native.CompilerType == CompilerType::GNU)
            //cpp_driver.CompileOptions.push_back("-Wa,-mbig-obj");
        {
            auto c = cpp_driver.addCommand();
            c << cmd::prog(self_builder)
                << cmd::out("build_self.generated.h")
                << cmd::out("build_self.packages.generated.h")
                ;
        }
        {
            auto c = cpp_driver.addCommand();
            c << cmd::prog(cl_generator)
                << cmd::in("src/sw/driver/options_cl.yml")
                << cmd::out("options_cl.generated.h")
                << cmd::out("options_cl.generated.cpp", cmd::Skip)
                ;
            c.c->ignore_deps_generated_commands = true;
            // make sure this is exported header, so we depend on it
            cpp_driver.Public += "options_cl.generated.h";
        }
        //if (!s.Variables["SW_SELF_BUILD"])
        {
            /*PrecompiledHeader pch;
            pch.header = "src/sw/driver/pch.h";
            pch.force_include_pch = true;*/
            //cpp_driver.addPrecompiledHeader(pch);
        }
    }

    auto &client = p.addTarget<ExecutableTarget>("sw");
    {
        client.PackageDefinitions = true;
        client.SwDefinitions = true;
        client.StartupProject = true;
        client += "src/sw/client/.*"_rr;
        client.CPPVersion = CPPLanguageStandard::CPP17;
        client += core, cpp_driver,
            //"org.sw.demo.microsoft.mimalloc"_dep,
            "pub.egorpugin.primitives.sw.main-master"_dep,
            "org.sw.demo.giovannidicanio.winreg-master"_dep;
        embed("pub.egorpugin.primitives.tools.embedder-master"_dep, client, "src/sw/client/inserts/inserts.cpp.in");
        if (client.getCompilerType() == CompilerType::MSVC)
            client.CompileOptions.push_back("-bigobj");
        if (client.getSettings().TargetOS.Type == OSType::Linux)
        {
            //client.getSelectedTool()->LinkOptions.push_back("-static-libstdc++");
            //client.getSelectedTool()->LinkOptions.push_back("-static-libgcc");
        }

        {
            auto c = client.addCommand();
            c << cmd::prog(create_git_rev)
                << sw::resolveExecutable("git")
                << client.SourceDir
                << cmd::out("gitrev.h");
            c.c->always = true;
        }

        if (client.getSettings().TargetOS.Type == OSType::Windows)
        {
            auto &client = tools.addTarget<ExecutableTarget>("client");
            client += "src/sw/tools/client.cpp";
            client +=
                "org.sw.demo.boost.dll"_dep,
                "org.sw.demo.boost.filesystem"_dep,
                "user32.lib"_slib;
            if (client.getSettings().TargetOS.Type == OSType::Windows)
                client.Public += "UNICODE"_d;
        }
    }
}
