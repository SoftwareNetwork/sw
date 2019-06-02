// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "driver.h"

#include "build.h"

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "driver.cpp");

namespace sw::driver::cpp
{

static std::optional<path> findConfig(const path &dir, const FilesOrdered &fe_s)
{
    for (auto &fn : fe_s)
    {
        if (fs::exists(dir / fn))
            return dir / fn;
    }
    return {};
}

PackageId Driver::getPackageId() const
{
    return "org.sw.sw.driver.cpp-0.3.0";
}

bool Driver::canLoad(const Input &i) const
{
    switch (i.getType())
    {
    case InputType::PackageId:
        // create package, install, get source dir, find there spec file
        // or install driver of package ...
        return true;
    case InputType::SpecificationFile:
    {
        auto &fes = Build::getAvailableFrontendConfigFilenames();
        return std::find(fes.begin(), fes.end(), i.getPath().filename().u8string()) != fes.end();
    }
    case InputType::InlineSpecification:
        SW_UNIMPLEMENTED;
        break;
    case InputType::DirectorySpecificationFile:
        return !!findConfig(i.getPath(), Build::getAvailableFrontendConfigFilenames());
    case InputType::Directory:
        SW_UNIMPLEMENTED;
        break;
    default:
        SW_UNREACHABLE;
    }
    return false;
}

void Driver::load(const Input &i)
{
    switch (i.getType())
    {
    case InputType::DirectorySpecificationFile:
    {
        auto p = *findConfig(i.getPath(), Build::getAvailableFrontendConfigFilenames());
    }
    default:
        SW_UNREACHABLE;
    }

    /*auto b = std::make_unique<Build>(swctx);
    b->Local = true;
    b->setSourceDirectory(fs::is_directory(p) ? p : p.parent_path());
    b->load(p, true);
    return b;*/

    SW_UNIMPLEMENTED;

    /*auto b = std::make_unique<Build>(swctx);
    b->Local = true;
    b->setSourceDirectory(f.value().parent_path());
    b->load(f.value());
    return b;*/
}

}
