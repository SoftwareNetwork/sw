// Copyright (C) 2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <sw/driver/cppan/driver.h>

namespace sw::driver::cppan
{

//SW_REGISTER_PACKAGE_DRIVER(CppanDriver);

path CppanDriver::getConfigFilename() const
{
    return filename;
}

PackageScriptPtr CppanDriver::load(const path &file_or_dir) const
{
    return PackageScriptPtr{};
}

bool CppanDriver::execute(const path &file_or_dir) const
{
    auto f = file_or_dir;
    if (fs::is_directory(f))
    {
        if (!hasConfig(f))
            return false;
        f /= getConfigFilename();
    }

    /*{
    Build b;
    auto &s = b.addSolution();
    s.SourceDir = "d:\\dev\\cppan2\\test\\build\\cpp\\exe2";
    auto &t = s.addTarget<ExecutableTarget>("exe");
    t += ".*"_rr;
    b.execute();
    return;
    }*/

    //ScopedTime t;


    /*if (p.filename() != "sw.cpp")
    {
    Build b;
    b.Local = true;
    auto &t = b.addTarget<ExecutableTarget>(p.filename().stem().string());
    t += p;
    b.execute();
    return;
    }*/

    //single_process_job(".sw/build", [&f, &t]()
    {
        /*Build b;
        b.Local = true;
        b.configure = true;
        b.build_and_run(f);*/

        //LOG_INFO(logger, "Total time: " << t.getTimeFloat());
    }//);

    return true;
}

} // namespace sw::driver
