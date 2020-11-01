// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "set_settings.h"

#include "detect.h"
#include "../build_settings.h"

namespace sw
{

template <class T>
static auto to_upkg(const T &s)
{
    return UnresolvedPackage(s).toString();
}

template <class K, class V>
static bool check_and_assign(K &k, const V &v, bool force = false)
{
    if (!k || force)
    {
        k = v;
        return true;
    }
    return false;
}

// actually we cannot move this to client,
// because we support different languages and packages
// scripting languages do not have os, arch, kernel, configuration etc.
static void addNativeSettings(TargetSettings &ts, bool force)
{
    check_and_assign(ts["native"]["configuration"], "release", force);
    check_and_assign(ts["native"]["library"], "shared", force);
    check_and_assign(ts["native"]["mt"], "false", force);
}

static void setRuleCompareRules(TargetSettings &ts)
{
    return;

    // mandatory rules
    for (auto &v : {"c","cpp","link"})
    {
        if (ts["rule"][v])
        {
            ts["rule"].ignoreInComparison(true);
            ts["rule"].useInHash(false);
        }
    }
}

static void addSettingsCommon(const SwCoreContext &swctx, TargetSettings &ts, bool force)
{
    addNativeSettings(ts, force);

    auto set_rule = [&ts, &force](auto &r, auto &s)
    {
        return check_and_assign(ts["rule"][r], s, force);
    };

    BuildSettings bs(ts);
    // on win we select msvc, clang, clangcl
    if (bs.TargetOS.is(OSType::Windows))
    {
        TargetSettings msvc;
        msvc["type"] = "msvc";

        if (0);
        // msvc
        else if (getProgramDetector().hasVsInstances())
        {
            msvc["package"] = "com.Microsoft.VisualStudio.VC.cl";
            set_rule("c", msvc);
            set_rule("cpp", msvc);

            msvc["package"] = "com.Microsoft.VisualStudio.VC.ml";
            set_rule("asm", msvc);
        }

        // use msvc's lib and link until llvm tools are not working
        msvc["package"] = "com.Microsoft.VisualStudio.VC.lib";
        set_rule("lib", msvc);
        msvc["package"] = "com.Microsoft.VisualStudio.VC.link";
        set_rule("link", msvc);

        // always use this rc
        ts["rule"]["rc"]["package"] = "com.Microsoft.Windows.rc";

        // libs
        check_and_assign(ts["native"]["stdlib"]["c"], to_upkg("com.Microsoft.Windows.SDK.ucrt"), force);
        auto cppset = check_and_assign(ts["native"]["stdlib"]["cpp"], to_upkg("com.Microsoft.VisualStudio.VC.libcpp"), force);
        if (cppset)
            check_and_assign(ts["native"]["stdlib"]["compiler"], to_upkg("com.Microsoft.VisualStudio.VC.runtime"), force);
        check_and_assign(ts["native"]["stdlib"]["kernel"], to_upkg("com.Microsoft.Windows.SDK.um"), force);

        UnresolvedPackage cppcl = ts["rule"]["cpp"]["package"].getValue();
        if (cppcl.getPath() == "com.Microsoft.VisualStudio.VC.cl")
        {
            // take same ver as cl
            {
                UnresolvedPackage up("com.Microsoft.VisualStudio.VC.libcpp");
                up.range = cppcl.range;
                check_and_assign(ts["native"]["stdlib"]["cpp"], to_upkg("com.Microsoft.VisualStudio.VC.libcpp"), force || cppset);
            }
            {
                UnresolvedPackage up("com.Microsoft.VisualStudio.VC.runtime");
                up.range = cppcl.range;
                check_and_assign(ts["native"]["stdlib"]["compiler"], to_upkg("com.Microsoft.VisualStudio.VC.runtime"), force || cppset);
            }
        }
    }
    else
        SW_UNIMPLEMENTED;

    setRuleCompareRules(ts);
}

// remember! only host tools
// TODO: load host settings from file
void addSettingsAndSetHostPrograms(const SwCoreContext &swctx, TargetSettings &ts)
{
    addSettingsCommon(swctx, ts, true);
}

void addSettingsAndSetPrograms(const SwCoreContext &swctx, TargetSettings &ts)
{
    addSettingsCommon(swctx, ts, false);
}

// they must be the same as used when building sw
void addSettingsAndSetConfigPrograms(const SwContext &swctx, TargetSettings &ts)
{
    ts["native"]["library"] = "static"; // why not shared?
                                        //ts["native"]["mt"] = "true";
    if (swctx.getSettings()["debug_configs"] == "true")
    {
#ifndef NDEBUG
        ts["native"]["configuration"] = "debug";
#else
        ts["native"]["configuration"] = "releasewithdebuginformation";
#endif
    }

#ifdef _MSC_VER
    Version clver(_MSC_VER / 100, _MSC_VER % 100);
#endif
}

void addSettingsAndSetHostPrograms1(const SwCoreContext &swctx, TargetSettings &ts)
{
    addNativeSettings(ts, true);
    return;

    if (swctx.getHostOs().Type == OSType::Windows)
    {
        check_and_assign(ts["native"]["stdlib"]["c"], to_upkg("com.Microsoft.Windows.SDK.ucrt"));
        check_and_assign(ts["native"]["stdlib"]["cpp"], to_upkg("com.Microsoft.VisualStudio.VC.libcpp"));
        check_and_assign(ts["native"]["stdlib"]["kernel"], to_upkg("com.Microsoft.Windows.SDK.um"));

        // now find the latest available sdk (ucrt) and select it
        //TargetSettings oss;
        //oss["os"] = ts["os"];
        //auto sdk = swctx.getPredefinedTargets().find(UnresolvedPackage(ts["native"]["stdlib"]["c"].getValue()), oss);
        //if (!sdk)
        //throw SW_RUNTIME_ERROR("No suitable installed WinSDK found for this host");
        //ts["native"]["stdlib"]["c"] = sdk->getPackage().toString(); // assign always
        //ts["os"]["version"] = sdkver->toString(3); // cut off the last (fourth) number

        auto clpkg = "com.Microsoft.VisualStudio.VC.cl";
        //auto cl = swctx.getPredefinedTargets().find(clpkg);

        auto clangpppkg = "org.LLVM.clangpp";
        //auto clangpp = swctx.getPredefinedTargets().find(clpkg);

        if (0);
#ifdef _MSC_VER
        // msvc + clangcl
        // clangcl must be compatible with msvc
        // and also clang actually
        else if (0/*cl != swctx.getPredefinedTargets().end(clpkg) && !cl->second.empty()*/)
        {
            check_and_assign(ts["native"]["program"]["c"], to_upkg("com.Microsoft.VisualStudio.VC.cl"));
            check_and_assign(ts["native"]["program"]["cpp"], to_upkg("com.Microsoft.VisualStudio.VC.cl"));
            check_and_assign(ts["native"]["program"]["asm"], to_upkg("com.Microsoft.VisualStudio.VC.ml"));
            check_and_assign(ts["native"]["program"]["lib"], to_upkg("com.Microsoft.VisualStudio.VC.lib"));
            check_and_assign(ts["native"]["program"]["link"], to_upkg("com.Microsoft.VisualStudio.VC.link"));
        }
        // separate?
#else __clang__
        else if (0/*clangpp != swctx.getPredefinedTargets().end(clangpppkg) && !clangpp->second.empty()*/)
        {
            check_and_assign(ts["native"]["program"]["c"], to_upkg("org.LLVM.clang"));
            check_and_assign(ts["native"]["program"]["cpp"], to_upkg("org.LLVM.clangpp"));
            check_and_assign(ts["native"]["program"]["asm"], to_upkg("org.LLVM.clang"));
            // ?
            check_and_assign(ts["native"]["program"]["lib"], to_upkg("com.Microsoft.VisualStudio.VC.lib"));
            check_and_assign(ts["native"]["program"]["link"], to_upkg("com.Microsoft.VisualStudio.VC.link"));
        }
#endif
        // add more defaults (clangcl, clang)
        //else
        //throw SW_RUNTIME_ERROR("Seems like you do not have Visual Studio installed.\nPlease, install the latest Visual Studio first.");
    }
    // add more defaults
    else
    {
        // set default libs?
        /*ts["native"]["stdlib"]["c"] = to_upkg("com.Microsoft.Windows.SDK.ucrt");
        ts["native"]["stdlib"]["cpp"] = to_upkg("com.Microsoft.VisualStudio.VC.libcpp");
        ts["native"]["stdlib"]["kernel"] = to_upkg("com.Microsoft.Windows.SDK.um");*/

        auto if_add = [&swctx](auto &s, const UnresolvedPackage &name)
        {
            check_and_assign(s, name.toString());
            return true;
        };

        auto err_msg = [](const String &cl)
        {
            return "sw was built with " + cl + " as compiler, but it was not found in your system. Install " + cl + " to proceed.";
        };

        // must be the same compiler as current!
#if defined(__clang__)
        bool ok = false;
        if (!(
            if_add(ts["native"]["program"]["c"], "com.Apple.clang"s) &&
            if_add(ts["native"]["program"]["cpp"], "com.Apple.clangpp"s)
            ))
        {
            // no error, we want to check second condition
            //throw SW_RUNTIME_ERROR(err_msg("appleclang"));
        }
        else
            ok = true;
        if (!ok && !(
            if_add(ts["native"]["program"]["c"], "org.LLVM.clang"s) &&
            if_add(ts["native"]["program"]["cpp"], "org.LLVM.clangpp"s)
            ))
        {
            throw SW_RUNTIME_ERROR(err_msg("clang"));
        }
        //if (getHostOs().is(OSType::Linux))
        //ts["native"]["stdlib"]["cpp"] = to_upkg("org.sw.demo.llvm_project.libcxx");
#elif defined(__GNUC__) || defined(__CYGWIN__)
        if (!(
            if_add(ts["native"]["program"]["c"], "org.gnu.gcc"s) &&
            if_add(ts["native"]["program"]["cpp"], "org.gnu.gpp"s)
            ))
        {
            throw SW_RUNTIME_ERROR(err_msg("gcc"));
        }
#elif defined(_WIN32)
#else
#error "Add your current compiler to detect.cpp and here."
#endif

        // using c prog
        if (ts["native"]["program"]["c"].isValue())
            if_add(ts["native"]["program"]["asm"], ts["native"]["program"]["c"].getValue());

        // reconsider, also with driver?
        check_and_assign(ts["native"]["program"]["lib"], "org.gnu.binutils.ar"s);

        // use driver
        // use cpp driver for the moment to not burden ourselves in adding stdlib
        if (ts["native"]["program"]["cpp"].isValue())
            if_add(ts["native"]["program"]["link"], ts["native"]["program"]["cpp"].getValue());
    }
}

//
void addSettingsAndSetPrograms1(const SwCoreContext &swctx, TargetSettings &ts)
{
    addNativeSettings(ts, false);

    BuildSettings bs(ts);
    // on win we select msvc, clang, clangcl
    if (bs.TargetOS.is(OSType::Windows))
    {
        //if (!ts["native"]["program"]["c"] || ts["native"]["program"]["c"].isValue())
        String sver;
        if (bs.TargetOS.Version)
            sver = "-" + bs.TargetOS.Version->toString();
        //check_and_assign(ts["native"]["stdlib"]["c"], to_upkg("com.Microsoft.Windows.SDK.ucrt" + sver));
        //check_and_assign(ts["native"]["stdlib"]["cpp"], to_upkg("com.Microsoft.VisualStudio.VC.libcpp"));
        //check_and_assign(ts["native"]["stdlib"]["kernel"], to_upkg("com.Microsoft.Windows.SDK.um" + sver));

        // now find the latest available sdk (ucrt) and select it
        //TargetSettings oss;
        //oss["os"] = ts["os"];
        //auto sdk = swctx.getPredefinedTargets().find(UnresolvedPackage(ts["native"]["stdlib"]["c"].getValue()), oss);
        //if (!sdk)
        //throw SW_RUNTIME_ERROR("No suitable installed WinSDK found for this host");
        //ts["native"]["stdlib"]["c"] = sdk->getPackage().toString(); // assign always
        //ts["os"]["version"] = sdkver->toString(3); // cut off the last (fourth) number

        auto clpkg = "com.Microsoft.VisualStudio.VC.cl";
        //auto cl = swctx.getPredefinedTargets().find(clpkg);

        auto clangpppkg = "org.LLVM.clangpp";
        //auto clangpp = swctx.getPredefinedTargets().find(clpkg);

        auto clangclpkg = "org.LLVM.clangcl";
        //auto clangcl = swctx.getPredefinedTargets().find(clangclpkg);

        if (0);
        // msvc
        else if (getProgramDetector().hasVsInstances())
        {
            ts["rule"]["c"]["package"] = "com.Microsoft.VisualStudio.VC.cl";
            ts["rule"]["c"]["type"] = "msvc";

            ts["rule"]["cpp"]["package"] = "com.Microsoft.VisualStudio.VC.cl";
            ts["rule"]["cpp"]["type"] = "msvc";

            ts["rule"]["asm"]["package"] = "com.Microsoft.VisualStudio.VC.ml";
            ts["rule"]["asm"]["type"] = "msvc";
        }
        // clang
        else if (0/*clangpp != swctx.getPredefinedTargets().end(clangpppkg) && !clangpp->second.empty()*/)
        {
            SW_UNIMPLEMENTED;
            check_and_assign(ts["native"]["program"]["c"], to_upkg("org.LLVM.clang"));
            check_and_assign(ts["native"]["program"]["cpp"], to_upkg("org.LLVM.clangpp"));
            check_and_assign(ts["native"]["program"]["asm"], to_upkg("org.LLVM.clang"));
            // ?
            check_and_assign(ts["native"]["program"]["lib"], to_upkg("com.Microsoft.VisualStudio.VC.lib"));
            check_and_assign(ts["native"]["program"]["link"], to_upkg("com.Microsoft.VisualStudio.VC.link"));
        }
        // clangcl
        else if (0/*clangcl != swctx.getPredefinedTargets().end(clangclpkg) && !clangcl->second.empty()*/)
        {
            SW_UNIMPLEMENTED;
            check_and_assign(ts["native"]["program"]["c"], to_upkg("org.LLVM.clangcl"));
            check_and_assign(ts["native"]["program"]["cpp"], to_upkg("org.LLVM.clangcl"));
            check_and_assign(ts["native"]["program"]["asm"], to_upkg("org.LLVM.clangcl"));
            // ?
            check_and_assign(ts["native"]["program"]["lib"], to_upkg("com.Microsoft.VisualStudio.VC.lib"));
            check_and_assign(ts["native"]["program"]["link"], to_upkg("com.Microsoft.VisualStudio.VC.link"));
        }
        //else
        //throw SW_RUNTIME_ERROR("No suitable compilers found.\nPlease, install one first.");

        // use msvc's lib and link until llvm tools are not working
        ts["rule"]["lib"]["package"] = "com.Microsoft.VisualStudio.VC.lib";
        ts["rule"]["lib"]["type"] = "msvc";

        ts["rule"]["link"]["package"] = "com.Microsoft.VisualStudio.VC.link";
        ts["rule"]["link"]["type"] = "msvc";

        // always use this rc
        ts["rule"]["rc"]["package"] = "com.Microsoft.Windows.rc";
    }
    // add more defaults
    else
    {
        // set default libs?
        /*ts["native"]["stdlib"]["c"] = to_upkg("com.Microsoft.Windows.SDK.ucrt");
        ts["native"]["stdlib"]["cpp"] = to_upkg("com.Microsoft.VisualStudio.VC.libcpp");
        ts["native"]["stdlib"]["kernel"] = to_upkg("com.Microsoft.Windows.SDK.um");*/

        auto if_add = [&swctx](auto &s, const UnresolvedPackage &name)
        {
            /*auto &pd = swctx.getPredefinedTargets();
            auto i = pd.find(name);
            if (i == pd.end() || i->second.empty())
            return false;*/
            check_and_assign(s, name.toString());
            return true;
        };

        auto try_clang = [&if_add, &ts]()
        {
            if_add(ts["native"]["program"]["c"], "org.LLVM.clang"s);
            if_add(ts["native"]["program"]["cpp"], "org.LLVM.clangpp"s);
            if_add(ts["native"]["program"]["c"], "com.Apple.clang"s);
            if_add(ts["native"]["program"]["cpp"], "com.Apple.clangpp"s);
            //if (getHostOs().is(OSType::Linux))
            //ts["native"]["stdlib"]["cpp"] = to_upkg("org.sw.demo.llvm_project.libcxx");
        };

        auto try_gcc = [&if_add, &ts]()
        {
            if_add(ts["native"]["program"]["c"], "org.gnu.gcc"s);
            if_add(ts["native"]["program"]["cpp"], "org.gnu.gpp"s);
        };

        if (bs.TargetOS.is(OSType::Mingw))
            try_gcc();

        try_clang();
        try_gcc();

        // using c prog
        if (ts["native"]["program"]["c"].isValue())
            if_add(ts["native"]["program"]["asm"], ts["native"]["program"]["c"].getValue());

        // reconsider, also with driver?
        check_and_assign(ts["native"]["program"]["lib"], "org.gnu.binutils.ar"s);

        // use driver
        // use cpp driver for the moment to not burden ourselves in adding stdlib
        if (ts["native"]["program"]["cpp"].isValue())
            if_add(ts["native"]["program"]["link"], ts["native"]["program"]["cpp"].getValue());
    }

    setRuleCompareRules(ts);
}

}
