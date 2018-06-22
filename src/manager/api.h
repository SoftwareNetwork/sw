// Copyright (C) 2016-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "cppan_version.h"
#include "enums.h"

namespace sw
{

struct PackagePath;
struct Remote;

struct Api
{
    void add_project(const Remote &r, PackagePath p);
    void remove_project(const Remote &r, PackagePath p);
    void add_version(const Remote &r, PackagePath p, const String &cppan);
    void add_version(const Remote &r, PackagePath p, const Version &vnew);
    void add_version(const Remote &r, PackagePath p, const Version &vnew, const String &vold);
    void update_version(const Remote &r, PackagePath p, const Version &v);
    void remove_version(const Remote &r, PackagePath p, const Version &v);
    void get_notifications(const Remote &r, int n = 10);
    void clear_notifications(const Remote &r);
};

}
