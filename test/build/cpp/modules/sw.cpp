void build(Solution &s) {
    auto &t = s.addExecutable("test");
    t.UseModules = true;
    //t.GenerateWindowsResource = false;
    t += cpp23;
    t += "MY_API"_api;
    t += "main.cpp";
    t += "m2.cpp";
    t += "Source.cpp";
    t += "m.ixx";
    t += "old_header.h"_qhu;
    t += "old_header2.h"_ahu;
}
