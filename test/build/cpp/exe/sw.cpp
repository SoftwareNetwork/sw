void build(Solution &s)
{
    auto &t = s.addTarget<ExecutableTarget>("exe");

    t.Definitions["AND_MY_STRING"] = "\"my string\"";
    t.Definitions["AND_MY_STRING1"] = "\"my string\"";
    t.Private.Definitions["AND_MY_STRING2"] = "\"my string\"";

    //t.Linker->

    //t.SourceFiles.add(std::regex(".*"));
    t += ".*\\.txt"_r;
    t += ".*\\.txt"_r;
    t += ".*\\.txt"_rr;
    t += ".*\\.cpp"_rr;
    t += ".*\\.h"_rr;
    //t -= "1/x.cpp";
    t += "1/x.cpp";
}