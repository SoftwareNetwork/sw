// Copyright (C) 2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <cppan_version.h>

#include <set>
#include <string>

namespace sw
{

// see https://opensource.org/licenses
enum class LicenseType
{
    // open source types
    UnspecifiedOpenSource = 1,

    // append only
    AUnspecifiedOpenSource = 11'000,
    AAL,
    AFL_3_0,
    AGPL_3_0,
    APL_1_0,
    APSL_2_0,
    Apache_2_0,
    Artistic_2_0,

    // append only
    BUnspecifiedOpenSource = 12'000,
    BSD_2_Clause,
    BSD_3_Clause,
    BSD_Patent,
    BSL_1_0,

    // append only
    CUnspecifiedOpenSource = 13'000,
    CATOSL_1_1,
    CDDL_1_0,
    CECILL_2_1,
    CNRI_Python,
    CPAL_1_0,
    CUA_OPL_1_0,

    // append only
    DUnspecifiedOpenSource = 14'000,

    // append only
    EUnspecifiedOpenSource = 15'000,
    ECL_2_0,
    EFL_2_0,
    EPL_1_0,
    EUDatagrid,
    EUPL_1_1,
    Entessa,
    eCos_2_0,

    // append only
    FUnspecifiedOpenSource = 16'000,
    FPL_1_0_0,
    Fair,
    Frameworx_1_0,

    // append only
    GUnspecifiedOpenSource = 17'000,
    GPL_2_0,
    GPL_3_0,

    // append only
    HUnspecifiedOpenSource = 18'000,
    HPND,

    // append only
    IUnspecifiedOpenSource = 19'000,
    IPA,
    IPL_1_0,
    ISC,

    // append only
    JUnspecifiedOpenSource = 20'000,

    // append only
    KUnspecifiedOpenSource = 21'000,

    // append only
    LUnspecifiedOpenSource = 22'000,
    LGPL_2_0,
    LGPL_2_1,
    LGPL_3_0,
    LPL_1_02,
    LPPL_1_3_c,
    LiLiQ_P_1_1,
    LiLiQ_R_1_1,
    LiLiQ_R_plus_1_1,

    // append only
    MUnspecifiedOpenSource = 23'000,
    MIT,
    MPL_1_0,
    MPL_1_1,
    MPL_2_0,
    MS_PL,
    MS_RL,
    MirOS,
    Motosoto,
    Multics,

    // append only
    NUnspecifiedOpenSource = 24'000,
    NASA_1_3,
    NCSA,
    NGPL,
    NPOSL_3_0,
    NTP,
    Naumen,
    Nokia,

    // append only
    OUnspecifiedOpenSource = 25'000,
    OCLC_2_0,
    OFL_1_1,
    OGTSL,
    OSET_2_1,
    OSL_3_0,

    // append only
    PUnspecifiedOpenSource = 26'000,
    PHP_3_0,
    PostgreSQL,
    Python_2_0,

    // append only
    QUnspecifiedOpenSource = 27'000,
    QPL_1_0,

    // append only
    RUnspecifiedOpenSource = 28'000,
    RPL_1_5,
    RPSL_1_0,
    RSCPL,

    // append only
    SUnspecifiedOpenSource = 29'000,
    SPL_1_0,
    SimPL_2_0,
    Sleepycat,

    // append only
    TUnspecifiedOpenSource = 30'000,

    // append only
    UUnspecifiedOpenSource = 31'000,
    UCL_1_0,
    UPL,

    // append only
    VUnspecifiedOpenSource = 32'000,
    VSL_1_0,

    // append only
    WUnspecifiedOpenSource = 33'000,
    W3C,
    WXwindows,
    Watcom_1_0,

    // append only
    XUnspecifiedOpenSource = 34'000,
    Xnet,

    // append only
    YUnspecifiedOpenSource = 35'000,

    // append only
    ZUnspecifiedOpenSource = 36'000,
    ZPL_2_0,
    Zlib,

    // proprietary licenses
    // append only
    UnspecifiedProprietary = 1'000'000,
};

enum class LicensePropery
{
    Permissive,
    Copylefted,
    Copyleft_except_for_GNU_AGPL,
    Limited,
    WithRestrictions,
    Manually,
    Yes,
    No,
    PublicDomain,
    GPL_v3_only,
    With_explicit_compatibility_list,
    Unknown,
};

struct SW_MANAGER_API License
{
    enum Approval
    {
        None = 0x00,
        Unknown = 0x01,
        FSF = 0x02,
        GPLv3 = 0x04,
        GPLv2 = 0x08,
        OSI = 0x10,
        Copyfree = 0x20,
        Debian = 0x40,
        Fedora = 0x80,
    };

    LicenseType Type;
    std::string Name;
    ::primitives::version::Version Version;
    std::string FullName;
    std::string Author;
    std::string PublicationDate;
    std::string Url;

    LicensePropery Linking = LicensePropery::No;
    LicensePropery Distribution = LicensePropery::No;
    LicensePropery Modification = LicensePropery::No;
    LicensePropery PatentGrant = LicensePropery::No;
    LicensePropery PrivateUse = LicensePropery::No;
    LicensePropery Sublicensing = LicensePropery::No;
    LicensePropery TrademarkGrant = LicensePropery::No;
    int ApprovalType = 0;
    bool Deprecated = false;
    bool Superseeded = false;

    bool operator<(const License &Rhs) const { return Type < Rhs.Type; }
    bool operator==(const License &Rhs) const { return Type == Rhs.Type; }
    bool operator==(const LicenseType &Rhs) const { return Type == Rhs; }

    static const License *get(LicenseType Type);
};

}
