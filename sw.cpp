void configure(Solution &s)
{
    s.Settings.Native.LibrariesType = LibraryType::Static;
    s.Settings.Native.ConfigurationType = ConfigurationType::ReleaseWithDebugInformation;
}

void build(Solution &s)
{
    auto &p = s.addProject("cppan2", "0.3.0");

    auto &support = p.addTarget<StaticLibraryTarget>("support");
    support.setRootDirectory("src/support");
    support.CPPVersion = CPPLanguageStandard::CPP17;
    support += ".*"_rr;
    support.Public +=
        "pub.egorpugin.primitives.hash-master"_dep,
        "pub.egorpugin.primitives.http-master"_dep,
        "pub.egorpugin.primitives.command-master"_dep,
        "pub.egorpugin.primitives.log-master"_dep,
        "pub.egorpugin.primitives.executor-master"_dep,
        "pub.cppan2.demo.boost.property_tree-1"_dep,
        "pub.cppan2.demo.boost.dll-1"_dep;
    if (s.Settings.TargetOS.Type == OSType::Windows)
        support.Public += "UNICODE"_d;

    auto &manager = p.addTarget<LibraryTarget>("manager");
    manager.setRootDirectory("src/manager");
    manager.ApiName = "CPPAN_MANAGER_API";
    manager.ExportIfStatic = true;
    manager.CPPVersion = CPPLanguageStandard::CPP17;
    manager += ".*"_rr;
    manager.Public += support,
        "pub.egorpugin.primitives.yaml-master"_dep,
        "pub.egorpugin.primitives.date_time-master"_dep,
        "pub.egorpugin.primitives.lock-master"_dep,
        "pub.egorpugin.primitives.pack-master"_dep,
        "pub.cppan2.demo.boost.variant-1"_dep,
        "pub.cppan2.demo.boost.stacktrace-1"_dep,
        "pub.cppan2.demo.sqlite3-3"_dep,
        "pub.cppan2.demo.fmt"_dep;
    manager.Public.Definitions["VERSION_MAJOR"] += std::to_string(manager.getPackage().version.Major);
    manager.Public.Definitions["VERSION_MINOR"] += std::to_string(manager.getPackage().version.Minor);
    manager.Public.Definitions["VERSION_PATCH"] += std::to_string(manager.getPackage().version.Patch);

    auto &inserter = p.addTarget<ExecutableTarget>("inserter");
    inserter.setRootDirectory("src/inserts");
    inserter.CPPVersion = CPPLanguageStandard::CPP17;
    inserter += ".*"_rr;
    inserter += "pub.egorpugin.primitives.filesystem-master"_dep;

    auto &builder = p.addTarget<LibraryTarget>("builder");
    builder.setRootDirectory("src/builder");
    builder.ApiName = "CPPAN_BUILDER_API";
    builder.ExportIfStatic = true;
    builder.CPPVersion = CPPLanguageStandard::CPP17;
    builder += ".*"_rr;
    builder -= "db_sqlite.*"_rr;
    builder.Public += manager,
        "pub.cppan2.demo.boost.assign-1"_dep,
        "pub.cppan2.demo.rbock.sqlpp11_connector_sqlite3-0.24"_dep,
        "pub.cppan2.preshing.junction-master"_dep;

    {
        auto in = inserter.SourceDir / "inserts.cpp.in";
        auto out = builder.BinaryDir / "inserts.cpp";
        auto c = std::make_shared<Command>();
        c->program = inserter.getOutputFile();
        c->args = { in.string(), out.string() };
        c->working_directory = inserter.SourceDir;
        c->addInput(in);
        c->addOutput(out);
        builder += out;
    }

    auto &client = p.addTarget<ExecutableTarget>("client");
    client.setRootDirectory("src/client");
    client += ".*"_rr;
    client.CPPVersion = CPPLanguageStandard::CPP17;
    client += builder,
        "pub.cppan2.demo.taywee.args"_dep,
        "pub.cppan2.demo.giovannidicanio.winreg-master"_dep;

    auto &srv = p.addDirectory("server");
    auto &webapp = srv.addTarget<ExecutableTarget>("webapp");
    webapp.setRootDirectory("src/server/webapp");
    webapp += ".*"_rr;
    webapp.CPPVersion = CPPLanguageStandard::CPP17;
    webapp += builder,
        "pub.cppan2.demo.emweb.wt.http"_dep,
        "pub.cppan2.demo.jtv.pqxx"_dep;

    auto &t = p.addDirectory("tools");
    if (s.Settings.TargetOS.Type == OSType::Windows)
    {
        auto &client = t.addTarget<ExecutableTarget>("client");
        client += "src/tools/client.cpp";
        client +=
            "pub.cppan2.demo.boost.dll-1"_dep,
            "pub.cppan2.demo.boost.filesystem-1"_dep,
            "user32.lib"_lib;
        if (s.Settings.TargetOS.Type == OSType::Windows)
            client.Public += "UNICODE"_d;
    }
}
