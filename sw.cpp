void configure(Solution &s)
{
    s.Settings.Native.LibrariesType = LibraryType::Static;
    s.Settings.Native.ConfigurationType = ConfigurationType::ReleaseWithDebugInformation;
}

void build(Solution &s)
{
    auto &p = s.addProject("cppan", "master");
    p.Source = Git("https://github.com/cppan/cppan", "", "{v}");

    auto &common = p.addTarget<StaticLibraryTarget>("common");
    common.CPPVersion = CPPLanguageStandard::CPP17;
    common +=
        "src/common/.*"_rr,
        "src/printers/.*"_rr,
        "src/comments/.*"_rr,
        "src/bazel/.*"_rr,
        "src/inserts/.*"_rr,
        "src/support/.*"_rr,
        "src/gen/.*"_rr;

    common -= "src/bazel/test/test.cpp", "src/gen/.*"_rr;
    common.Public += "src"_id, "src/common"_id, "src/support"_id;

    common.Public += "VERSION_MAJOR=0"_d;
    common.Public += "VERSION_MINOR=2"_d;
    common.Public += "VERSION_PATCH=4"_d;
    if (s.Settings.TargetOS.Type == OSType::Windows)
        common.Public += "UNICODE"_d;

    common.Public +=
        "pub.cppan2.demo.boost.optional-1"_dep,
        "pub.cppan2.demo.boost.property_tree-1"_dep,
        "pub.cppan2.demo.boost.variant-1"_dep,
        //"pub.cppan2.demo.boost.stacktrace-1"_dep,
        "pub.cppan2.demo.apolukhin.stacktrace-master"_dep,
        "pub.cppan2.demo.sqlite3-3"_dep,

        "pub.egorpugin.primitives.string-master"_dep,
        "pub.egorpugin.primitives.filesystem-master"_dep,
        "pub.egorpugin.primitives.context-master"_dep,
        "pub.egorpugin.primitives.date_time-master"_dep,
        "pub.egorpugin.primitives.executor-master"_dep,
        "pub.egorpugin.primitives.hash-master"_dep,
        "pub.egorpugin.primitives.http-master"_dep,
        "pub.egorpugin.primitives.lock-master"_dep,
        "pub.egorpugin.primitives.log-master"_dep,
        "pub.egorpugin.primitives.pack-master"_dep,
        "pub.egorpugin.primitives.command-master"_dep,
        "pub.egorpugin.primitives.yaml-master"_dep;

    time_t v;
    time(&v);
    common.fileWriteSafe("stamp.h.in", "\"" + std::to_string(v) + "\"", true);

    auto &inserts_generator = p.addTarget<ExecutableTarget>("inserts_generator");
    inserts_generator.CPPVersion = CPPLanguageStandard::CPP17;
    inserts_generator += "src/gen/inserter.cpp";
    inserts_generator += "pub.egorpugin.primitives.filesystem-master"_dep;

    {
        auto c = std::make_shared<Command>();
        c->program = inserts_generator.getOutputFile();
        c->args.push_back((common.SourceDir / "src/inserts/inserts.cpp.in").string());
        c->args.push_back((common.BinaryDir / "inserts.cpp").string());
        c->working_directory = common.SourceDir / "src";
        c->addInput(c->args[0]);
        c->addOutput(c->args[1]);
        common += path(c->args[1]);
    }

    auto flex_bison = [&common](const std::string &name)
    {

        fs::create_directories(common.BinaryDir / ("src/" + name));

        // flex/bison
        {
            auto c = std::make_shared<Command>();
            c->program = "bison.exe";
            c->args.push_back("-d");
            c->args.push_back("-o" + (common.BinaryDir / ("src/" + name + "/grammar.cpp")).string());
            c->args.push_back((common.SourceDir / ("src/" + name + "/grammar.yy")).string());
            c->addInput(common.SourceDir / ("src/" + name + "/grammar.yy"));
            c->addOutput(common.BinaryDir / ("src/" + name + "/grammar.cpp"));
            common += path(common.BinaryDir / ("src/" + name + "/grammar.cpp"));
        }
        {
            auto c = std::make_shared<Command>();
            c->program = "flex.exe";
            c->args.push_back("--header-file=" + (common.BinaryDir / ("src/" + name + "/lexer.h")).string());
            c->args.push_back("-o" + (common.BinaryDir / ("src/" + name + "/lexer.cpp")).string());
            c->args.push_back((common.SourceDir / ("src/" + name + "/lexer.ll")).string());
            c->addInput(common.SourceDir / ("src/" + name + "/lexer.ll"));
            c->addOutput(common.BinaryDir / ("src/" + name + "/lexer.h"));
            c->addOutput(common.BinaryDir / ("src/" + name + "/lexer.cpp"));
            common += path(common.BinaryDir / ("src/" + name + "/lexer.cpp"));
        }
    };

    //flex_bison("bazel");
    //flex_bison("comments");

    auto &client = p.addTarget<ExecutableTarget>("client");
    client.CPPVersion = CPPLanguageStandard::CPP17;
    client += "src/client/.*"_rr, common,
        "pub.cppan2.demo.boost.program_options-1"_dep,
        "pub.cppan2.demo.yhirose.cpp_linenoise-master"_dep;
}
