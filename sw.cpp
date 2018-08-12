void configure(Solution &s)
{
    s.Settings.Native.LibrariesType = LibraryType::Static;
    s.Settings.Native.ConfigurationType = ConfigurationType::ReleaseWithDebugInformation;
}

void build(Solution &s)
{
    auto &p = s.addProject("sw", "0.3.0");

    auto &support = p.addTarget<StaticLibraryTarget>("support");
    support.setRootDirectory("src/support");
    support.CPPVersion = CPPLanguageStandard::CPP17;
    support += ".*"_rr;
    support.Public +=
        "pub.egorpugin.primitives.http-master"_dep,
        "pub.egorpugin.primitives.hash-master"_dep,
        "pub.egorpugin.primitives.command-master"_dep,
        "pub.egorpugin.primitives.log-master"_dep,
        "pub.egorpugin.primitives.executor-master"_dep,
        "org.sw.demo.boost.property_tree-1"_dep,
        "org.sw.demo.boost.stacktrace-1"_dep,
        "org.sw.demo.boost.dll-1"_dep;
    support.ApiName = "SW_SUPPORT_API";
    if (s.Settings.TargetOS.Type == OSType::Windows)
        support.Public += "UNICODE"_d;

    auto &protos = p.addTarget<StaticLibraryTarget>("protos");
    protos.CPPVersion = CPPLanguageStandard::CPP17;
    protos.setRootDirectory("src/protocol");
    protos += ".*"_rr;
    protos.Public +=
        "org.sw.demo.google.grpc.grpcpp-1"_dep,
        "pub.egorpugin.primitives.log-master"_dep;
    gen_grpc(protos, protos.SourceDir / "api.proto", true);

    auto &manager = p.addTarget<LibraryTarget>("manager");
    manager.ApiName = "SW_MANAGER_API";
    //manager.ExportIfStatic = true;
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
        "pub.egorpugin.primitives.win32helpers-master"_dep;
    manager += "src/manager/.*"_rr, "include/manager/.*"_rr;
    manager.Public += "include"_idir, "src/manager"_idir;
    manager.Public.Definitions["VERSION_MAJOR"] += std::to_string(manager.getPackage().version.getMajor());
    manager.Public.Definitions["VERSION_MINOR"] += std::to_string(manager.getPackage().version.getMinor());
    manager.Public.Definitions["VERSION_PATCH"] += std::to_string(manager.getPackage().version.getPatch());
    embed(manager, manager.SourceDir / "src/manager/inserts/inserts.cpp.in");
    gen_sqlite2cpp(manager, manager.SourceDir / "src/manager/inserts/packages_db_schema.sql", "db_packages.h", "db::packages");
    gen_sqlite2cpp(manager, manager.SourceDir / "src/manager/inserts/service_db_schema.sql", "db_service.h", "db::service");

    auto &builder = p.addTarget<LibraryTarget>("builder");
    builder.ApiName = "SW_BUILDER_API";
    //builder.ExportIfStatic = true;
    builder.CPPVersion = CPPLanguageStandard::CPP17;
    builder += "src/builder/.*"_rr, "include/builder/.*"_rr;
    builder.Public += "include"_idir, "src/builder"_idir;
    builder -= "src/builder/db_sqlite.*"_rr;
    builder.Public += manager, "org.sw.demo.preshing.junction-master"_dep;

    auto &cpp_driver = p.addTarget<LibraryTarget>("driver.cpp");
    cpp_driver.ApiName = "SW_DRIVER_CPP_API";
    //cpp_driver.ExportIfStatic = true;
    cpp_driver.CPPVersion = CPPLanguageStandard::CPP17;
    cpp_driver.Public += builder,
        "org.sw.demo.boost.assign-1"_dep,
        "org.sw.demo.boost.uuid-1"_dep,
        "pub.egorpugin.primitives.context-master"_dep;
    cpp_driver += "src/driver/cpp/.*"_rr, "include/driver/cpp/.*"_rr;
    cpp_driver.Public += "include"_idir, "src/driver/cpp"_idir;
    embed(cpp_driver, cpp_driver.SourceDir / "src/driver/cpp/inserts/inserts.cpp.in");

#ifndef SW_SELF_BUILD
    auto &client = p.addTarget<ExecutableTarget>("client");
    client.setRootDirectory("src/client");
    client += ".*"_rr;
    client.CPPVersion = CPPLanguageStandard::CPP17;
    client += cpp_driver,
        "org.sw.demo.taywee.args"_dep,
        "org.sw.demo.giovannidicanio.winreg-master"_dep,
        "pub.egorpugin.primitives.minidump-master"_dep;

    auto &t = p.addDirectory("tools");
    if (s.Settings.TargetOS.Type == OSType::Windows)
    {
        auto &client = t.addTarget<ExecutableTarget>("client");
        client += "src/tools/client.cpp";
        client +=
            "org.sw.demo.boost.dll-1"_dep,
            "org.sw.demo.boost.filesystem-1"_dep,
            "user32.lib"_lib;
        if (s.Settings.TargetOS.Type == OSType::Windows)
            client.Public += "UNICODE"_d;
    }
#endif
}
