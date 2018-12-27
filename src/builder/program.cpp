// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "program.h"

#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/lock_types.hpp>

namespace sw
{

Version Program::getVersion() const
{
    if (version)
        return version.value();

    if (file.empty())
    {
        version = gatherVersion();
        return version.value();
    }

    static std::unordered_map<path, Version> versions;
    static boost::upgrade_mutex m;

    boost::upgrade_lock lk(m);
    auto i = versions.find(file);
    if (i != versions.end())
    {
        version = i->second;
        return version.value();
    }

    boost::upgrade_to_unique_lock lk2(lk);

    if (version) // double check
        return version.value();

    version = versions[file] = gatherVersion();
    return version.value();
}

}
