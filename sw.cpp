#pragma sw require header org.sw.demo.google.grpc.cpp.plugin
#pragma sw require header org.sw.demo.lexxmark.winflexbison.bison
#pragma sw require header org.sw.demo.qtproject.qt.base.tools.moc
#pragma sw require header pub.egorpugin.primitives.tools.embedder-0.3.1

#define QT_VERSION ""
#define PRIMITIVES_VERSION "-0.3.1"

void build(Solution &s)
{
    auto &sw = s.addProject("sw", "0.4.5");
    sw += Git("https://github.com/SoftwareNetwork/sw", "", "b0.4.5");

    auto &p = sw.addProject("client");

    auto cppstd = cpp23;

    auto &support = p.addTarget<LibraryTarget>("support");
    {
        support.ApiName = "SW_SUPPORT_API";
        support.ExportIfStatic = true;
        support += cppstd;
        support += "src/sw/support/.*"_rr;
        auto cmddep = "pub.egorpugin.primitives.command" PRIMITIVES_VERSION ""_dep;
        auto verdep = "pub.egorpugin.primitives.version1" PRIMITIVES_VERSION ""_dep;
        auto srcdep = "pub.egorpugin.primitives.source1" PRIMITIVES_VERSION ""_dep;
        support.Public +=
            cmddep, verdep, srcdep,
            "pub.egorpugin.primitives.date_time" PRIMITIVES_VERSION ""_dep,
            "pub.egorpugin.primitives.http" PRIMITIVES_VERSION ""_dep,
            "pub.egorpugin.primitives.hash" PRIMITIVES_VERSION ""_dep,
            "pub.egorpugin.primitives.log" PRIMITIVES_VERSION ""_dep,
            "pub.egorpugin.primitives.executor" PRIMITIVES_VERSION ""_dep,
            "pub.egorpugin.primitives.symbol" PRIMITIVES_VERSION ""_dep,
            "org.sw.demo.boost.property_tree"_dep,
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
    }

    auto &protos = p.addTarget<StaticLibraryTarget>("protos");
    {
        protos += cpp17; // some bug with protobuf/vs
        protos += "src/sw/protocol/.*"_rr;
        protos.Public += "pub.egorpugin.primitives.grpc_helpers" PRIMITIVES_VERSION ""_dep;
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
        manager += cppstd;
        manager.Public += "BOOST_DLL_USE_STD_FS"_def;

        manager +=
            "pub.egorpugin.primitives.csv" PRIMITIVES_VERSION ""_dep;
        manager.Public += support, protos,
            "pub.egorpugin.primitives.db.sqlite3" PRIMITIVES_VERSION ""_dep,
            "pub.egorpugin.primitives.lock" PRIMITIVES_VERSION ""_dep,
            "pub.egorpugin.primitives.pack" PRIMITIVES_VERSION ""_dep,
            "pub.egorpugin.primitives.sw.settings" PRIMITIVES_VERSION ""_dep,
            "pub.egorpugin.primitives.yaml" PRIMITIVES_VERSION ""_dep,
            "org.sw.demo.nlohmann.json"_dep,
            "org.sw.demo.boost.variant"_dep,
            "org.sw.demo.boost.dll"_dep,
            "org.sw.demo.rbock.sqlpp11_connector_sqlite3"_dep
            ;

        manager.Public -= "pub.egorpugin.primitives.win32helpers" PRIMITIVES_VERSION ""_dep;
        if (manager.getBuildSettings().TargetOS.Type == OSType::Windows)
            manager.Public += "pub.egorpugin.primitives.win32helpers" PRIMITIVES_VERSION ""_dep;

        manager += "src/sw/manager/.*"_rr;
        manager.Public += "src/sw/manager/manager.natvis";
        manager.Public.Definitions["VERSION_MAJOR"] += std::to_string(manager.getPackage().getVersion().getMajor());
        manager.Public.Definitions["VERSION_MINOR"] += std::to_string(manager.getPackage().getVersion().getMinor());
        manager.Public.Definitions["VERSION_PATCH"] += std::to_string(manager.getPackage().getVersion().getPatch());
        embed2("pub.egorpugin.primitives.tools.embedder2" PRIMITIVES_VERSION ""_dep, manager, "src/sw/manager/inserts/packages_db_schema.sql");
        gen_sqlite2cpp("pub.egorpugin.primitives.tools.sqlpp11.sqlite2cpp" PRIMITIVES_VERSION ""_dep,
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
        builder += cppstd;
        builder += "src/sw/builder/.*"_rr;
        builder.Public += manager,
            "org.sw.demo.preshing.junction-master"_dep,
            "org.sw.demo.boost.graph"_dep,
            "org.sw.demo.boost.serialization"_dep,
            "org.sw.demo.microsoft.gsl"_dep,
            "pub.egorpugin.primitives.emitter" PRIMITIVES_VERSION ""_dep;
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
        builder_distributed += cppstd;
        builder_distributed += "src/sw/builder_distributed/.*"_rr;
        builder_distributed.Public += builder;
    }

    auto &core = p.addTarget<LibraryTarget>("core");
    {
        core.ApiName = "SW_CORE_API";
        core.ExportIfStatic = true;
        core += cppstd;
        core.Public += builder;
        core += "src/sw/core/.*"_rr;
        core += "org.sw.demo.Neargye.magic_enum"_dep;
        core += "org.sw.demo.zeux.pugixml"_dep;
        embed2("pub.egorpugin.primitives.tools.embedder2" PRIMITIVES_VERSION ""_dep, core, "src/sw/core/inserts/input_db_schema.sql");
        gen_sqlite2cpp("pub.egorpugin.primitives.tools.sqlpp11.sqlite2cpp" PRIMITIVES_VERSION ""_dep,
            core, core.SourceDir / "src/sw/core/inserts/input_db_schema.sql", "db_inputs.h", "db::inputs");
    }

    auto &cpp_driver = p.addTarget<LibraryTarget>("driver.cpp");
    {
        cpp_driver.ApiName = "SW_DRIVER_CPP_API";
        cpp_driver.ExportIfStatic = true;
        cpp_driver.PackageDefinitions = true;
        cpp_driver.WholeArchive = true;
        cpp_driver += cppstd;
        cpp_driver += "org.sw.demo.Kitware.CMake.lib"_dep; // cmake fe
        cpp_driver += "org.sw.demo.ReneNyffenegger.cpp_base64-master"_dep;
        cpp_driver.Public += core,
            "pub.egorpugin.primitives.patch" PRIMITIVES_VERSION ""_dep,
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

        cpp_driver += "PRIMITIVES_VERSION=\"" PRIMITIVES_VERSION "\""_def;
        {
            auto &self_builder = cpp_driver.addTarget<ExecutableTarget>("self_builder");
            self_builder += "PRIMITIVES_VERSION=\"" PRIMITIVES_VERSION "\""_def;
            self_builder.PackageDefinitions = true;
            self_builder += cppstd;
            self_builder += "src/sw/driver/tools/self_builder.cpp";
            self_builder +=
                core,
                "pub.egorpugin.primitives.emitter" PRIMITIVES_VERSION ""_dep,
                "pub.egorpugin.primitives.sw.main" PRIMITIVES_VERSION ""_dep;

            auto c = cpp_driver.addCommand();
            c << cmd::prog(self_builder)
                << cmd::out("build_self.generated.h")
                << cmd::out("build_self.packages.generated.h")
                ;
        }
        {
            auto &cl_generator = cpp_driver.addTarget<ExecutableTarget>("cl_generator");
            cl_generator.PackageDefinitions = true;
            cl_generator += cppstd;
            cl_generator += "src/sw/driver/tools/cl_generator.*"_rr;
            cl_generator +=
                "pub.egorpugin.primitives.emitter" PRIMITIVES_VERSION ""_dep,
                "pub.egorpugin.primitives.yaml" PRIMITIVES_VERSION ""_dep,
                "pub.egorpugin.primitives.main" PRIMITIVES_VERSION ""_dep;

            auto c = cpp_driver.addCommand();
            c << cmd::prog(cl_generator)
                << cmd::in("src/sw/driver/options_cl.yml")
                << cmd::out("options_cl.generated.h")
                << cmd::out("options_cl.generated.cpp", cmd::Skip)
                ;
            std::dynamic_pointer_cast<::sw::driver::Command>(c.getCommand())->ignore_deps_generated_commands = true;
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

        embed2("pub.egorpugin.primitives.tools.embedder2" PRIMITIVES_VERSION ""_dep, cpp_driver, "src/sw/driver/sw1.h");
        embed2("pub.egorpugin.primitives.tools.embedder2" PRIMITIVES_VERSION ""_dep, cpp_driver, "src/sw/driver/sw_check_abi_version.h");
        embed2("pub.egorpugin.primitives.tools.embedder2" PRIMITIVES_VERSION ""_dep, cpp_driver, "src/sw/driver/misc/delay_load_helper.cpp");

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
            embed2("pub.egorpugin.primitives.tools.embedder2" PRIMITIVES_VERSION ""_dep, cpp_driver, pp, cpp_driver.BinaryPrivateDir / "sw.pp.emb", 1);

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
        client_common += cppstd;
        client_common.Public += builder_distributed, core, cpp_driver;

        embed2("pub.egorpugin.primitives.tools.embedder2" PRIMITIVES_VERSION ""_dep, client_common, "src/sw/client/common/inserts/SWConfig.cmake");
        embed2("pub.egorpugin.primitives.tools.embedder2" PRIMITIVES_VERSION ""_dep, client_common, "src/sw/client/common/inserts/project_templates.yml");

        generate_cl("pub.egorpugin.primitives.tools.cl_generator" PRIMITIVES_VERSION ""_dep, client_common,
            "src/sw/client/common/cl.yml", "llvm");
    }

    // client
    {
        client.PackageDefinitions = true;
        client.SwDefinitions = true;
        client.StartupProject = true;
        client += "src/sw/client/cli/.*"_rr;
        client += cppstd;
        client += client_common,
            //"org.sw.demo.microsoft.mimalloc"_dep,
            "pub.egorpugin.primitives.sw.main" PRIMITIVES_VERSION ""_dep
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

        create_git_revision("pub.egorpugin.primitives.tools.create_git_rev" PRIMITIVES_VERSION ""_dep, client);
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
        mirror += cppstd;
        mirror += "src/sw/tools/mirror.cpp";
        mirror += manager;
        mirror += "pub.egorpugin.primitives.sw.main" PRIMITIVES_VERSION ""_dep;
    }

    if (s.getExternalVariables()["with-gui"] != "true")
        return;

    auto &gui = client.addTarget<ExecutableTarget>("gui", "0.4.0");
    {
        auto &t = gui;
        t.PackageDefinitions = true;
        t.SwDefinitions = true;
        t += "src/sw/client/gui/.*"_rr;
        t += cppstd;
        t += client_common;

        t += "org.sw.demo.qtproject.qt.base.widgets" QT_VERSION ""_dep;

        if (t.getBuildSettings().TargetOS.Type == OSType::Windows) {
            if (auto L = t.getSelectedTool()->as<VisualStudioLinker*>(); L)
                L->Subsystem = vs::Subsystem::Windows;
            t += "org.sw.demo.qtproject.qt.base.winmain" QT_VERSION ""_dep;
            t += "org.sw.demo.qtproject.qt.base.plugins.platforms.windows" QT_VERSION ""_dep;
            t += "org.sw.demo.qtproject.qt.base.plugins.styles.windowsvista" QT_VERSION ""_dep;
        }
        if (t.getBuildSettings().TargetOS.Type == OSType::Linux) {
            t += "org.sw.demo.qtproject.qt.wayland.plugins.platforms.qwayland.generic" QT_VERSION ""_dep;
            t += "org.sw.demo.qtproject.qt.wayland.plugins.platforms.qwayland.egl" QT_VERSION ""_dep;
            t += "org.sw.demo.qtproject.qt.wayland.plugins.hardwareintegration.client.wayland_egl" QT_VERSION ""_dep;
            t += "org.sw.demo.qtproject.qt.wayland.plugins.shellintegration.xdg" QT_VERSION ""_dep;
            t += "org.sw.demo.qtproject.qt.wayland.plugins.decorations.bradient" QT_VERSION ""_dep;
        }
        if (t.getBuildSettings().TargetOS.Type == OSType::Macos) {
            t += "org.sw.demo.qtproject.qt.base.plugins.platforms.cocoa" QT_VERSION ""_dep;
        }

        qt_moc_rcc_uic("org.sw.demo.qtproject.qt" QT_VERSION ""_dep, t);
        qt_tr("org.sw.demo.qtproject.qt" QT_VERSION ""_dep, t);

        create_git_revision("pub.egorpugin.primitives.tools.create_git_rev" PRIMITIVES_VERSION ""_dep, t);
    }
}
