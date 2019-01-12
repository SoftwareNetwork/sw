#pragma sw require header pub.egorpugin.primitives.tools.embedder-master
#pragma sw require header org.sw.demo.google.grpc.grpc_cpp_plugin-1
#pragma sw require header org.sw.demo.lexxmark.winflexbison.bison-master

void configure(Solution &s)
{
    s.Settings.Native.LibrariesType = LibraryType::Static;
    //s.Settings.Native.ConfigurationType = ConfigurationType::ReleaseWithDebugInformation;
    //s.Settings.Native.CompilerType = CompilerType::ClangCl;
    //s.Settings.Native.CompilerType = CompilerType::Clang;
}

void build(Solution &s)
{
    auto &p = s.addProject("sw.client", "0.3.0");
    p += Git("https://github.com/SoftwareNetwork/sw", "", "master");

    auto &support = p.addTarget<StaticLibraryTarget>("support");
    support.CPPVersion = CPPLanguageStandard::CPP17;
    support += "src/support/.*"_rr;
    support.Public += "src/support"_idir;
    support.Public +=
        "pub.egorpugin.primitives.http-master"_dep,
        "pub.egorpugin.primitives.hash-master"_dep,
        "pub.egorpugin.primitives.command-master"_dep,
        "pub.egorpugin.primitives.log-master"_dep,
        "pub.egorpugin.primitives.executor-master"_dep,
        "pub.egorpugin.primitives.symbol-master"_dep,
        "org.sw.demo.boost.property_tree-1"_dep,
        "org.sw.demo.boost.stacktrace-1"_dep;
    support.ApiName = "SW_SUPPORT_API";
    if (s.Settings.TargetOS.Type == OSType::Windows)
        support.Public += "UNICODE"_d;

    auto &protos = p.addTarget<StaticLibraryTarget>("protos");
    protos.CPPVersion = CPPLanguageStandard::CPP17;
    protos += "src/protocol/.*"_rr;
    protos.Public += "src/protocol"_idir;
    protos.Public +=
        "org.sw.demo.google.grpc.grpcpp-1"_dep,
        "pub.egorpugin.primitives.templates-master"_dep,
        "pub.egorpugin.primitives.log-master"_dep;
    gen_grpc(protos, protos.SourceDir / "src/protocol/api.proto", true);

    auto &manager = p.addTarget<LibraryTarget>("manager");
    manager.ApiName = "SW_MANAGER_API";
    manager.ExportIfStatic = true;
    manager.CPPVersion = CPPLanguageStandard::CPP17;
    manager.Public += support, protos,
        "pub.egorpugin.primitives.yaml-master"_dep,
        "pub.egorpugin.primitives.date_time-master"_dep,
        "pub.egorpugin.primitives.lock-master"_dep,
        "pub.egorpugin.primitives.pack-master"_dep,
        "org.sw.demo.nlohmann.json-3"_dep,
        "org.sw.demo.boost.variant-1"_dep,
        "org.sw.demo.boost.dll-1"_dep,
        "pub.egorpugin.primitives.db.sqlite3-master"_dep,
        "org.sw.demo.rbock.sqlpp11_connector_sqlite3-0"_dep,
        "pub.egorpugin.primitives.version-master"_dep,
        "pub.egorpugin.primitives.sw.settings-master"_dep,
        "pub.egorpugin.primitives.win32helpers-master"_dep;
    manager += "src/manager/.*"_rr, "include/sw/manager/.*"_rr;
    manager.Public += "include"_idir, "src/manager"_idir;
    manager.Public.Definitions["VERSION_MAJOR"] += std::to_string(manager.getPackage().version.getMajor());
    manager.Public.Definitions["VERSION_MINOR"] += std::to_string(manager.getPackage().version.getMinor());
    manager.Public.Definitions["VERSION_PATCH"] += std::to_string(manager.getPackage().version.getPatch());
    embed(manager, "src/manager/inserts/inserts.cpp.in");
    gen_sqlite2cpp(manager, manager.SourceDir / "src/manager/inserts/packages_db_schema.sql", "db_packages.h", "db::packages");
    gen_sqlite2cpp(manager, manager.SourceDir / "src/manager/inserts/service_db_schema.sql", "db_service.h", "db::service");
    if (!s.Variables["SW_SELF_BUILD"])
    {
        PrecompiledHeader pch;
        pch.header = "src/manager/pch.h";
        pch.force_include_pch = true;
        manager.addPrecompiledHeader(pch);
    }

    auto &tools = p.addDirectory("tools");
    auto &self_builder = tools.addTarget<ExecutableTarget>("self_builder");
    self_builder.PackageDefinitions = true;
    self_builder.CPPVersion = CPPLanguageStandard::CPP17;
    self_builder += "src/tools/self_builder.cpp";
    self_builder +=
        manager,
        "pub.egorpugin.primitives.context-master"_dep,
        "pub.egorpugin.primitives.sw.main-master"_dep;

    auto &builder = p.addTarget<LibraryTarget>("builder");
    builder.ApiName = "SW_BUILDER_API";
    builder.ExportIfStatic = true;
    builder.CPPVersion = CPPLanguageStandard::CPP17;
    builder += "src/builder/.*"_rr, "include/sw/builder/.*"_rr;
    builder.Public += "include"_idir, "src/builder"_idir;
    builder -= "src/builder/db_sqlite.*"_rr;
    builder.Public += manager, "org.sw.demo.preshing.junction-master"_dep,
        "pub.egorpugin.primitives.context-master"_dep;

    auto &cpp_driver = p.addTarget<LibraryTarget>("driver.cpp");
    cpp_driver.ApiName = "SW_DRIVER_CPP_API";
    cpp_driver.ExportIfStatic = true;
    cpp_driver.CPPVersion = CPPLanguageStandard::CPP17;
    cpp_driver.Public += builder,
        "org.sw.demo.microsoft.gsl-*"_dep,
        "org.sw.demo.boost.assign-1"_dep,
        "org.sw.demo.boost.bimap-1"_dep,
        "org.sw.demo.boost.uuid-1"_dep;
    cpp_driver += "src/driver/cpp/.*"_rr, "include/sw/driver/cpp/.*"_rr;
    cpp_driver -= "src/driver/cpp/inserts/.*"_rr;
    if (s.Settings.TargetOS.Type != OSType::Windows)
        cpp_driver -= "src/driver/cpp/misc/.*"_rr;
    cpp_driver.Public += "include"_idir, "src/driver/cpp"_idir;
    embed(cpp_driver, "src/driver/cpp/inserts/inserts.cpp.in");
    gen_flex_bison(cpp_driver, "src/driver/cpp/bazel/lexer.ll", "src/driver/cpp/bazel/grammar.yy");
    if (s.Settings.Native.CompilerType == CompilerType::MSVC)
        cpp_driver.CompileOptions.push_back("-bigobj");
    //else if (s.Settings.Native.CompilerType == CompilerType::GNU)
        //cpp_driver.CompileOptions.push_back("-Wa,-mbig-obj");
    {
        auto c = cpp_driver.addCommand();
        c << cmd::prog(self_builder)
            << cmd::out("build_self.generated.h")
            << cmd::out("build_self.packages.generated.h")
            ;
    }

    auto &client = p.addTarget<ExecutableTarget>("sw");
    client.PackageDefinitions = true;
    client += "src/client/.*"_rr;
    client += "src/client"_idir;
    client.CPPVersion = CPPLanguageStandard::CPP17;
    client += cpp_driver,
        "pub.egorpugin.primitives.sw.main-master"_dep,
        "org.sw.demo.giovannidicanio.winreg-master"_dep;
    if (s.Settings.TargetOS.Type == OSType::Linux)
    {
        //client.getSelectedTool()->LinkOptions.push_back("-static-libstdc++");
        //client.getSelectedTool()->LinkOptions.push_back("-static-libgcc");
    }

    if (s.Settings.TargetOS.Type == OSType::Windows)
    {
        auto &client = tools.addTarget<ExecutableTarget>("client");
        client += "src/tools/client.cpp";
        client +=
            "org.sw.demo.boost.dll-1"_dep,
            "org.sw.demo.boost.filesystem-1"_dep,
            "user32.lib"_lib;
        if (s.Settings.TargetOS.Type == OSType::Windows)
            client.Public += "UNICODE"_d;
    }
}
