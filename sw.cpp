#pragma sw require header pub.egorpugin.primitives.tools.embedder-master
#pragma sw require header org.sw.demo.google.grpc.cpp.plugin
#pragma sw require header org.sw.demo.lexxmark.winflexbison.bison
#pragma sw require header org.sw.demo.qtproject.qt.base.tools.moc

void build(Solution &s)
{
    auto &p = s.addProject("sw.client", "0.4.0");
    p += Git("https://github.com/SoftwareNetwork/sw", "", "master");

    auto &support = p.addTarget<StaticLibraryTarget>("support");
    {
        support.CPPVersion = CPPLanguageStandard::CPP17;
        support += "src/sw/support/.*"_rr;
        auto cmddep = "pub.egorpugin.primitives.command-master"_dep;
        support.Public +=
            cmddep,
            "pub.egorpugin.primitives.http-master"_dep,
            "pub.egorpugin.primitives.hash-master"_dep,
            "pub.egorpugin.primitives.log-master"_dep,
            "pub.egorpugin.primitives.executor-master"_dep,
            "pub.egorpugin.primitives.symbol-master"_dep,
            "org.sw.demo.boost.property_tree"_dep,
            "org.sw.demo.boost.stacktrace"_dep;
        //cmddep->getSettings()["export-if-static"] = "true";
        //cmddep->getSettings()["export-if-static"].setRequired();
        support.ApiName = "SW_SUPPORT_API";
        if (support.getBuildSettings().TargetOS.Type == OSType::Windows)
        {
            support.Protected += "_CRT_SECURE_NO_WARNINGS"_d;
            support.Public += "UNICODE"_d;
        }
        if (support.getCompilerType() != CompilerType::MSVC)
        {
            support.Protected.CompileOptions.push_back("-Wall");
            support.Protected.CompileOptions.push_back("-Wextra");
        }
    }

    auto &protos = p.addTarget<StaticLibraryTarget>("protos");
    {
        protos.CPPVersion = CPPLanguageStandard::CPP17;
        protos += "src/sw/protocol/.*"_rr;
        protos.Public += "pub.egorpugin.primitives.grpc_helpers-master"_dep;
        ProtobufData d;
        d.public_protobuf = true;
        d.addIncludeDirectory(protos.SourceDir / "src");
        for (auto &[p, _] : protos["src/sw/protocol/.*\\.proto"_rr])
            gen_grpc_cpp("org.sw.demo.google.protobuf"_dep, "org.sw.demo.google.grpc.cpp.plugin"_dep, protos, p, d);
    }

    auto &manager = p.addTarget<LibraryTarget>("manager");
    {
        manager.ApiName = "SW_MANAGER_API";
        manager.ExportIfStatic = true;
        manager.CPPVersion = CPPLanguageStandard::CPP17;
        manager.Public += "BOOST_DLL_USE_STD_FS"_def;

        auto srcdep = "pub.egorpugin.primitives.source-master"_dep;
        auto verdep = "pub.egorpugin.primitives.version-master"_dep;
        manager.Public += support, protos,
            srcdep, verdep,
            "pub.egorpugin.primitives.date_time-master"_dep,
            "pub.egorpugin.primitives.db.sqlite3-master"_dep,
            "pub.egorpugin.primitives.lock-master"_dep,
            "pub.egorpugin.primitives.pack-master"_dep,
            "pub.egorpugin.primitives.sw.settings-master"_dep,
            "pub.egorpugin.primitives.yaml-master"_dep,
            "org.sw.demo.nlohmann.json-3"_dep,
            "org.sw.demo.boost.variant"_dep,
            "org.sw.demo.boost.dll"_dep,
            "org.sw.demo.rbock.sqlpp11_connector_sqlite3-develop"_dep
            ;
        srcdep->getSettings()["export-if-static"] = "true";
        srcdep->getSettings()["export-if-static"].setRequired();
        verdep->getSettings()["export-if-static"] = "true";
        verdep->getSettings()["export-if-static"].setRequired();

        manager.Public -= "pub.egorpugin.primitives.win32helpers-master"_dep;
        if (manager.getBuildSettings().TargetOS.Type == OSType::Windows)
            manager.Public += "pub.egorpugin.primitives.win32helpers-master"_dep;

        manager += "src/sw/manager/.*"_rr;
        manager.Public.Definitions["VERSION_MAJOR"] += std::to_string(manager.getPackage().getVersion().getMajor());
        manager.Public.Definitions["VERSION_MINOR"] += std::to_string(manager.getPackage().getVersion().getMinor());
        manager.Public.Definitions["VERSION_PATCH"] += std::to_string(manager.getPackage().getVersion().getPatch());
        embed2("pub.egorpugin.primitives.tools.embedder2-master"_dep, manager, "src/sw/manager/inserts/packages_db_schema.sql");
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

    auto &builder = p.addTarget<LibraryTarget>("builder");
    {
        builder.ApiName = "SW_BUILDER_API";
        builder.ExportIfStatic = true;
        builder.CPPVersion = CPPLanguageStandard::CPP17;
        builder += "src/sw/builder/.*"_rr;
        builder.Public += manager,
            "org.sw.demo.preshing.junction-master"_dep,
            "org.sw.demo.boost.graph"_dep,
            "org.sw.demo.boost.serialization"_dep,
            "org.sw.demo.microsoft.gsl"_dep,
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
        if (core.getBuildSettings().TargetOS.Type == OSType::Windows)
            core += "OleAut32.lib"_slib;
        core += "org.sw.demo.Neargye.magic_enum"_dep;
    }

    auto &cpp_driver = p.addTarget<LibraryTarget>("driver.cpp");
    {
        cpp_driver.ApiName = "SW_DRIVER_CPP_API";
        cpp_driver.ExportIfStatic = true;
        cpp_driver.PackageDefinitions = true;
        cpp_driver.CPPVersion = CPPLanguageStandard::CPP17;
        cpp_driver.Public += core,
            "pub.egorpugin.primitives.patch-master"_dep,
            "org.sw.demo.ToruNiina.toml11"_dep,
            "org.sw.demo.giovannidicanio.winreg"_dep,
            "org.sw.demo.boost.assign"_dep,
            "org.sw.demo.boost.bimap"_dep,
            "org.sw.demo.boost.uuid"_dep;
        cpp_driver += "src/sw/driver/.*"_rr;
        cpp_driver -= "src/sw/driver/misc/delay_load_helper.cpp";
        gen_flex_bison("org.sw.demo.lexxmark.winflexbison"_dep, cpp_driver, "src/sw/driver/bazel/lexer.ll", "src/sw/driver/bazel/grammar.yy");
        if (cpp_driver.getCompilerType() == CompilerType::MSVC)
            cpp_driver.CompileOptions.push_back("-bigobj");
        if (cpp_driver.getBuildSettings().TargetOS.Type == OSType::Windows)
            cpp_driver += "dbghelp.lib"_slib;
        //else if (s.getBuildSettings().Native.CompilerType == CompilerType::GNU)
            //cpp_driver.CompileOptions.push_back("-Wa,-mbig-obj");
        {
            auto &self_builder = cpp_driver.addTarget<ExecutableTarget>("self_builder");
            self_builder.PackageDefinitions = true;
            self_builder.CPPVersion = CPPLanguageStandard::CPP17;
            self_builder += "src/sw/tools/self_builder.cpp";
            self_builder +=
                manager,
                "pub.egorpugin.primitives.emitter-master"_dep,
                "pub.egorpugin.primitives.sw.main-master"_dep;

            auto c = cpp_driver.addCommand();
            c << cmd::prog(self_builder)
                << cmd::out("build_self.generated.h")
                << cmd::out("build_self.packages.generated.h")
                ;
        }
        {
            auto &cl_generator = cpp_driver.addTarget<ExecutableTarget>("cl_generator");
            cl_generator.PackageDefinitions = true;
            cl_generator.CPPVersion = CPPLanguageStandard::CPP17;
            cl_generator += "src/sw/tools/cl_generator.*"_rr;
            cl_generator +=
                "pub.egorpugin.primitives.emitter-master"_dep,
                "pub.egorpugin.primitives.yaml-master"_dep,
                "pub.egorpugin.primitives.main-master"_dep;

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

        embed2("pub.egorpugin.primitives.tools.embedder2-master"_dep, cpp_driver, "src/sw/driver/sw1.h");
        embed2("pub.egorpugin.primitives.tools.embedder2-master"_dep, cpp_driver, "src/sw/driver/sw_check_abi_version.h");
        embed2("pub.egorpugin.primitives.tools.embedder2-master"_dep, cpp_driver, "src/sw/driver/misc/delay_load_helper.cpp");

        // preprocess sw.h
        /*if (!cpp_driver.DryRun)
        {
            auto pp = cpp_driver.BinaryPrivateDir / "sw.pp";

            auto f = cpp_driver["src/sw/driver/misc/sw.cpp"].as<NativeSourceFile *>();
            if (!f)
                throw SW_RUNTIME_ERROR("cannot cast to NativeSourceFile");

            // for cmdline build
            f->skip_linking = true;
            f->fancy_name = "[preprocess forced include header]";

            if (auto c = f->compiler->as<VisualStudioCompiler *>())
            {
                c->PreprocessToFile = true;
                c->PreprocessSupressLineDirectives = true;
                c->PreprocessFileName = pp;
                File(pp, cpp_driver.getFs()).setGenerated(true);
            }
            else
                SW_UNIMPLEMENTED;

            cpp_driver += pp; // mark as generated (to add into vs solution)
            embed2("pub.egorpugin.primitives.tools.embedder2-master"_dep, cpp_driver, pp, cpp_driver.BinaryPrivateDir / "sw.pp.emb", 1);

            // exclude
            cpp_driver.add(sw::CallbackType::EndPrepare, [&cpp_driver, f]()
            {
                cpp_driver -= "src/sw/driver/misc/sw.cpp";
                auto c = f->compiler->getCommand(cpp_driver);
                // for ide build
                c->inputs.erase(cpp_driver.SourceDir / "src/sw/driver/misc/sw.cpp");
            });
        }*/
    }

    auto &client = p.addTarget<ExecutableTarget>("sw", "1.0.0");
    auto &client_common = client.addTarget<StaticLibrary>("common");
    {
        client_common.PackageDefinitions = true;
        client_common.SwDefinitions = true;
        client_common.StartupProject = true;
        client_common += "src/sw/client/common/.*"_rr;
        client_common.CPPVersion = CPPLanguageStandard::CPP17;
        client_common.Public += core, cpp_driver;
        client_common.Public += "org.sw.demo.giovannidicanio.winreg"_dep;

        embed2("pub.egorpugin.primitives.tools.embedder2-master"_dep, client_common, "src/sw/client/common/inserts/SWConfig.cmake");
        embed2("pub.egorpugin.primitives.tools.embedder2-master"_dep, client_common, "src/sw/client/common/inserts/project_templates.yml");

        generate_cl("pub.egorpugin.primitives.tools.cl_generator-master"_dep, client_common,
            "src/sw/client/common/cl.yml", "llvm");
    }

    // client
    {
        client.PackageDefinitions = true;
        client.SwDefinitions = true;
        client.StartupProject = true;
        client += "src/sw/client/cli/.*"_rr;
        client.CPPVersion = CPPLanguageStandard::CPP17;
        client += client_common,
            //"org.sw.demo.microsoft.mimalloc"_dep,
            "pub.egorpugin.primitives.sw.main-master"_dep
            ;
        if (client.getCompilerType() == CompilerType::MSVC)
            client.CompileOptions.push_back("-bigobj");
        if (client.getBuildSettings().TargetOS.Type != OSType::Windows)
        {
            //client.getSelectedTool()->LinkOptions.push_back("-static-libstdc++");
            //client.getSelectedTool()->LinkOptions.push_back("-static-libgcc");

            // needed to export all shared symbols
            // so dlopen will work for plugins
            //client.LinkOptions.push_back("-Wl,--export-dynamic");
            //client.LinkOptions.push_back("-Wl,-export-dynamic");
            client.LinkOptions.push_back("-rdynamic");
        }

        if (client.getCompilerType() == CompilerType::MSVC)
            client.CompileOptions.push_back("-wd4275");

        {
            auto &create_git_rev = client.addTarget<ExecutableTarget>("create_git_rev");
            create_git_rev.CPPVersion = CPPLanguageStandard::CPP17;
            create_git_rev += "src/sw/tools/create_git_rev.*"_rr;
            create_git_rev +=
                "pub.egorpugin.primitives.command-master"_dep,
                "pub.egorpugin.primitives.sw.main-master"_dep;

            auto c = client.addCommand();
            c << cmd::prog(create_git_rev)
                << sw::resolveExecutable("git")
                << client.SourceDir
                << cmd::out("gitrev.h");
            c.c->always = true;
        }
    }

    if (s.getExternalVariables()["with-gui"] != "true")
        return;

#ifndef SW_DRIVER_ADD_SELF
    auto &gui = client.addTarget<ExecutableTarget>("gui", "0.4.0");
    {
        gui.PackageDefinitions = true;
        gui.SwDefinitions = true;
        gui += "src/sw/client/gui/.*"_rr;
        gui.CPPVersion = CPPLanguageStandard::CPP17;
        gui += client_common;

        gui += "org.sw.demo.qtproject.qt.base.widgets"_dep;
        gui += "org.sw.demo.qtproject.qt.base.winmain"_dep;
        gui += "org.sw.demo.qtproject.qt.base.plugins.platforms.windows"_dep;
        gui += "org.sw.demo.qtproject.qt.base.plugins.styles.windowsvista"_dep;

        gui -= "org.sw.demo.qtproject.qt.winextras"_dep;
        if (client.getBuildSettings().TargetOS.Type == OSType::Windows)
            gui += "org.sw.demo.qtproject.qt.winextras"_dep;

        if (auto L = gui.getSelectedTool()->as<VisualStudioLinker*>(); L)
            L->Subsystem = vs::Subsystem::Windows;

        qt_moc_rcc_uic("org.sw.demo.qtproject.qt"_dep, gui);
        qt_tr("org.sw.demo.qtproject.qt"_dep, gui);
    }
#endif
}
