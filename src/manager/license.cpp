// Copyright (C) 2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <license.h>

namespace sw
{

const License *License::get(LicenseType Type)
{
    // for more licenses see
    // https://en.wikipedia.org/wiki/Comparison_of_free_and_open-source_software_licenses
    static const std::set<License> Licenses{
        {
            LicenseType::UnspecifiedOpenSource,
            "Unspecified Open Source",
            {},
            "Unspecified Open Source License",
            "",
            "",
            "",
            LicensePropery::Permissive,
            LicensePropery::Permissive,
            LicensePropery::Permissive,
        },

        {
            LicenseType::UnspecifiedProprietary,
            "Unspecified Proprietary",
            {},
            "Unspecified Proprietary License",
        },

        // A

        {
            LicenseType::Apache_2_0,
            "Apache 2.0",
            { 2,0 },
            "Apache License 2.0",
            "Apache Software Foundation",
            "2004",
            "http://www.apache.org/licenses/LICENSE-2.0",
            LicensePropery::Permissive,
            LicensePropery::Permissive,
            LicensePropery::Permissive,
            LicensePropery::Yes,
            LicensePropery::Yes,
            LicensePropery::Permissive,
            LicensePropery::No,
            License::Approval::FSF | License::Approval::GPLv3 | License::Approval::OSI |
            License::Approval::Debian | License::Approval::Fedora,
        },

        {
            LicenseType::AGPL_3_0,
            "AGPL 3.0",
            { 3,0 },
            "GNU Affero General Public License 3.0",
            "Free Software Foundation",
            "19 November 2007",
            "https://www.gnu.org/licenses/agpl.html",
            LicensePropery::Copylefted,
            LicensePropery::Copyleft_except_for_GNU_AGPL,
            LicensePropery::Copylefted,
            LicensePropery::Unknown,
            LicensePropery::Yes,
            LicensePropery::Unknown,
            LicensePropery::Unknown,
            License::Approval::FSF | License::Approval::GPLv3,
        },

        // B

        {
            LicenseType::BSD_2_Clause,
            "BSD-2-Clause",
            { 1,0 },
            "BSD 2-clause License",
            "Regents of the University of California",
            "April 1999",
            "https://opensource.org/licenses/BSD-2-Clause",
            LicensePropery::Permissive,
            LicensePropery::Permissive,
            LicensePropery::Permissive,
            LicensePropery::Manually,
            LicensePropery::Yes,
            LicensePropery::Permissive,
            LicensePropery::Manually,
            License::Approval::FSF | License::Approval::GPLv3 | License::Approval::OSI |
            License::Approval::Copyfree | License::Approval::Debian | License::Approval::Fedora,
        },

        {
            LicenseType::BSD_3_Clause,
            "BSD-3-Clause",
            { 2,0 },
            "BSD 3-clause License",
            "Regents of the University of California",
            "22 July 1999",
            "https://opensource.org/licenses/BSD-3-Clause",
            LicensePropery::Permissive,
            LicensePropery::Permissive,
            LicensePropery::Permissive,
            LicensePropery::Manually,
            LicensePropery::Yes,
            LicensePropery::Permissive,
            LicensePropery::Manually,
            License::Approval::FSF | License::Approval::GPLv3 | License::Approval::OSI |
            License::Approval::Copyfree | License::Approval::Debian | License::Approval::Fedora,
        },

        {
            LicenseType::BSL_1_0,
            "BSL 1.0",
            { 1,0 },
            "Boost Software License 1.0",
            "",
            "17 August 2003",
            "http://www.boost.org/LICENSE_1_0.txt",
            LicensePropery::Permissive,
            LicensePropery::Unknown,
            LicensePropery::Permissive,
            LicensePropery::Unknown,
            LicensePropery::Unknown,
            LicensePropery::Unknown,
            LicensePropery::Unknown,
            License::Approval::FSF | License::Approval::GPLv3 | License::Approval::OSI |
            License::Approval::Copyfree | License::Approval::Debian | License::Approval::Fedora,
        },

        // G

        {
            LicenseType::GPL_2_0,
            "GPL 2.0",
            { 2,0 },
            "GNU General Public License 2.0",
            "Free Software Foundation",
            "June 1991",
            "https://www.gnu.org/licenses/old-licenses/gpl-2.0.html",
            LicensePropery::Copylefted,
            LicensePropery::Copylefted,
            LicensePropery::Copylefted,
            LicensePropery::Yes,
            LicensePropery::Yes,
            LicensePropery::Copylefted,
            LicensePropery::Yes,
            License::Approval::FSF | License::Approval::OSI |
            License::Approval::Debian | License::Approval::Fedora,
        },

        {
            LicenseType::GPL_3_0,
            "GPL 3.0",
            { 3,0 },
            "GNU General Public License 3.0",
            "Free Software Foundation",
            "29 June 2007",
            "https://www.gnu.org/licenses/gpl.html",
            LicensePropery::GPL_v3_only,
            LicensePropery::Copylefted,
            LicensePropery::Copylefted,
            LicensePropery::Yes,
            LicensePropery::Yes,
            LicensePropery::Copylefted,
            LicensePropery::Yes,
            License::Approval::FSF | License::Approval::GPLv3 | License::Approval::OSI |
            License::Approval::Debian | License::Approval::Fedora,
        },

        {
            LicenseType::LGPL_2_1,
            "LGPL 2.1",
            { 2,1 },
            "GNU Lesser General Public License 2.1",
            "Free Software Foundation",
            "February 1999",
            "https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html",
            LicensePropery::WithRestrictions,
            LicensePropery::Copylefted,
            LicensePropery::Copylefted,
            LicensePropery::Yes,
            LicensePropery::Yes,
            LicensePropery::Copylefted,
            LicensePropery::Yes,
            License::Approval::FSF | License::Approval::OSI |
            License::Approval::Debian | License::Approval::Fedora,
        },

        {
            LicenseType::LGPL_3_0,
            "LGPL 3.0",
            { 3,0 },
            "GNU Lesser General Public License 3.0",
            "Free Software Foundation",
            "29 June 2007",
            "https://www.gnu.org/licenses/lgpl.html",
            LicensePropery::WithRestrictions,
            LicensePropery::Copylefted,
            LicensePropery::Copylefted,
            LicensePropery::Yes,
            LicensePropery::Yes,
            LicensePropery::Copylefted,
            LicensePropery::Yes,
            License::Approval::FSF | License::Approval::GPLv3 | License::Approval::OSI |
            License::Approval::Debian | License::Approval::Fedora,
        },

        // M

        {
            LicenseType::MIT,
            "MIT",
            { 1,0 },
            "MIT License",
            "Massachusetts Institute of Technology",
            "1988",
            "https://opensource.org/licenses/MIT",
            LicensePropery::Permissive,
            LicensePropery::Permissive,
            LicensePropery::Permissive,
            LicensePropery::Manually,
            LicensePropery::Yes,
            LicensePropery::Permissive,
            LicensePropery::Manually,
            License::Approval::FSF | License::Approval::GPLv3 | License::Approval::OSI |
            License::Approval::Copyfree | License::Approval::Debian | License::Approval::Fedora,
        },

        // Z

        {
            LicenseType::Zlib,
            "Zlib",
            { 0,7 },
            "Zlib License",
            "Jean-Loup Gailly and Mark Adler",
            "1995-04-15",
            "https://opensource.org/licenses/Zlib",
            LicensePropery::Permissive,
            LicensePropery::Unknown,
            LicensePropery::Permissive,
            LicensePropery::Unknown,
            LicensePropery::Unknown,
            LicensePropery::Unknown,
            LicensePropery::Unknown,
            License::Approval::FSF | License::Approval::GPLv3 | License::Approval::OSI |
            License::Approval::Debian | License::Approval::Fedora,
        },
    };

    auto i = Licenses.find({ Type });
    if (i == Licenses.end())
        throw std::runtime_error("No such license");
    return &*i;
}

}
