void build(Solution &s) {
    auto &t = s.addExecutable("test");
    t.UseModules = true;
    //t.GenerateWindowsResource = false;
    t += cpp23;
    t += "main.cpp";
    t += "m.cpp";
    t += "old_header.h"_qhu;
    t += "old_header2.h"_ahu;
}
