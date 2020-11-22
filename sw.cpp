#pragma sw require header pub.egorpugin.primitives.tools.embedder
#pragma sw require header org.sw.demo.google.grpc.cpp.plugin
#pragma sw require header org.sw.demo.lexxmark.winflexbison.bison
//#pragma sw require header org.sw.demo.qtproject.qt.base.tools.moc

void build(Solution &s)
{
    auto &sw = s.addProject("sw", "0.4.3");
    sw += Git("https://github.com/SoftwareNetwork/sw", "", "master");

    auto &p = sw.addProject("client");

    auto &support = p.addTarget<LibraryTarget>("support");
    {
        support.ApiName = "SW_SUPPORT_API";
        support.ExportIfStatic = true;
        support += cpp20;
        support += "src/sw/support/.*"_rr;
        auto cmddep = "pub.egorpugin.primitives.command"_dep;
        auto verdep = "pub.egorpugin.primitives.version"_dep;
        auto srcdep = "pub.egorpugin.primitives.source"_dep;
        support.Public +=
            cmddep, verdep, srcdep,
            "pub.egorpugin.primitives.date_time"_dep,
            "pub.egorpugin.primitives.http"_dep,
            "pub.egorpugin.primitives.hash"_dep,
            "pub.egorpugin.primitives.log"_dep,
            "pub.egorpugin.primitives.executor"_dep,
            "pub.egorpugin.primitives.symbol"_dep,
            "org.sw.demo.boost.property_tree"_dep,
            "org.sw.demo.boost.serialization"_dep,
            "org.sw.demo.boost.stacktrace"_dep;
        //cmddep->getSettings()["export-if-static"] = "true";
        //cmddep->getSettings()["export-if-static"].setRequired();
        verdep->getSettings()["export-if-static"] = "true";
        verdep->getSettings()["export-if-static"].setRequired();
        srcdep->getSettings()["export-if-static"] = "true";
        srcdep->getSettings()["export-if-static"].setRequired();
        if (support.getBuildSettings().TargetOS.Type == OSType::Windows)
        {
            support.Protected += "_CRT_SECURE_NO_WARNINGS"_d;
            support.Public += "UNICODE"_d;
        }
        if (support.getCompilerType() != CompilerType::MSVC &&
            support.getCompilerType() != CompilerType::ClangCl)
        {
            support.Protected.CompileOptions.push_back("-Wall");
            support.Protected.CompileOptions.push_back("-Wextra");
        }
        if (support.getCompilerType() == CompilerType::ClangCl)
        {
            support.Protected.CompileOptions.push_back("-Wno-macro-redefined");
            support.Protected.CompileOptions.push_back("-Wno-microsoft-template");
            support.Protected.CompileOptions.push_back("-Wno-deprecated-declarations"); // maybe use STL define instead?
            support.Protected.CompileOptions.push_back("-Wno-assume");
        }
        if (support.getCompilerType() == CompilerType::MSVC)
            support.Protected.CompileOptions.push_back("-wd4275");
    }

    auto &protos = p.addTarget<StaticLibraryTarget>("protos");
    {
        protos += cpp20;
        protos += "src/sw/protocol/.*"_rr;
        protos.Public += "pub.egorpugin.primitives.grpc_helpers"_dep;
        ProtobufData d;
        d.public_protobuf = true;
        d.addIncludeDirectory(protos.SourceDir / "src");
        for (auto &[p, _] : protos["src/sw/protocol/.*\\.proto"_rr])
            gen_grpc_cpp("org.sw.demo.google.protobuf"_dep, "org.sw.demo.google.grpc.cpp.plugin"_dep, protos, p, d);
        if (protos.getCompilerType() == CompilerType::MSVC)
            protos.Protected += "_SILENCE_CXX20_IS_POD_DEPRECATION_WARNING"_def;
    }

    auto &manager = p.addTarget<LibraryTarget>("manager");
    {
        manager.ApiName = "SW_MANAGER_API";
        manager.ExportIfStatic = true;
        manager += cpp20;
        manager.Public += "BOOST_DLL_USE_STD_FS"_def;

        manager +=
            "pub.egorpugin.primitives.csv"_dep;
        manager.Public += support, protos,
            "pub.egorpugin.primitives.db.sqlite3"_dep,
            "pub.egorpugin.primitives.lock"_dep,
            "pub.egorpugin.primitives.pack"_dep,
            "pub.egorpugin.primitives.sw.settings"_dep,
            "pub.egorpugin.primitives.yaml"_dep,
            "org.sw.demo.nlohmann.json"_dep,
            "org.sw.demo.boost.variant"_dep,
            "org.sw.demo.boost.dll"_dep,
            "org.sw.demo.rbock.sqlpp11_connector_sqlite3"_dep
            ;

        manager.Public -= "pub.egorpugin.primitives.win32helpers"_dep;
        if (manager.getBuildSettings().TargetOS.Type == OSType::Windows)
            manager.Public += "pub.egorpugin.primitives.win32helpers"_dep;

        manager += "src/sw/manager/.*"_rr;
        manager.Public += "src/sw/manager/manager.natvis";
        manager.Public.Definitions["VERSION_MAJOR"] += std::to_string(manager.getPackage().getVersion().getMajor());
        manager.Public.Definitions["VERSION_MINOR"] += std::to_string(manager.getPackage().getVersion().getMinor());
        manager.Public.Definitions["VERSION_PATCH"] += std::to_string(manager.getPackage().getVersion().getPatch());
        embed2("pub.egorpugin.primitives.tools.embedder2"_dep, manager, "src/sw/manager/inserts/packages_db_schema.sql");
        gen_sqlite2cpp("pub.egorpugin.primitives.tools.sqlpp11.sqlite2cpp"_dep,
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
        builder += cpp20;
        builder += "src/sw/builder/.*"_rr;
        builder.Public += manager,
            "org.sw.demo.preshing.junction-master"_dep,
            "org.sw.demo.boost.graph"_dep,
            "org.sw.demo.microsoft.gsl"_dep,
            "pub.egorpugin.primitives.emitter"_dep;
        //if (!s.Variables["SW_SELF_BUILD"])
        {
            /*PrecompiledHeader pch;
            pch.header = "src/sw/builder/pch.h";
            pch.force_include_pch = true;*/
            //builder.addPrecompiledHeader(pch);
        }
    }

    auto &builder_distributed = builder.addTarget<LibraryTarget>("distributed");
    {
        builder_distributed.ApiName = "SW_BUILDER_DISTRIBUTED_API";
        builder_distributed += cpp20;
        builder_distributed += "src/sw/builder_distributed/.*"_rr;
        builder_distributed.Public += builder;
    }

    auto &core = p.addTarget<LibraryTarget>("core");
    {
        core.ApiName = "SW_CORE_API";
        core.ExportIfStatic = true;
        core += cpp20;
        core.Public += builder;
        core += "src/sw/core/.*"_rr;
        core += "org.sw.demo.Neargye.magic_enum"_dep;
        embed2("pub.egorpugin.primitives.tools.embedder2"_dep, core, "src/sw/core/inserts/input_db_schema.sql");
        gen_sqlite2cpp("pub.egorpugin.primitives.tools.sqlpp11.sqlite2cpp"_dep,
            core, core.SourceDir / "src/sw/core/inserts/input_db_schema.sql", "db_inputs.h", "db::inputs");
    }

    auto &cpp_driver = p.addTarget<LibraryTarget>("driver.cpp");
    {
        cpp_driver.ApiName = "SW_DRIVER_CPP_API";
        cpp_driver.ExportIfStatic = true;
        cpp_driver.PackageDefinitions = true;
        cpp_driver.WholeArchive = true;
        cpp_driver += cpp20;
        cpp_driver += "org.sw.demo.Kitware.CMake.lib"_dep; // cmake fe
        cpp_driver += "org.sw.demo.ReneNyffenegger.cpp_base64-master"_dep;
        cpp_driver.Public += core,
            "pub.egorpugin.primitives.patch"_dep,
            "org.sw.demo.ToruNiina.toml11"_dep,
            "org.sw.demo.boost.assign"_dep,
            "org.sw.demo.boost.bimap"_dep,
            "org.sw.demo.boost.uuid"_dep;
        cpp_driver.Public -= "org.sw.demo.giovannidicanio.winreg-2"_dep;
        cpp_driver += "src/sw/driver/.*"_rr;
        cpp_driver -= "src/sw/driver/tools/.*"_rr;
        cpp_driver -= "src/sw/driver/misc/delay_load_helper.cpp";
        gen_flex_bison("org.sw.demo.lexxmark.winflexbison"_dep, cpp_driver, "src/sw/driver/bazel/lexer.ll", "src/sw/driver/bazel/grammar.yy");
        if (cpp_driver.getCompilerType() == CompilerType::MSVC || cpp_driver.getCompilerType() == CompilerType::ClangCl)
            cpp_driver.CompileOptions.push_back("-bigobj");
        if (cpp_driver.getBuildSettings().TargetOS.Type == OSType::Windows)
        {
            cpp_driver.Public += "org.sw.demo.giovannidicanio.winreg-2"_dep;
            cpp_driver += "dbghelp.lib"_slib;
            cpp_driver += "OleAut32.lib"_slib;
        }
        if (cpp_driver.getCompilerType() == CompilerType::MSVC)
        {
            // for toml dependency
            cpp_driver.CompileOptions.push_back("/Zc:__cplusplus");
        }
        //else if (s.getBuildSettings().Native.CompilerType == CompilerType::GNU)
            //cpp_driver.CompileOptions.push_back("-Wa,-mbig-obj");

        if (cpp_driver.getBuildSettings().Native.LibrariesType == LibraryType::Shared)
            cpp_driver += "SW_DRIVER_SHARED_BUILD"_def;

        {
            auto &self_builder = cpp_driver.addTarget<ExecutableTarget>("self_builder");
            self_builder.PackageDefinitions = true;
            self_builder += cpp20;
            self_builder += "src/sw/driver/tools/self_builder.cpp";
            self_builder +=
                core,
                "pub.egorpugin.primitives.emitter"_dep,
                "pub.egorpugin.primitives.sw.main"_dep;

            auto c = cpp_driver.addCommand();
            c << cmd::prog(self_builder)
                << cmd::out("build_self.generated.h")
                << cmd::out("build_self.packages.generated.h")
                ;
        }
        {
            auto &cl_generator = cpp_driver.addTarget<ExecutableTarget>("cl_generator");
            cl_generator.PackageDefinitions = true;
            cl_generator += cpp20;
            cl_generator += "src/sw/driver/tools/cl_generator.*"_rr;
            cl_generator +=
                "pub.egorpugin.primitives.emitter"_dep,
                "pub.egorpugin.primitives.yaml"_dep,
                "pub.egorpugin.primitives.main"_dep;

            auto c = cpp_driver.addCommand();
            c << cmd::prog(cl_generator)
                << cmd::in("src/sw/driver/options_cl.yml")
                << cmd::out("options_cl.generated.h")
                << cmd::out("options_cl.generated.cpp", cmd::Skip)
                ;
        }
        //if (!s.Variables["SW_SELF_BUILD"])
        {
            /*PrecompiledHeader pch;
            pch.header = "src/sw/driver/pch.h";
            pch.force_include_pch = true;*/
            //cpp_driver.addPrecompiledHeader(pch);
        }

        embed2("pub.egorpugin.primitives.tools.embedder2"_dep, cpp_driver, "src/sw/driver/sw1.h");
        embed2("pub.egorpugin.primitives.tools.embedder2"_dep, cpp_driver, "src/sw/driver/sw_check_abi_version.h");
        embed2("pub.egorpugin.primitives.tools.embedder2"_dep, cpp_driver, "src/sw/driver/misc/delay_load_helper.cpp");

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
            embed2("pub.egorpugin.primitives.tools.embedder2"_dep, cpp_driver, pp, cpp_driver.BinaryPrivateDir / "sw.pp.emb", 1);

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
    auto &client_common = client.addTarget<LibraryTarget>("common");
    {
        client_common.ApiName = "SW_CLIENT_COMMON_API";
        client_common.PackageDefinitions = true;
        client_common.SwDefinitions = true;
        client_common.StartupProject = true;
        client_common += "src/sw/client/common/.*"_rr;
        client_common += cpp20;
        client_common.Public += builder_distributed, core, cpp_driver;

        embed2("pub.egorpugin.primitives.tools.embedder2"_dep, client_common, "src/sw/client/common/inserts/SWConfig.cmake");
        embed2("pub.egorpugin.primitives.tools.embedder2"_dep, client_common, "src/sw/client/common/inserts/project_templates.yml");

        generate_cl("pub.egorpugin.primitives.tools.cl_generator"_dep, client_common,
            "src/sw/client/common/cl.yml", "llvm");
    }

    // client
    {
        client.PackageDefinitions = true;
        client.SwDefinitions = true;
        client.StartupProject = true;
        client += "src/sw/client/cli/.*"_rr;
        client += cpp20;
        client += client_common,
            //"org.sw.demo.microsoft.mimalloc"_dep,
            "pub.egorpugin.primitives.sw.main"_dep
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

        create_git_revision("pub.egorpugin.primitives.tools.create_git_rev"_dep, client);
    }

    // tests
    {
        // at the moment tests cannot run in parallel
        auto p = std::make_shared<sw::ResourcePool>(1);

        auto add_build_test = [&cpp_driver, &client, &p](const path &dir)
        {
            auto t = cpp_driver.addTest(client);
            t->pool = p;
            t->push_back("build");
            t->push_back(dir);
            return t;
        };

        auto add_configs = [](auto &c)
        {
            c->push_back("-static");
            c->push_back("-shared");
            c->push_back("-config=d,msr,rwdi,r");
        };

        auto root = client.SourceDir / "test" / "build";
        auto add_build_test_with_configs = [&add_build_test, &add_configs, &root](const auto &dir)
        {
            auto t = add_build_test(root / dir);
            t.getCommand()->name = dir;
            add_configs(t.getCommand());
            return t;
        };

        add_build_test_with_configs("simple/sw.cpp");
        add_build_test_with_configs("c/exe");
        add_build_test_with_configs("c/api");
        add_build_test_with_configs("cpp/static");
        add_build_test_with_configs("cpp/multiconf");
        add_build_test_with_configs("cpp/pch");
    }

    auto &sp = sw.addProject("server");
    auto &mirror = sp.addTarget<ExecutableTarget>("mirror");
    {
        // move to src/sw/server/tools?
        mirror.PackageDefinitions = true;
        mirror += cpp20;
        mirror += "src/sw/tools/mirror.cpp";
        mirror += manager;
        mirror += "pub.egorpugin.primitives.sw.main"_dep;
    }

    if (s.getExternalVariables()["with-gui"] != "true")
        return;

    /*auto &gui = client.addTarget<ExecutableTarget>("gui", "0.4.0");
    {
        gui.PackageDefinitions = true;
        gui.SwDefinitions = true;
        gui += "src/sw/client/gui/.*"_rr;
        gui += cpp20;
        gui += client_common;

        gui += "org.sw.demo.qtproject.qt.base.widgets"_dep;
        gui += "org.sw.demo.qtproject.qt.base.winmain"_dep;
        gui += "org.sw.demo.qtproject.qt.base.plugins.platforms.windows"_dep;
        gui += "org.sw.demo.qtproject.qt.base.plugins.styles.windowsvista"_dep;
        gui += "org.sw.demo.qtproject.qt.labs.vstools.natvis-dev"_dep;

        gui -= "org.sw.demo.qtproject.qt.winextras"_dep;
        if (client.getBuildSettings().TargetOS.Type == OSType::Windows)
            gui += "org.sw.demo.qtproject.qt.winextras"_dep;

        if (auto L = gui.getSelectedTool()->as<VisualStudioLinker*>(); L)
            L->Subsystem = vs::Subsystem::Windows;

        qt_moc_rcc_uic("org.sw.demo.qtproject.qt"_dep, gui);
        qt_tr("org.sw.demo.qtproject.qt"_dep, gui);

        create_git_revision("pub.egorpugin.primitives.tools.create_git_rev"_dep, gui);
    }*/
}
