// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef SW_PACKAGE_API
#define SW_PACKAGE_API
#endif

#include <sw/driver/cpp/sw.h>

#include <primitives/context.h>

#include <boost/algorithm/string.hpp>
#include <directories.h>

std::unordered_map<String, NativeExecutedTarget*> boost_targets;

UnresolvedPackages pkgs;

struct pkg_map
{
    pkg_map()
    {
        std::ifstream ifile(getDirectories().storage_dir_etc / "self.txt");
        if (!ifile)
            return;
        while (1)
        {
            String k, v;
            ifile >> k;
            if (!ifile)
                break;
            ifile >> v;
            m[k] = v;
        }
    }

    ~pkg_map()
    {
        std::ofstream ofile(getDirectories().storage_dir_etc / "self.txt");
        for (auto &[k, v] : m)
            ofile << k << " " << v << "\n";
    }

    std::map<String, String> m;
};

// returns real version
std::tuple<path, Version> getDirSrc(const String &p)
{
    static pkg_map m;

    auto i = m.m.find(p);
    if (i != m.m.end())
    {
        PackageId real_pkg(i->second);
        auto d = real_pkg.getDirSrc();
        if (fs::exists(d))
            return { d, real_pkg.getVersion() };
    }

    auto pkg = extractFromString(p);
    auto real_pkg = resolve_dependencies({ pkg })[pkg];

    auto d = real_pkg.getDirSrc();
    if (!fs::exists(d))
        throw std::runtime_error("Cannot resolve dep: " + p);
    m.m[p] = real_pkg.toString();
    return { d, real_pkg.getVersion() };
}

static void resolve()
{
    resolveAllDependencies(pkgs);
}

template <class T>
auto &addTarget(Solution &s, const PackagePath &p, const String &v)
{
    auto &t = s.TargetBase::addTarget<T>(p, v);
    auto [d, v2] = getDirSrc(p.toString() + "-" + v);
    t.SourceDir = d;
    t.pkg.version = v2;
    t.pkg.createNames();
    t.init();
    return t;
}

template <class T>
auto &addBoostTarget(Solution &s, const String &name)
{
    static PackagePath b = "pvt.cppan.demo.boost";
    return addTarget<T>(s, b / name, "1.67.0");
}

void addPrivateDefinitions(TargetOptionsGroup &t, const String &N)
{
    DefinitionsType defs;
    defs["BOOST_" + N + "_BUILDING_THE_LIB"];
    defs["BOOST_" + N + "_SOURCE"];
    t.Private += defs;
}

void addStaticDefinitions(TargetOptionsGroup &t, const String &N)
{
    DefinitionsType defs;
    defs["BOOST_" + N + "_BUILD_LIB"];
    defs["BOOST_" + N + "_STATIC_LINK"];
    t.Public << sw::Static << defs;
}

void addSharedDefinitions(TargetOptionsGroup &t, const String &N)
{
    DefinitionsType defs2;
    defs2["BOOST_" + N + "_BUILD_DLL"];
    t.Private << sw::Shared << defs2;

    DefinitionsType defs;
    defs["BOOST_" + N + "_DYN_LINK"];
    defs["BOOST_" + N + "_USE_DLL"];
    t.Public << sw::Shared << defs;
}

auto &addCompiledBoostTarget(Solution &s, const String &name)
{
    auto &t = addBoostTarget<LibraryTarget>(s, name);
    auto N = boost::to_upper_copy(name);
    addPrivateDefinitions(t, N);
    addSharedDefinitions(t, N);
    addStaticDefinitions(t, N);
    return t;
}

auto &addStaticOnlyCompiledBoostTarget(Solution &s, const String &name)
{
    auto &t = addBoostTarget<StaticLibraryTarget>(s, name);
    auto N = boost::to_upper_copy(name);
    addPrivateDefinitions(t, N);
    addStaticDefinitions(t, N);
    return t;
}

#include "boost_deps.h"

void post_sources()
{
    boost_targets["config"]->fileWriteOnce("include/boost/config/auto_link.hpp",
        R"(
#ifdef BOOST_LIB_PREFIX
#  undef BOOST_LIB_PREFIX
#endif
#if defined(BOOST_LIB_NAME)
#  undef BOOST_LIB_NAME
#endif
#if defined(BOOST_LIB_THREAD_OPT)
#  undef BOOST_LIB_THREAD_OPT
#endif
#if defined(BOOST_LIB_RT_OPT)
#  undef BOOST_LIB_RT_OPT
#endif
#if defined(BOOST_LIB_LINK_OPT)
#  undef BOOST_LIB_LINK_OPT
#endif
#if defined(BOOST_LIB_DEBUG_OPT)
#  undef BOOST_LIB_DEBUG_OPT
#endif
#if defined(BOOST_DYN_LINK)
#  undef BOOST_DYN_LINK
#endif
)"
);

    boost_targets["config"]->replaceInFileOnce("include/boost/config/compiler/visualc.hpp",
        "#if (_MSC_VER > 1910)",
        "#if (_MSC_VER > 1911)");
}

void build_boost(Solution &s)
{
    auto header_only_target_names = { "accumulators","algorithm","align","any","array","asio","assert","assign","beast","bimap","bind","callable_traits","circular_buffer","compatibility","compute","concept_check","config","container_hash","conversion","convert","core","coroutine2","crc","detail","disjoint_sets","dll","dynamic_bitset","endian","flyweight","foreach","format","function","function_types","functional","fusion","geometry","gil","hana","heap","hof","icl","integer","interprocess","intrusive","io","iterator","lambda","lexical_cast","local_function","lockfree","logic","metaparse","move","mp11","mpl","msm","multi_array","multi_index","multiprecision","numeric","interval","odeint","ublas","optional","parameter","phoenix","poly_collection","polygon","pool","predef","preprocessor","process","property_map","property_tree","proto","ptr_container","qvm","range","ratio","rational","scope_exit","signals2","smart_ptr","sort","spirit","statechart","static_assert","throw_exception","tokenizer","tti","tuple","type_index","type_traits","typeof","units","unordered","utility","uuid","variant","vmd","winapi","xpressive", };
    for (auto &t : header_only_target_names)
    {
        auto &tgt = addBoostTarget<LibraryTarget>(s, t);
        tgt.HeaderOnly = true;
        boost_targets[t] = &tgt;
    }

    // some settings
    *boost_targets["function"] += "include/.*\\.hpp"_rr;
    *boost_targets["pool"] += "include/.*\\.[ih]pp"_rr;
    *boost_targets["spirit"] += "include/.*\\.[cih]pp"_rr;

    // compiled
    auto compiled_target_names = { "atomic","chrono","container","date_time","filesystem","graph",
        "iostreams","locale","log","math","program_options","random","regex","serialization","signals",
        "stacktrace","system","thread","timer","type_erasure","wave", };
    // "mpi","python","graph_parallel",
    // "coroutine","fiber","context",
    // "test",
    for (auto &t : compiled_target_names)
        boost_targets[t] = &addCompiledBoostTarget(s, t);
    boost_targets["exception"] = &addStaticOnlyCompiledBoostTarget(s, "exception");

    // some settings
    *boost_targets["container"] -= "src/dlmalloc.*\\.c"_rr;
    *boost_targets["iostreams"] -= "src/lzma.cpp";
    boost_targets["stacktrace"]->HeaderOnly = true;
    if (s.Settings.TargetOS.Type == OSType::Windows)
        boost_targets["random"]->LinkLibraries.insert("Advapi32.lib");

    if (s.Settings.TargetOS.Type == OSType::Windows)
        boost_targets["uuid"]->Public.LinkLibraries.insert("Bcrypt.lib");

    boost_targets["math"]->Private.IncludeDirectories.insert(boost_targets["math"]->SourceDir / "src/tr1");
    boost_targets["math"]->Private.IncludeDirectories.insert(boost_targets["math"]->SourceDir / "src");
    boost_targets["math"]->Public.IncludeDirectories.insert(boost_targets["math"]->SourceDir / "include");
    ((LibraryTarget*)boost_targets["math"])->Public << sw::Shared << "BOOST_MATH_TR1_DYN_LINK"_d;

    if (s.Settings.TargetOS.Type == OSType::Windows)
    {
        *boost_targets["locale"] -= "src/icu/.*"_rr;
        *boost_targets["locale"] -= "src/posix/.*"_rr;
        boost_targets["locale"]->Public.Definitions["BOOST_LOCALE_NO_POSIX_BACKEND"];
    }
    else
    {
        *boost_targets["locale"] -= "src/win32/.*"_rr;
        boost_targets["locale"]->Public.Definitions["BOOST_LOCALE_NO_WINAPI_BACKEND"];
        boost_targets["locale"]->Public.Definitions["BOOST_LOCALE_WITH_ICONV"];
    }

    *boost_targets["log"] +=
        "include/.*"_rr,
        "src/.*\\.mc"_rr,
        "src/.*\\.hpp"_rr,
        "src/attribute_name.cpp",
        "src/attribute_set.cpp",
        "src/attribute_value_set.cpp",
        "src/code_conversion.cpp",
        "src/core.cpp",
        "src/date_time_format_parser.cpp",
        //"src/debug_output_backend.cpp",
        "src/default_attribute_names.cpp",
        "src/default_sink.cpp",
        "src/dump.cpp",
        "src/event.cpp",
        "src/exceptions.cpp",
        "src/format_parser.cpp",
        "src/global_logger_storage.cpp",
        //"src/light_rw_mutex.cpp",
        "src/named_scope.cpp",
        "src/named_scope_format_parser.cpp",
        "src/once_block.cpp",
        "src/process_id.cpp",
        "src/process_name.cpp",
        "src/record_ostream.cpp",
        "src/severity_level.cpp",
        "src/spirit_encoding.cpp",
        "src/syslog_backend.cpp",
        "src/text_file_backend.cpp",
        "src/text_multifile_backend.cpp",
        "src/text_ostream_backend.cpp",
        "src/thread_id.cpp",
        "src/threadsafe_queue.cpp",
        "src/thread_specific.cpp",
        "src/timer.cpp",
        "src/timestamp.cpp",
        "src/trivial.cpp",
        "src/unhandled_exception_count.cpp"
        ;
    boost_targets["log"]->Public.Definitions["BOOST_LOG_WITHOUT_EVENT_LOG"];
    boost_targets["log"]->Private += sw::Shared, "BOOST_LOG_DLL"_d;
    if (s.Settings.TargetOS.Type == OSType::Windows)
    {
        boost_targets["log"]->Public.Definitions["WIN32_LEAN_AND_MEAN"];
        boost_targets["log"]->Public.Definitions["BOOST_USE_WINDOWS_H"];
        boost_targets["log"]->Public.Definitions["NOMINMAX"];
    }
    else
        *boost_targets["log"] -= "src/debug_output_backend.cpp", "src/light_rw_mutex.cpp";

    //((LibraryTarget*)boost_targets["fiber"])->Shared.Private.Definitions["BOOST_FIBERS_SOURCE"];
    //boost_targets["fiber"]->Private.Definitions["BOOST_FIBERS_DYN_LINK"];

    *boost_targets["signals"] += "include/.*\\.[ih]pp"_rr;

    //*boost_targets["python"] += "pvt.cppan.demo.python.libcompat";

    *((LibraryTarget*)boost_targets["thread"]) -= "src/pthread/once_atomic.cpp";
    if (s.Settings.TargetOS.Type == OSType::Windows)
    {
        *((LibraryTarget*)boost_targets["thread"]) -= "src/pthread/.*"_rr;
        *((LibraryTarget*)boost_targets["thread"]) >> sw::Shared >> "src/win32/tss_pe.cpp";
        *((LibraryTarget*)boost_targets["thread"]) >> sw::Static >> "src/win32/tss_dll.cpp";
    }
    else
        *((LibraryTarget*)boost_targets["thread"]) -= "src/win32/.*"_rr;
    //*boost_targets["thread"] += *boost_targets["date_time"];

    boost_deps();
    post_sources();
}

#include <build_self.generated.h>

void build_other(Solution &s)
{
    build_self_generated(s);

    auto &zlib = s.getTarget<LibraryTarget>("org.sw.demo.madler.zlib");
    auto &bzip2 = s.getTarget<LibraryTarget>("org.sw.demo.bzip2");
    auto &sqlite3 = s.getTarget<LibraryTarget>("org.sw.demo.sqlite3");

    *boost_targets["iostreams"] += bzip2, zlib;

    auto &yaml_cpp = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.jbeder.yaml_cpp", "master");
    yaml_cpp.Private << sw::Shared << "yaml_cpp_EXPORTS"_d;
    yaml_cpp.Public << sw::Shared << "YAML_CPP_DLL"_d;
    yaml_cpp.Public += *boost_targets["smart_ptr"], *boost_targets["iterator"];

    //
    auto &lz4 = addTarget<LibraryTarget>(s, "pvt.cppan.demo.lz4", "1");
    {
        lz4 += "lib/lz4.c",
            "lib/lz4.h",
            "lib/lz4frame.c",
            "lib/lz4frame.h",
            "lib/lz4frame_static.h",
            "lib/lz4hc.c",
            "lib/lz4hc.h",
            "lib/lz4opt.h",
            "lib/xxhash.c",
            "lib/xxhash.h";
        if (s.Settings.TargetOS.Type == OSType::Windows)
        {
            lz4.Private << sw::Shared << "LZ4_DLL_IMPORT"_d;
            lz4.Interface << sw::Shared << "LZ4_DLL_EXPORT"_d;
        }
    }

    auto &lzo = addTarget<LibraryTarget>(s, "pvt.cppan.demo.oberhumer.lzo.lzo", "2");
    lzo.ApiName = "__LZO_EXPORT1";

    //
    auto &libcharset = addTarget<LibraryTarget>(s, "pvt.cppan.demo.gnu.iconv.libcharset", "1");
    {
        libcharset.setChecks("libcharset");
        libcharset += "libcharset/include/localcharset.h.build.in", "libcharset/lib/localcharset.c";
        libcharset += "HAVE_VISIBILITY=0"_v;
        libcharset.ApiName = "LIBCHARSET_DLL_EXPORTED";
        libcharset.fileWriteOnce(libcharset.BinaryPrivateDir / "config.h", "", true);
        libcharset.replaceInFileOnce("libcharset/include/localcharset.h.build.in",
            "#define LIBCHARSET_DLL_EXPORTED",
            "//#define LIBCHARSET_DLL_EXPORTED");
        libcharset.configureFile(libcharset.SourceDir / "libcharset/include/localcharset.h.build.in", libcharset.BinaryDir / "localcharset.h");

        if (s.Settings.TargetOS.Type == OSType::Windows)
        {
            libcharset.Public.Definitions["LIBDIR"] = "\".\"";
            libcharset.Public.Definitions["LOCALEDIR"] = "\"./locale\"";
        }
        else
        {
            libcharset.Public.Definitions["HAVE_WORKING_O_NOFOLLOW"];
            libcharset.Public.Definitions["LIBDIR"] = "\"/usr/local/lib\"";
            libcharset.Public.Definitions["LOCALEDIR"] = "\"/usr/share/locale\"";
        }

        if (libcharset.Variables["HAVE_LANGINFO_H"] && libcharset.Variables["HAVE_NL_LANGINFO"])
            libcharset.Public.Definitions["HAVE_LANGINFO_CODESET"];
    }

    //
    auto &libiconv = addTarget<LibraryTarget>(s, "pvt.cppan.demo.gnu.iconv.libiconv", "1");
    {
        libiconv.setChecks("libiconv");
        libiconv += "include/iconv.h.build.in", "lib/.*\\.h"_rr, "lib/iconv.c";
        libiconv.Public += libcharset;
        libiconv.Public.Definitions["ICONV_CONST"] = "const";
        libiconv += "HAVE_VISIBILITY=0"_v;
        libiconv += "USE_MBSTATE_T=0"_v;
        libiconv += "BROKEN_WCHAR_H=0"_v;
        libiconv += "ICONV_CONST=const"_v;
        libiconv.ApiName = "LIBICONV_DLL_EXPORTED";
        libiconv.replaceInFileOnce("include/iconv.h.build.in",
            "#define LIBICONV_DLL_EXPORTED",
            "//#define LIBICONV_DLL_EXPORTED");
        libiconv.replaceInFileOnce("include/iconv.h.build.in",
            "dummy1[28]",
            "dummy1[40]");
        libiconv.configureFile(libiconv.SourceDir / "include/iconv.h.build.in", libiconv.BinaryDir / "iconv.h");
        libiconv.fileWriteOnce(libiconv.BinaryPrivateDir / "config.h",
            R"(
#define ENABLE_NLS 1
#define WORDS_LITTLEENDIAN ( ! ( WORDS_BIGENDIAN ) )

/* The _Noreturn keyword of draft C1X.  */
#ifndef _Noreturn
# if (3 <= __GNUC__ || (__GNUC__ == 2 && 8 <= __GNUC_MINOR__) \
        || 0x5110 <= __SUNPRO_C)
#  define _Noreturn __attribute__ ((__noreturn__))
# elif 1200 <= _MSC_VER
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn
# endif
#endif



/* Define to the equivalent of the C99 'restrict' keyword, or to
    nothing if this is not supported.  Do not define if restrict is
    supported directly.  */
#define restrict __restrict
/* Work around a bug in Sun C++: it does not support _Restrict or
    __restrict__, even though the corresponding Sun C compiler ends up with
    \"#define restrict _Restrict\" or \"#define restrict __restrict__\" in the
    previous line.  Perhaps some future version of Sun C++ will work with
    restrict; if so, hopefully it defines __RESTRICT like Sun C does.  */
#if defined __SUNPRO_CC && !defined __RESTRICT
# define _Restrict
# define __restrict__
#endif

/* Define as a marker that can be attached to declarations that might not
    be used.  This helps to reduce warnings, such as from
    GCC -Wunused-parameter.  */
#if __GNUC__ >= 3 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 7)
# define _GL_UNUSED __attribute__ ((__unused__))
#else
# define _GL_UNUSED
#endif
/* The name _UNUSED_PARAMETER_ is an earlier spelling, although the name
    is a misnomer outside of parameter lists.  */
#define _UNUSED_PARAMETER_ _GL_UNUSED

/* The __pure__ attribute was added in gcc 2.96.  */
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 96)
# define _GL_ATTRIBUTE_PURE __attribute__ ((__pure__))
#else
# define _GL_ATTRIBUTE_PURE /* empty */
#endif

/* The __const__ attribute was added in gcc 2.95.  */
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 95)
# define _GL_ATTRIBUTE_CONST __attribute__ ((__const__))
#else
# define _GL_ATTRIBUTE_CONST /* empty */
#endif

/* On Windows, variables that may be in a DLL must be marked specially.  */
//#if defined _MSC_VER && defined _DLL
//# define DLL_VARIABLE __declspec (dllimport)
//#else
//# define DLL_VARIABLE
//#endif
)",
true
);
    }

    auto &intl = addTarget<LibraryTarget>(s, "pvt.cppan.demo.gnu.gettext.intl", "0");
    {
        intl.setChecks("intl");
        intl +=
            "gettext-runtime/intl/.*\\.c"_rr,
            "gettext-runtime/intl/.*\\.h"_rr;

        intl -=
            "gettext-runtime/intl/intl-exports.c",
            "gettext-runtime/intl/localcharset.c",
            "gettext-runtime/intl/os2compat.c",
            "gettext-runtime/intl/relocatable.c";

        intl.Public += libiconv;

        intl.Private += "IN_LIBINTL"_d;
        if (s.Settings.TargetOS.Type != OSType::Windows)
        {
            intl.Public += "LOCALE_ALIAS_PATH=\"/etc/locale.alias\""_d;
        }

        if (s.Settings.TargetOS.Type == OSType::Windows)
        {
            intl += "HAVE_VISIBILITY=0"_v;
            intl += "HAVE_POSIX_PRINTF=0"_v;
        }
        else
        {
            intl += "HAVE_VISIBILITY=1"_v;
            intl += "HAVE_POSIX_PRINTF=1"_v;
        }

        //
        /*if (!intl.Variables.has("HAVE_NEWLOCALE"))
            intl += "HAVE_NEWLOCALE=0"_v;
        if (intl.Variables.find("HAVE_ASPRINTF") == intl.Variables.end())
            intl += "HAVE_ASPRINTF=0"_v;
        if (intl.Variables.find("HAVE_SNPRINTF") == intl.Variables.end())
            intl += "HAVE_SNPRINTF=0"_v;
        if (intl.Variables.find("HAVE_WPRINTF") == intl.Variables.end())
            intl += "HAVE_WPRINTF=0"_v;*/

        intl.Private += "HAVE_CONFIG_H"_d;

        if (s.Settings.TargetOS.Type == OSType::Windows)
            intl.Public += "Advapi32.lib"_l;

        if (intl.Variables["HAVE_LANGINFO_H"] && intl.Variables["HAVE_NL_LANGINFO"])
            intl.Public.Definitions["HAVE_LANGINFO_CODESET"];

        intl.ApiName = "CPPAN_LIBINTL_DLL_EXPORT";
        intl.replaceInFileOnce("gettext-runtime/intl/libgnuintl.in.h", "extern \"", "myexternc");
        intl.replaceInFileOnce("gettext-runtime/intl/libgnuintl.in.h", "extern ", "extern " + intl.ApiName + " ");
        intl.replaceInFileOnce("gettext-runtime/intl/libgnuintl.in.h", "myexternc", "extern \"");
        intl.replaceInFileOnce("gettext-runtime/intl/libgnuintl.in.h", "IN_LIBGLOCALE", "__VERY_UNDEFINED_DEFINITION__");

        intl.pushFrontToFileOnce("gettext-runtime/intl/vasnprintf.c", "#include <wchar.h>");

        intl.configureFile("gettext-runtime/intl/libgnuintl.in.h", "libgnuintl.h");
        intl.configureFile("gettext-runtime/intl/libgnuintl.in.h", "libintl.h");
        intl.configureFile("gettext-runtime/intl/libgnuintl.in.h", "gettext.h");

        intl.fileWriteOnce(intl.BinaryPrivateDir / "config.h", R"xxx(
#pragma once

#include <stdint.h>

#define HAVE_ICONV 1
#define HAVE_LC_MESSAGES 1
#define HAVE_PER_THREAD_LOCALE 1

#ifndef WIN32
#define HAVE_PTHREAD_RWLOCK 1
#endif

#ifdef _MSC_VER
#define LOCALEDIR "."
#define LOCALE_ALIAS_PATH "."
#define tmp_dirname "%TEMP%"
#endif

/* Please see the Gnulib manual for how to use these macros.

Suppress extern inline with HP-UX cc, as it appears to be broken; see
<http://lists.gnu.org/archive/html/bug-texinfo/2013-02/msg00030.html>.

Suppress extern inline with Sun C in standards-conformance mode, as it
mishandles inline functions that call each other.  E.g., for 'inline void f
(void) { } inline void g (void) { f (); }', c99 incorrectly complains
'reference to static identifier "f" in extern inline function'.
This bug was observed with Sun C 5.12 SunOS_i386 2011/11/16.

Suppress extern inline (with or without __attribute__ ((__gnu_inline__)))
on configurations that mistakenly use 'static inline' to implement
functions or macros in standard C headers like <ctype.h>.  For example,
if isdigit is mistakenly implemented via a static inline function,
a program containing an extern inline function that calls isdigit
may not work since the C standard prohibits extern inline functions
from calling static functions.  This bug is known to occur on:

OS X 10.8 and earlier; see:
http://lists.gnu.org/archive/html/bug-gnulib/2012-12/msg00023.html

DragonFly; see
http://muscles.dragonflybsd.org/bulk/bleeding-edge-potential/latest-per-pkg/ah-tty-0.3.12.log

FreeBSD; see:
http://lists.gnu.org/archive/html/bug-gnulib/2014-07/msg00104.html

OS X 10.9 has a macro __header_inline indicating the bug is fixed for C and
for clang but remains for g++; see <http://trac.macports.org/ticket/41033>.
Assume DragonFly and FreeBSD will be similar.  */
#if (((defined __APPLE__ && defined __MACH__) \
      || defined __DragonFly__ || defined __FreeBSD__) \
     && (defined __header_inline \
         ? (defined __cplusplus && defined __GNUC_STDC_INLINE__ \
            && ! defined __clang__) \
         : ((! defined _DONT_USE_CTYPE_INLINE_ \
             && (defined __GNUC__ || defined __cplusplus)) \
            || (defined _FORTIFY_SOURCE && 0 < _FORTIFY_SOURCE \
                && defined __GNUC__ && ! defined __cplusplus))))
# define _GL_EXTERN_INLINE_STDHEADER_BUG
#endif
#if ((__GNUC__ \
      ? defined __GNUC_STDC_INLINE__ && __GNUC_STDC_INLINE__ \
      : (199901L <= __STDC_VERSION__ \
         && !defined __HP_cc \
         && !defined __PGI \
         && !(defined __SUNPRO_C && __STDC__))) \
     && !defined _GL_EXTERN_INLINE_STDHEADER_BUG)
# define _GL_INLINE inline
# define _GL_EXTERN_INLINE extern inline
# define _GL_EXTERN_INLINE_IN_USE
#elif (2 < __GNUC__ + (7 <= __GNUC_MINOR__) && !defined __STRICT_ANSI__ \
       && !defined _GL_EXTERN_INLINE_STDHEADER_BUG)
# if defined __GNUC_GNU_INLINE__ && __GNUC_GNU_INLINE__
/* __gnu_inline__ suppresses a GCC 4.2 diagnostic.  */
#  define _GL_INLINE extern inline __attribute__ ((__gnu_inline__))
# else
#  define _GL_INLINE extern inline
# endif
# define _GL_EXTERN_INLINE extern
# define _GL_EXTERN_INLINE_IN_USE
#else
# define _GL_INLINE static _GL_UNUSED
# define _GL_EXTERN_INLINE static _GL_UNUSED
#endif


/* In GCC 4.6 (inclusive) to 5.1 (exclusive),
suppress bogus "no previous prototype for 'FOO'"
and "no previous declaration for 'FOO'" diagnostics,
when FOO is an inline function in the header; see
<https://gcc.gnu.org/bugzilla/show_bug.cgi?id=54113> and
<https://gcc.gnu.org/bugzilla/show_bug.cgi?id=63877>.  */
#if __GNUC__ == 4 && 6 <= __GNUC_MINOR__
# if defined __GNUC_STDC_INLINE__ && __GNUC_STDC_INLINE__
#  define _GL_INLINE_HEADER_CONST_PRAGMA
# else
#  define _GL_INLINE_HEADER_CONST_PRAGMA \
     _Pragma ("GCC diagnostic ignored \"-Wsuggest-attribute=const\"")
# endif
# define _GL_INLINE_HEADER_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wmissing-prototypes\"") \
    _Pragma ("GCC diagnostic ignored \"-Wmissing-declarations\"") \
    _GL_INLINE_HEADER_CONST_PRAGMA
# define _GL_INLINE_HEADER_END \
    _Pragma ("GCC diagnostic pop")
#else
# define _GL_INLINE_HEADER_BEGIN
# define _GL_INLINE_HEADER_END
#endif

/* Define as a marker that can be attached to declarations that might not
be used.  This helps to reduce warnings, such as from
GCC -Wunused-parameter.  */
#ifndef _GL_UNUSED
# if __GNUC__ >= 3 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 7)
#  define _GL_UNUSED __attribute__ ((__unused__))
# else
#  define _GL_UNUSED
# endif
#endif



#define __libc_lock_t                   gl_lock_t
#define __libc_lock_define              gl_lock_define
#define __libc_lock_define_initialized  gl_lock_define_initialized
#define __libc_lock_init                gl_lock_init
#define __libc_lock_lock                gl_lock_lock
#define __libc_lock_unlock              gl_lock_unlock
#define __libc_lock_recursive_t                   gl_recursive_lock_t
#define __libc_lock_define_recursive              gl_recursive_lock_define
#define __libc_lock_define_initialized_recursive  gl_recursive_lock_define_initialized
#define __libc_lock_init_recursive                gl_recursive_lock_init
#define __libc_lock_lock_recursive                gl_recursive_lock_lock
#define __libc_lock_unlock_recursive              gl_recursive_lock_unlock
#define glthread_in_use  libintl_thread_in_use
#define glthread_lock_init_func     libintl_lock_init_func
#define glthread_lock_lock_func     libintl_lock_lock_func
#define glthread_lock_unlock_func   libintl_lock_unlock_func
#define glthread_lock_destroy_func  libintl_lock_destroy_func
#define glthread_rwlock_init_multithreaded     libintl_rwlock_init_multithreaded
#define glthread_rwlock_init_func              libintl_rwlock_init_func
#define glthread_rwlock_rdlock_multithreaded   libintl_rwlock_rdlock_multithreaded
#define glthread_rwlock_rdlock_func            libintl_rwlock_rdlock_func
#define glthread_rwlock_wrlock_multithreaded   libintl_rwlock_wrlock_multithreaded
#define glthread_rwlock_wrlock_func            libintl_rwlock_wrlock_func
#define glthread_rwlock_unlock_multithreaded   libintl_rwlock_unlock_multithreaded
#define glthread_rwlock_unlock_func            libintl_rwlock_unlock_func
#define glthread_rwlock_destroy_multithreaded  libintl_rwlock_destroy_multithreaded
#define glthread_rwlock_destroy_func           libintl_rwlock_destroy_func
#define glthread_recursive_lock_init_multithreaded     libintl_recursive_lock_init_multithreaded
#define glthread_recursive_lock_init_func              libintl_recursive_lock_init_func
#define glthread_recursive_lock_lock_multithreaded     libintl_recursive_lock_lock_multithreaded
#define glthread_recursive_lock_lock_func              libintl_recursive_lock_lock_func
#define glthread_recursive_lock_unlock_multithreaded   libintl_recursive_lock_unlock_multithreaded
#define glthread_recursive_lock_unlock_func            libintl_recursive_lock_unlock_func
#define glthread_recursive_lock_destroy_multithreaded  libintl_recursive_lock_destroy_multithreaded
#define glthread_recursive_lock_destroy_func           libintl_recursive_lock_destroy_func
#define glthread_once_func            libintl_once_func
#define glthread_once_singlethreaded  libintl_once_singlethreaded
#define glthread_once_multithreaded   libintl_once_multithreaded



/* On Windows, variables that may be in a DLL must be marked specially.  */
#if (defined _MSC_VER && defined _DLL) && !defined IN_RELOCWRAPPER
# define DLL_VARIABLE __declspec (dllimport)
#else
# define DLL_VARIABLE
#endif

)xxx", true
);
    }

    auto &gss = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.gnu.gss", "1");
    {
        gss.setChecks("gss");
        gss +=
            "lib/[^/]*\\.c"_rr,
            "lib/[^/]*\\.h"_rr,
            "lib/gl/strverscmp.c",
            "lib/headers/.*\\.h"_rr;

        gss -=
            "lib/gl/strverscmp.c";

        gss.Public +=
            "lib/headers"_id;

        gss.Public += intl;

        gss.Definitions["PACKAGE"] = "\"" + gss.pkg.ppath.toString() + "\"";
        gss.Definitions["PACKAGE_VERSION"] = "\"1.0.3\"";
        gss.Public += "PO_SUFFIX=\".po\""_d;

        gss.fileWriteOnce(gss.BinaryPrivateDir / "config.h", "", true);
        gss.pushFrontToFileOnce("lib/version.c", R"xxx(
#include "gl/strverscmp.c"
#define strverscmp __strverscmp
)xxx");
    }

    //
    auto &libxml2 = addTarget<LibraryTarget>(s, "pvt.cppan.demo.libxml2", "2");
    {
        libxml2.setChecks("libxml2");
        libxml2.Public += libiconv, zlib;
        libxml2 +=
            ".*\\.h"_rr,
            "DOCBparser.c",
            "HTMLparser.c",
            "HTMLtree.c",
            "SAX.c",
            "SAX2.c",
            "buf.c",
            "c14n.c",
            "catalog.c",
            "chvalid.c",
            "debugXML.c",
            "dict.c",
            "encoding.c",
            "entities.c",
            "error.c",
            "globals.c",
            "hash.c",
            "include/libxml/xmlversion.h.in",
            "legacy.c",
            "list.c",
            "nanoftp.c",
            "nanohttp.c",
            "parser.c",
            "parserInternals.c",
            "pattern.c",
            "relaxng.c",
            "schematron.c",
            "threads.c",
            "tree.c",
            "trio.c",
            "trionan.c",
            "triostr.c",
            "uri.c",
            "valid.c",
            "xinclude.c",
            "xlink.c",
            "xmlIO.c",
            "xmlmemory.c",
            "xmlmodule.c",
            "xmlreader.c",
            "xmlregexp.c",
            "xmlsave.c",
            "xmlschemas.c",
            "xmlschemastypes.c",
            "xmlstring.c",
            "xmlunicode.c",
            "xmlwriter.c",
            "xpath.c",
            "xpointer.c",
            "xzlib.c";
        libxml2.Private.Definitions["HAVE_ZLIB_H"];
        libxml2.Private.Definitions["IN_LIBXML"];
        libxml2 += sw::Static, "LIBXML_STATIC"_d;
        libxml2.Private.Definitions["LIBXML_THREAD_ENABLED"];
        if (s.Settings.TargetOS.Type == OSType::Windows)
        {
            libxml2.Private.Definitions["WIN32"];
            libxml2.Private.Definitions["HAVE_WIN32_THREADS"];
        }
        libxml2.Variables["VERSION"] = libxml2.pkg.version.toString();
        libxml2.Variables["LIBXML_VERSION_NUMBER"] = 20908;// std::to_string(libxml2.pkg.version.toNumber());
        libxml2 +=
            "WITH_ZLIB=1"_v,
            "WITH_LZMA=0"_v,
            "WITH_ICU=0"_v,
            "WITH_ICONV=1"_v,
            "WITH_TRIO=0"_v,
            "WITH_TREE=1"_v,
            "WITH_OUTPUT=1"_v,
            "WITH_PUSH=1"_v,
            "WITH_READER=1"_v,
            "WITH_PATTERN=1"_v,
            "WITH_WRITER=1"_v,
            "WITH_SAX1=1"_v,
            "WITH_FTP=1"_v,
            "WITH_HTTP=1"_v,
            "WITH_VALID=1"_v,
            "WITH_HTML=1"_v,
            "WITH_LEGACY=1"_v,
            "WITH_C14N=1"_v,
            "WITH_CATALOG=1"_v,
            "WITH_DOCB=0"_v,
            "WITH_XPATH=1"_v,
            "WITH_XPTR=1"_v,
            "WITH_XINCLUDE=1"_v,
            "WITH_ISO8859X=1"_v,
            "WITH_DEBUG=0"_v,
            "WITH_MEM_DEBUG=0"_v,
            "WITH_RUN_DEBUG=0"_v,
            "WITH_REGEXPS=1"_v,
            "WITH_SCHEMAS=1"_v,
            "WITH_SCHEMATRON=1"_v,
            "WITH_MODULES=0"_v,
            "WITH_ISO8859X=1"_v,
            "WITH_ISO8859X=1"_v,
            "WITH_ISO8859X=1"_v,
            "WITH_ISO8859X=1"_v,
            "WITH_THREADS=1"_v,
            "WITH_THREAD_ALLOC=1"_v;
        error_code ec;
        fs::remove(libxml2.SourceDir / "include/libxml/xmlversion.h", ec);
        libxml2.configureFile(
            libxml2.SourceDir / "include/libxml/xmlversion.h.in",
            libxml2.BinaryDir / "libxml/xmlversion.h");
        libxml2.fileWriteOnce(libxml2.BinaryPrivateDir / "config.h",
            R"(
#ifdef _WIN32
#define _WINSOCKAPI_ 1
#define NEED_SOCKETS
#include <win32config.h>
#else

#define STDC_HEADERS 1

#define XML_SOCKLEN_T socklen_t

#define SEND_ARG2_CAST (const void *)
#define GETHOSTBYNAME_ARG_CAST (const char *)

#define VA_LIST_IS_ARRAY 1

#define SUPPORT_IP6 0

#endif
)",
true
);
        if (s.Settings.TargetOS.Type == OSType::Windows)
        {
            libxml2.Public << "ws2_32.lib"_l;
        }
    }

    //
    auto &lzma = addTarget<LibraryTarget>(s, "pvt.cppan.demo.xz_utils.lzma", "5");
    {
        lzma.setChecks("lzma");
        lzma +=
            "src/common/.*\\.c"_rr,
            "src/common/.*\\.h"_rr,
            "src/liblzma/.*\\.c"_rr,
            "src/liblzma/.*\\.h"_rr;

        lzma.Private +=
            "src/liblzma/lzma"_id,
            "src/liblzma/simple"_id,
            "src/liblzma/lz"_id,
            "src/liblzma/common"_id,
            "src/liblzma/check"_id,
            "src/liblzma/rangecoder"_id,
            "src/liblzma/delta"_id,
            "src/common"_id;

        lzma.Public +=
            "src/liblzma/api"_id;

        lzma.Private += "HAVE_CONFIG_H"_d;
        if (s.Settings.TargetOS.Type != OSType::Windows)
        {
            lzma.Private += "MYTHREAD_POSIX"_d;
        }
        if (s.Settings.TargetOS.Type == OSType::Windows)
        {
            lzma.Private += "MYTHREAD_WIN95"_d;
        }
        lzma.Private += sw::Shared, "DLL_EXPORT"_d;
        lzma.Public += sw::Static, "LZMA_API_STATIC"_d;

        lzma.replaceInFileOnce("src/liblzma/check/check.h",
            "#ifndef LZMA_SHA256FUNC",
            "#undef LZMA_SHA256FUNC\n#ifndef LZMA_SHA256FUNC");
        lzma.fileWriteOnce(lzma.BinaryPrivateDir / "config.h", R"(
#include <stdbool.h>
#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#endif

/* Prefix for symbols exported by tuklib_*.c files */
#define TUKLIB_SYMBOL_PREFIX lzma_

/* How many MiB of RAM to assume if the real amount cannot be determined. */
#define ASSUME_RAM 128

/* Define to 1 if crc32 integrity check is enabled. */
#define HAVE_CHECK_CRC32 1

/* Define to 1 if crc64 integrity check is enabled. */
#define HAVE_CHECK_CRC64 1

/* Define to 1 if sha256 integrity check is enabled. */
#define HAVE_CHECK_SHA256 1
#define HAVE_INTERNAL_SHA256 1

/* Define to 1 if arm decoder is enabled. */
#define HAVE_DECODER_ARM 1

/* Define to 1 if armthumb decoder is enabled. */
#define HAVE_DECODER_ARMTHUMB 1

/* Define to 1 if delta decoder is enabled. */
#define HAVE_DECODER_DELTA 1

/* Define to 1 if ia64 decoder is enabled. */
#define HAVE_DECODER_IA64 1

/* Define to 1 if lzma1 decoder is enabled. */
#define HAVE_DECODER_LZMA1 1

/* Define to 1 if lzma2 decoder is enabled. */
#define HAVE_DECODER_LZMA2 1

/* Define to 1 if powerpc decoder is enabled. */
#define HAVE_DECODER_POWERPC 1

/* Define to 1 if sparc decoder is enabled. */
#define HAVE_DECODER_SPARC 1

/* Define to 1 if x86 decoder is enabled. */
#define HAVE_DECODER_X86 1

/* Define to 1 if arm encoder is enabled. */
#define HAVE_ENCODER_ARM 1

/* Define to 1 if armthumb encoder is enabled. */
#define HAVE_ENCODER_ARMTHUMB 1

/* Define to 1 if delta encoder is enabled. */
#define HAVE_ENCODER_DELTA 1

/* Define to 1 if ia64 encoder is enabled. */
#define HAVE_ENCODER_IA64 1

/* Define to 1 if lzma1 encoder is enabled. */
#define HAVE_ENCODER_LZMA1 1

/* Define to 1 if lzma2 encoder is enabled. */
#define HAVE_ENCODER_LZMA2 1

/* Define to 1 if powerpc encoder is enabled. */
#define HAVE_ENCODER_POWERPC 1

/* Define to 1 if sparc encoder is enabled. */
#define HAVE_ENCODER_SPARC 1

/* Define to 1 if x86 encoder is enabled. */
#define HAVE_ENCODER_X86 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define to 1 to enable bt2 match finder. */
#define HAVE_MF_BT2 1

/* Define to 1 to enable bt3 match finder. */
#define HAVE_MF_BT3 1

/* Define to 1 to enable bt4 match finder. */
#define HAVE_MF_BT4 1

/* Define to 1 to enable hc3 match finder. */
#define HAVE_MF_HC3 1

/* Define to 1 to enable hc4 match finder. */
#define HAVE_MF_HC4 1
)", true);
    }

    //
    auto &eccdata = addTarget<ExecutableTarget>(s, "pvt.cppan.demo.gnu.nettle.eccdata", "3");
    {
        eccdata.setChecks("eccdata");
        eccdata +=
            "eccdata.c",
            "mini-gmp.c",
            "mini-gmp.h";

        eccdata -=
            "mini-gmp.c";
    }

    //
    auto &nettle = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.gnu.nettle.nettle", "3");
    {
        nettle.setChecks("nettle");
        nettle +=
            ".*\\.c"_rr,
            ".*\\.h"_rr,
            "version.h.in";
        nettle.Private += "UNUSED="_d;

        error_code ec;
        fs::create_directories(nettle.BinaryDir / "nettle");
        for (auto &f : nettle.gatherAllFiles())
            if (f.extension().string() == ".h")
                fs::copy_file(f, nettle.BinaryDir / "nettle" / f.filename(), fs::copy_options::skip_existing);

        if (s.Settings.Native.CompilerType == CompilerType::MSVC)
        {
            nettle.Private += "alloca=_alloca"_d;
        }

        nettle += "NETTLE_USE_MINI_GMP=1"_v;
        nettle += "MAJOR_VERSION=3"_v;
        nettle += "MINOR_VERSION=3"_v;
        const auto GMP_NUMB_BITS = nettle.Variables["SIZEOF_UNSIGNED_LONG"] * 8;
        nettle.Variables["GMP_NUMB_BITS"] = GMP_NUMB_BITS;
        nettle.configureFile("version.h.in", "version.h");
        nettle.fileWriteOnce(nettle.BinaryPrivateDir / "config.h", "", true);
        nettle.fileWriteOnce("nettle-stdint.h", "#include <stdint.h>", true);
        nettle.replaceInFileOnce(nettle.SourceDir / "pss-mgf1.h",
                "#include \"sha2.h\"",
                "#include \"sha2.h\"\n#include \"sha3.h\"");

        const std::map<int,std::vector<int>> args =
        {
            { 192,{192,7,6,GMP_NUMB_BITS} },
            { 224,{224,12,6,GMP_NUMB_BITS} },
            { 256,{256,14,6,GMP_NUMB_BITS} },
            { 384,{384,41,6,GMP_NUMB_BITS} },
            { 521,{521,56,6,GMP_NUMB_BITS} },
            { 25519,{255,14,6,GMP_NUMB_BITS} },
        };

        for (auto &[k,a] : args)
        {
            auto c = std::make_shared<Command>();
            c->program = eccdata.getOutputFile();
            for (auto &a2 : a)
                c->args.push_back(std::to_string(a2));
            nettle += c->redirectStdout(nettle.BinaryDir / ("ecc-" + std::to_string(k) + ".h"));
            //nettle.fileWriteOnce(, "c->out.text", true);
        }
    }

    //
    auto &libarchive = addTarget<LibraryTarget>(s, "pvt.cppan.demo.libarchive.libarchive", "3");
    {
        libarchive.setChecks("libarchive");
        libarchive +=
            "build/cmake/CheckFuncs.cmake",
            "build/cmake/CheckFuncs_stub.c.in",
            "build/cmake/CheckTypeExists.cmake",
            "build/cmake/config.h.in",
            "libarchive/[^/]*\\.c"_rr,
            "libarchive/[^/]*\\.h"_rr;

        libarchive.Public +=
            "libarchive"_id;

        libarchive.Public += "ARCHIVE_CRYPTO_MD5_NETTLE"_d;
        libarchive.Public += "ARCHIVE_CRYPTO_RMD160_NETTLE"_d;
        libarchive.Public += "ARCHIVE_CRYPTO_SHA1_NETTLE"_d;
        libarchive.Public += "ARCHIVE_CRYPTO_SHA256_NETTLE"_d;
        libarchive.Public += "ARCHIVE_CRYPTO_SHA384_NETTLE"_d;
        libarchive.Public += "ARCHIVE_CRYPTO_SHA512_NETTLE"_d;
        libarchive.Public += "HAVE_BZLIB_H"_d;
        libarchive.Public += "HAVE_CONFIG_H"_d;
        libarchive.Public += "HAVE_ICONV"_d;
        libarchive.Public += "HAVE_ICONV_H"_d;
        libarchive.Public += "HAVE_LIBLZ4"_d;
        libarchive.Public += "HAVE_LIBLZMA"_d;
        libarchive.Public += "HAVE_LIBNETTLE"_d;
        libarchive.Public += "HAVE_LIBXML_XMLREADER_H"_d;
        libarchive.Public += "HAVE_LIBXML_XMLWRITER_H"_d;
        libarchive.Public += "HAVE_LOCALCHARSET_H"_d;
        libarchive.Public += "HAVE_LOCALE_CHARSET"_d;
        libarchive.Public += "HAVE_LZ4HC_H"_d;
        libarchive.Public += "HAVE_LZ4_H"_d;
        libarchive.Public += "HAVE_LZMA_H"_d;
        libarchive.Public += "HAVE_LZO_LZO1X_H"_d;
        libarchive.Public += "HAVE_LZO_LZOCONF_H"_d;
        libarchive.Public += "HAVE_NETTLE_AES_H"_d;
        libarchive.Public += "HAVE_NETTLE_HMAC_H"_d;
        libarchive.Public += "HAVE_NETTLE_MD5_H"_d;
        libarchive.Public += "HAVE_NETTLE_PBKDF2_H"_d;
        libarchive.Public += "HAVE_NETTLE_RIPEMD160_H"_d;
        libarchive.Public += "HAVE_NETTLE_SHA_H"_d;
        libarchive.Public += "HAVE_ZLIB_H"_d;
        libarchive.Public += sw::Static, "LIBARCHIVE_STATIC"_d;
        libarchive += "ICONV_CONST=const"_v;

        if (s.Settings.TargetOS.Type == OSType::Windows)
            libarchive.Public += "Advapi32.lib"_l, "User32.lib"_l;

        libarchive.Public += bzip2, nettle, libxml2, lz4, zlib, lzo, lzma;

        libarchive.Variables["HAVE_WCSCPY"] = 1;
        libarchive.Variables["HAVE_WCSLEN"] = 1;
        libarchive.Variables["HAVE_WMEMCMP"] = 1;

        if (!libarchive.Variables["HAVE_DEV_T"])
        {
            if (s.Settings.Native.CompilerType == CompilerType::MSVC)
                libarchive.Variables["dev_t"] = "unsigned int";
        }

        if (!libarchive.Variables["HAVE_GID_T"])
        {
            if (s.Settings.TargetOS.Type == OSType::Windows)
                libarchive.Variables["gid_t"] = "short";
            else
                libarchive.Variables["gid_t"] = "unsigned int";
        }

        if (!libarchive.Variables["HAVE_ID_T"])
        {
            if (s.Settings.TargetOS.Type == OSType::Windows)
                libarchive.Variables["id_t"] = "short";
            else
                libarchive.Variables["id_t"] = "unsigned int";
        }

        if (!libarchive.Variables["HAVE_UID_T"])
        {
            if (s.Settings.TargetOS.Type == OSType::Windows)
                libarchive.Variables["uid_t"] = "short";
            else
                libarchive.Variables["uid_t"] = "unsigned int";
        }

        if (!libarchive.Variables["HAVE_MODE_T"])
        {
            if (s.Settings.TargetOS.Type == OSType::Windows)
                libarchive.Variables["mode_t"] = "unsigned short";
            else
                libarchive.Variables["mode_t"] = "int";
        }

        if (!libarchive.Variables["HAVE_OFF_T"])
        {
            libarchive.Variables["off_t"] = "__int64";
        }

        if (!libarchive.Variables["HAVE_SIZE_T"])
        {
            if (s.Settings.TargetOS.Arch == ArchType::x86_64)
                libarchive.Variables["size_t"] = "uint64_t";
            else
                libarchive.Variables["size_t"] = "uint32_t";
        }

        if (!libarchive.Variables["HAVE_SSIZE_T"])
        {
            if (s.Settings.TargetOS.Arch == ArchType::x86_64)
                libarchive.Variables["ssize_t"] = "int64_t";
            else
                libarchive.Variables["ssize_t"] = "long";
        }

        if (!libarchive.Variables["HAVE_PID_T"])
        {
            if (s.Settings.TargetOS.Type == OSType::Windows)
                libarchive.Variables["pid_t"] = "int";
            else
                throw std::runtime_error("pid_t doesn't exist on this platform?");
        }

        if (!libarchive.Variables["HAVE_INTPTR_T"])
        {
            if (s.Settings.TargetOS.Arch == ArchType::x86_64)
                libarchive.Variables["intptr_t"] = "int64_t";
            else
                libarchive.Variables["intptr_t"] = "int32_t";
        }

        if (!libarchive.Variables["HAVE_UINTPTR_T"])
        {
            if (s.Settings.TargetOS.Arch == ArchType::x86_64)
                libarchive.Variables["intptr_t"] = "uint64_t";
            else
                libarchive.Variables["intptr_t"] = "uint32_t";
        }

        if (!libarchive.Variables["HAVE_SIZEOF_WCHAR_T"])
        {
            libarchive.Variables["HAVE_WCHAR_T"] = 1;
        }

        if (s.Settings.TargetOS.Type == OSType::Windows)
            libarchive.Variables["HAVE_WINCRYPT_H"] = 1;

        // TODO: add 'or cygwin'
        // IF(NOT WIN32 OR CYGWIN)
        if (s.Settings.TargetOS.Type != OSType::Windows)
        {
            libarchive -=
                "libarchive/archive_entry_copy_bhfi.c",
                "libarchive/archive_read_disk_windows.c",
                "libarchive/archive_windows.c",
                "libarchive/archive_windows.c",
                "libarchive/archive_write_disk_windows.c",
                "libarchive/filter_fork_windows.c";
        }

        // if (UNIX)
        if (s.Settings.TargetOS.Type != OSType::Windows)
        {
            // TODO:
            /*
            # acl
            if (UNIX)
                CHECK_LIBRARY_EXISTS(acl "acl_get_file" "" HAVE_LIBACL)
                IF(HAVE_LIBACL)
                    SET(CMAKE_REQUIRED_LIBRARIES "acl")
                    FIND_LIBRARY(ACL_LIBRARY NAMES acl)
                    LIST(APPEND ADDITIONAL_LIBS ${ACL_LIBRARY})
                ENDIF(HAVE_LIBACL)
                #
                include(build/cmake/CheckFuncs.cmake)
                include(build/cmake/CheckTypeExists.cmake)
                CHECK_FUNCTION_EXISTS_GLIBC(acl_create_entry HAVE_ACL_CREATE_ENTRY)
                CHECK_FUNCTION_EXISTS_GLIBC(acl_init HAVE_ACL_INIT)
                CHECK_FUNCTION_EXISTS_GLIBC(acl_set_fd HAVE_ACL_SET_FD)
                CHECK_FUNCTION_EXISTS_GLIBC(acl_set_fd_np HAVE_ACL_SET_FD_NP)
                CHECK_FUNCTION_EXISTS_GLIBC(acl_set_file HAVE_ACL_SET_FILE)
                CHECK_TYPE_EXISTS(acl_permset_t "${INCLUDES}"    HAVE_ACL_PERMSET_T)

                # The "acl_get_perm()" function was omitted from the POSIX draft.
                # (It's a pretty obvious oversight; otherwise, there's no way to
                # test for specific permissions in a permset.)  Linux uses the obvious
                # name, FreeBSD adds _np to mark it as "non-Posix extension."
                # Test for both as a double-check that we really have POSIX-style ACL support.
                CHECK_FUNCTION_EXISTS(acl_get_fd_np HAVE_ACL_GET_FD_NP)
                CHECK_FUNCTION_EXISTS(acl_get_perm HAVE_ACL_GET_PERM)
                CHECK_FUNCTION_EXISTS(acl_get_perm_np HAVE_ACL_GET_PERM_NP)
                CHECK_FUNCTION_EXISTS(acl_get_link HAVE_ACL_GET_LINK)
                CHECK_FUNCTION_EXISTS(acl_get_link_np HAVE_ACL_GET_LINK_NP)
                CHECK_FUNCTION_EXISTS(acl_is_trivial_np HAVE_ACL_IS_TRIVIAL_NP)
                CHECK_FUNCTION_EXISTS(acl_set_link_np HAVE_ACL_SET_LINK_NP)

                # MacOS has an acl.h that isn't POSIX.  It can be detected by
                # checking for ACL_USER
                CHECK_SYMBOL_EXISTS(ACL_USER "${INCLUDES}" HAVE_ACL_USER)
            endif()
            */
        }

        libarchive.configureFile("build/cmake/config.h.in", libarchive.BinaryPrivateDir / "config.h");
    }

    //
    auto &nghttp2 = addTarget<LibraryTarget>(s, "pvt.cppan.demo.nghttp2", "1");
    {
        nghttp2.setChecks("nghttp2");
        nghttp2 +=
            "cmakeconfig.h.in",
            "lib/.*\\.c"_rr,
            "lib/.*\\.h"_rr,
            "lib/includes/.*\\.h"_rr,
            "lib/includes/.*\\.h.in"_rr;

        nghttp2.Public +=
            "lib/includes"_id;

        nghttp2.Private += "BUILDING_NGHTTP2"_d;
        nghttp2.Public += sw::Static, "NGHTTP2_STATICLIB"_d;

        nghttp2.Private += "HAVE_CONFIG_H"_d;

        nghttp2.Definitions["PACKAGE"] = "\"" + nghttp2.pkg.ppath.toString() + "\"";
        nghttp2.Variables["PACKAGE_VERSION"] = "1.26.0";
        nghttp2.Variables["PACKAGE_VERSION_NUM"] = "0x012600LL";

        if (s.Settings.Native.CompilerType == CompilerType::MSVC)
        {
            if (s.Settings.TargetOS.Arch == ArchType::x86_64)
                nghttp2.Variables["ssize_t"] = "int64_t";
            else
                nghttp2.Variables["ssize_t"] = "int";
        }

        nghttp2.configureFile("lib/includes/nghttp2/nghttp2ver.h.in", "nghttp2/nghttp2ver.h");
        nghttp2.configureFile("cmakeconfig.h.in", nghttp2.BinaryPrivateDir / "config.h");
    }

    auto &crypto = addTarget<LibraryTarget>(s, "pvt.cppan.demo.openssl.crypto", "1");
    {
        crypto.setChecks("crypto");
        crypto +=
            "crypto/.*\\.c"_rr,
            "crypto/.*\\.h"_rr,
            "e_os.h",
            "engines/.*\\.c"_rr,
            "engines/.*\\.h"_rr,
            "include/.*"_rr;

        crypto -=
            "crypto/LPdir_.*"_rr,
            "crypto/aes/asm/.*"_rr,
            //"crypto/bf/.*"_rr,
            "crypto/bn/asm/.*"_rr,
            "crypto/des/ncbc_enc.c",
            "crypto/md2/.*"_rr,
            "crypto/rc2/tab.c",
            "crypto/rc5/.*"_rr,
            "engines/afalg/.*"_rr;

        crypto.Private +=
            "crypto/include"_id,
            "."_id,
            "crypto/modes"_id,
            "crypto/asn1"_id,
            "crypto/dsa"_id,
            "crypto/evp"_id;

        crypto.Public +=
            "include"_id;

        crypto.ExportAllSymbols = true;

        crypto.Private += "NO_WINDOWS_BRAINDEATH"_d;
        crypto.Private += "OPENSSL_NO_DYNAMIC_ENGINE"_d;
        crypto.Public += "OPENSSL_NO_ASM"_d;
        if (s.Settings.TargetOS.Type == OSType::Windows)
        {
            crypto.Private += "DSO_WIN32"_d;
            crypto.Public += "Crypt32.lib"_l;
            crypto.Public += "WIN32_LEAN_AND_MEAN"_d;
        }

        if (s.Settings.TargetOS.Type == OSType::Windows)
        {
            crypto.Public += "ws2_32.lib"_l, "advapi32.lib"_l, "User32.lib"_l;
        }

        if (s.Settings.TargetOS.Type == OSType::Windows)
        {
            crypto.Public.Definitions["OPENSSLDIR"] = "\"C:/Program Files/Common Files/SSL/\"";
            crypto.Public.Definitions["ENGINESDIR"] = "\"C:/Program Files/OpenSSL/lib/engines/\"";
        }
        else
        {
            crypto.Public.Definitions["OPENSSLDIR"] = "\"/usr/local/ssl\"";
            crypto.Public.Definitions["ENGINESDIR"] = "\"/usr/local/ssl/lib/engines\"";
        }

        crypto -=
            // arch stuff
            "crypto/armcap.c",
            "crypto/s390xcap.c",
            "crypto/sparcv9cap.c",
            "crypto/ppccap.c",
            "crypto/aes/aes_x86core.c",

            // bins
            "crypto/x509v3/v3conf.c",
            "crypto/x509v3/v3prin.c",
            "crypto/x509v3/tabtest.c",

            "crypto/pkcs7/pk7_enc.c",

            "crypto/ec/ecp_nistz256.c",
            "crypto/ec/ecp_nistz256_table.c",

            "engines/e_chil.c";

        if (s.Settings.TargetOS.Type == OSType::Windows)
            crypto -= "crypto/poly1305/poly1305_ieee754.c";

        crypto -=
            "crypto/bf/bf_cbc.c";

        crypto.Variables["OPENSSL_SYS"] = "UNIX";
        if (s.Settings.TargetOS.Type == OSType::Linux)
            crypto.Variables["OPENSSL_SYS"] = "LINUX";
        else if (s.Settings.TargetOS.Type == OSType::Cygwin)
            crypto.Variables["OPENSSL_SYS"] = "CYGWIN";
        else if (s.Settings.TargetOS.Type == OSType::Windows)
        {
            if (s.Settings.TargetOS.Arch == ArchType::x86_64)
                crypto.Variables["OPENSSL_SYS"] = "WIN64A";
            else
                crypto.Variables["OPENSSL_SYS"] = "WIN32";
        }
        else if (s.Settings.TargetOS.Type == OSType::Macos)
            crypto.Variables["OPENSSL_SYS"] = "MACOSX";

        /*
        if (MINGW)
        if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(OPENSSL_SYS MINGW64)
        else()
        set(OPENSSL_SYS MINGW32)
        endif()
        endif()*/

        if (s.Settings.TargetOS.Arch == ArchType::x86_64)
        {
            crypto.Variables["SIXTY_FOUR_BIT"] = "define";
            crypto.Variables["THIRTY_TWO_BIT"] = "undef";
        }
        else
        {
            crypto.Variables["SIXTY_FOUR_BIT"] = "undef";
            crypto.Variables["THIRTY_TWO_BIT"] = "define";
        }

        if (s.Settings.TargetOS.Type == OSType::Windows)
            crypto.Variables["CPPAN_SHARED_LIBRARY_SUFFIX"] = ".dll";
        else if (s.Settings.TargetOS.Type == OSType::Macos)
            crypto.Variables["CPPAN_SHARED_LIBRARY_SUFFIX"] = ".dylib";
        else
            crypto.Variables["CPPAN_SHARED_LIBRARY_SUFFIX"] = ".so";

        crypto.fileWriteOnce("openssl/opensslconf.h.in", R"xxx(
/*
         *
         * Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
         *
         * Licensed under the OpenSSL license (the "License").  You may not use
         * this file except in compliance with the License.  You can obtain a copy
         * in the file LICENSE in the source distribution or at
         * https://www.openssl.org/source/license.html
         */

        #ifdef  __cplusplus
        extern "C" {
        #endif

        #ifdef OPENSSL_ALGORITHM_DEFINES
        # error OPENSSL_ALGORITHM_DEFINES no longer supported
        #endif

        /*
         * OpenSSL was configured with the following options:
         */

        #ifndef OPENSSL_SYS_${OPENSSL_SYS}
        # define OPENSSL_SYS_${OPENSSL_SYS} 1
        #endif
        #ifndef OPENSSL_NO_MD2
        # define OPENSSL_NO_MD2
        #endif
        #ifndef OPENSSL_NO_RC5
        # define OPENSSL_NO_RC5
        #endif
        #ifndef OPENSSL_THREADS
        # define OPENSSL_THREADS
        #endif
        #ifndef OPENSSL_NO_ASAN
        # define OPENSSL_NO_ASAN
        #endif
        #ifndef OPENSSL_NO_CRYPTO_MDEBUG
        # define OPENSSL_NO_CRYPTO_MDEBUG
        #endif
        #ifndef OPENSSL_NO_CRYPTO_MDEBUG_BACKTRACE
        # define OPENSSL_NO_CRYPTO_MDEBUG_BACKTRACE
        #endif
        #ifndef OPENSSL_NO_EC_NISTP_64_GCC_128
        # define OPENSSL_NO_EC_NISTP_64_GCC_128
        #endif
        //#ifndef OPENSSL_NO_EGD
        //# define OPENSSL_NO_EGD
        //#endif
        #ifndef OPENSSL_NO_FUZZ_AFL
        # define OPENSSL_NO_FUZZ_AFL
        #endif
        #ifndef OPENSSL_NO_FUZZ_LIBFUZZER
        # define OPENSSL_NO_FUZZ_LIBFUZZER
        #endif
        #ifndef OPENSSL_NO_HEARTBEATS
        # define OPENSSL_NO_HEARTBEATS
        #endif
        #ifndef OPENSSL_NO_MSAN
        # define OPENSSL_NO_MSAN
        #endif
        #ifndef OPENSSL_NO_SCTP
        # define OPENSSL_NO_SCTP
        #endif
        #ifndef OPENSSL_NO_SSL_TRACE
        # define OPENSSL_NO_SSL_TRACE
        #endif
        #ifndef OPENSSL_NO_SSL3
        # define OPENSSL_NO_SSL3
        #endif
        #ifndef OPENSSL_NO_SSL3_METHOD
        # define OPENSSL_NO_SSL3_METHOD
        #endif
        #ifndef OPENSSL_NO_UBSAN
        # define OPENSSL_NO_UBSAN
        #endif
        #ifndef OPENSSL_NO_UNIT_TEST
        # define OPENSSL_NO_UNIT_TEST
        #endif
        #ifndef OPENSSL_NO_WEAK_SSL_CIPHERS
        # define OPENSSL_NO_WEAK_SSL_CIPHERS
        #endif
        #ifndef OPENSSL_NO_AFALGENG
        # define OPENSSL_NO_AFALGENG
        #endif

        /*
         * Sometimes OPENSSSL_NO_xxx ends up with an empty file and some compilers
         * don't like that.  This will hopefully silence them.
         */
        #define NON_EMPTY_TRANSLATION_UNIT static void *dummy = &dummy;

        /*
         * Applications should use -DOPENSSL_API_COMPAT=<version> to suppress the
         * declarations of functions deprecated in or before <version>. Otherwise, they
         * still won't see them if the library has been built to disable deprecated
         * functions.
         */
        #if defined(OPENSSL_NO_DEPRECATED)
        # define DECLARE_DEPRECATED(f)
        #elif __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ > 0)
        # define DECLARE_DEPRECATED(f)    f __attribute__ ((deprecated));
        #else
        # define DECLARE_DEPRECATED(f)   f;
        #endif

        #ifndef OPENSSL_FILE
        # ifdef OPENSSL_NO_FILENAMES
        #  define OPENSSL_FILE ""
        #  define OPENSSL_LINE 0
        # else
        #  define OPENSSL_FILE __FILE__
        #  define OPENSSL_LINE __LINE__
        # endif
        #endif

        #ifndef OPENSSL_MIN_API
        # define OPENSSL_MIN_API 0
        #endif

        #if !defined(OPENSSL_API_COMPAT) || OPENSSL_API_COMPAT < OPENSSL_MIN_API
        # undef OPENSSL_API_COMPAT
        # define OPENSSL_API_COMPAT OPENSSL_MIN_API
        #endif

        #if OPENSSL_API_COMPAT < 0x10100000L
        # define DEPRECATEDIN_1_1_0(f)   DECLARE_DEPRECATED(f)
        #else
        # define DEPRECATEDIN_1_1_0(f)
        #endif

        #if OPENSSL_API_COMPAT < 0x10000000L
        # define DEPRECATEDIN_1_0_0(f)   DECLARE_DEPRECATED(f)
        #else
        # define DEPRECATEDIN_1_0_0(f)
        #endif

        #if OPENSSL_API_COMPAT < 0x00908000L
        # define DEPRECATEDIN_0_9_8(f)   DECLARE_DEPRECATED(f)
        #else
        # define DEPRECATEDIN_0_9_8(f)
        #endif

        //#define OPENSSL_CPUID_OBJ

        /* Generate 80386 code? */
        #undef I386_ONLY

        #undef OPENSSL_UNISTD
        #define OPENSSL_UNISTD <unistd.h>

        #define OPENSSL_EXPORT_VAR_AS_FUNCTION

        /*
         * The following are cipher-specific, but are part of the public API.
         */
        #if !defined(OPENSSL_SYS_UEFI)
        # undef BN_LLONG
        /* Only one for the following should be defined */
        # undef SIXTY_FOUR_BIT_LONG
        # ${SIXTY_FOUR_BIT} SIXTY_FOUR_BIT
        # ${THIRTY_TWO_BIT} THIRTY_TWO_BIT
        #endif

        #define RC4_INT unsigned int

        #ifdef  __cplusplus
        }
        #endif
)xxx", true);

        crypto.fileWriteOnce("internal/bn_conf.h.in", R"xxx(
        /*
         * Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
         *
         * Licensed under the OpenSSL license (the "License").  You may not use
         * this file except in compliance with the License.  You can obtain a copy
         * in the file LICENSE in the source distribution or at
         * https://www.openssl.org/source/license.html
         */

        #ifndef HEADER_BN_CONF_H
        # define HEADER_BN_CONF_H

        /*
         * The contents of this file are not used in the UEFI build, as
         * both 32-bit and 64-bit builds are supported from a single run
         * of the Configure script.
         */

        /* Should we define BN_DIV2W here? */

        /* Only one for the following should be defined */
        # undef SIXTY_FOUR_BIT_LONG
        # ${SIXTY_FOUR_BIT} SIXTY_FOUR_BIT
        # ${THIRTY_TWO_BIT} THIRTY_TWO_BIT

        #endif
)xxx", true);

        crypto.fileWriteOnce("internal/dso_conf.h.in", R"xxx(
        /*
         * Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
         *
         * Licensed under the OpenSSL license (the "License").  You may not use
         * this file except in compliance with the License.  You can obtain a copy
         * in the file LICENSE in the source distribution or at
         * https://www.openssl.org/source/license.html
         */

        #ifndef HEADER_DSO_CONF_H
        # define HEADER_DSO_CONF_H

        # define DSO_EXTENSION "${CPPAN_SHARED_LIBRARY_SUFFIX}"
        #endif
)xxx", true);

        crypto.configureFile(crypto.BinaryDir / "openssl/opensslconf.h.in", "openssl/opensslconf.h");
        crypto.configureFile(crypto.BinaryDir / "internal/bn_conf.h.in", "internal/bn_conf.h");
        crypto.configureFile(crypto.BinaryDir / "internal/dso_conf.h.in", "internal/dso_conf.h");
    }

    auto &ssl = addTarget<LibraryTarget>(s, "pvt.cppan.demo.openssl.ssl", "1");
    {
        ssl.setChecks("ssl");
        ssl.ExportAllSymbols = true;
        ssl +=
            "e_os.h",
            "ssl/.*\\.c"_rr,
            "ssl/.*\\.h"_rr;

        ssl.Private +=
            "."_id,
            "tls"_id;

        ssl.Public += crypto;
    }

    auto &libssh2 = addTarget<LibraryTarget>(s, "pvt.cppan.demo.libssh2", "1");
    {
        libssh2.setChecks("libssh2");
        libssh2 +=
            "cmake/CheckNonblockingSocketSupport.cmake",
            "include/.*"_rr,
            "src/.*\\.c"_rr,
            "src/.*\\.h"_rr,
            "src/.*\\.in"_rr;

        libssh2.Private +=
            "src"_id;

        libssh2.Public +=
            "include"_id;

        libssh2.Private += "LIBSSH2_LIBRARY"_d;
        libssh2.Public += "LIBSSH2_OPENSSL"_d;
        if (s.Settings.TargetOS.Type == OSType::Windows)
        {
            libssh2.Public += sw::Shared, "LIBSSH2_WIN32"_d;
        }

        libssh2.Public += ssl;

        if (
            libssh2.Variables["HAVE_O_NONBLOCK"] == "0" &&
            libssh2.Variables["HAVE_IOCTLSOCKET"] == "0" &&
            libssh2.Variables["HAVE_IOCTLSOCKET_CASE"] == "0" &&
            libssh2.Variables["HAVE_SO_NONBLOCK"] == "0"
            )
            libssh2.Variables["HAVE_DISABLED_NONBLOCKING"] = "1";

        libssh2.configureFile("src/libssh2_config_cmake.h.in", "libssh2_config.h");
    }

    auto recv_args = [&s](auto &t)
    {
        if (t.Variables["HAVE_SSIZE_T"] && t.Variables["HAVE_SOCKLEN_T"])
        {
            t.Variables["RECVFROM_TYPE_RETV"] = "ssize_t";
            t.Variables["RECVFROM_TYPE_ARG3"] = "size_t";
        }
        else
        {
            t.Variables["RECVFROM_TYPE_RETV"] = "int";
            t.Variables["RECVFROM_TYPE_ARG3"] = "int";
        }

        if (s.Settings.TargetOS.Type == OSType::Windows)
            t.Variables["RECVFROM_TYPE_ARG1"] = "SOCKET";
        else
            t.Variables["RECVFROM_TYPE_ARG1"] = "int";

        if (t.Variables["HAVE_SOCKLEN_T"])
        {
            t.Variables["RECVFROM_TYPE_ARG6"] = "socklen_t *";
            t.Variables["GETNAMEINFO_TYPE_ARG2"] = "socklen_t";
            t.Variables["GETNAMEINFO_TYPE_ARG46"] = "socklen_t";
        }
        else
        {
            t.Variables["RECVFROM_TYPE_ARG6"] = "int *";
            t.Variables["GETNAMEINFO_TYPE_ARG2"] = "int";
            t.Variables["GETNAMEINFO_TYPE_ARG46"] = "int";
        }

        t.Variables["RECV_TYPE_RETV"] = t.Variables["RECVFROM_TYPE_RETV"];
        t.Variables["SEND_TYPE_RETV"] = t.Variables["RECVFROM_TYPE_RETV"];
        t.Variables["RECV_TYPE_ARG1"] = t.Variables["RECVFROM_TYPE_ARG1"];
        t.Variables["SEND_TYPE_ARG1"] = t.Variables["RECVFROM_TYPE_ARG1"];
        t.Variables["RECV_TYPE_ARG3"] = t.Variables["RECVFROM_TYPE_ARG3"];
        t.Variables["SEND_TYPE_ARG3"] = t.Variables["RECVFROM_TYPE_ARG3"];
        t.Variables["GETHOSTNAME_TYPE_ARG2"] = t.Variables["RECVFROM_TYPE_ARG3"];

        // These should always be "sane" values to use always
        t.Variables["RECVFROM_QUAL_ARG5"] = "";
        t.Variables["RECVFROM_TYPE_ARG2"] = "void *";
        t.Variables["RECVFROM_TYPE_ARG4"] = "int";
        t.Variables["RECVFROM_TYPE_ARG5"] = "struct sockaddr *";
        t.Variables["RECV_TYPE_ARG2"] = "void *";
        t.Variables["RECV_TYPE_ARG4"] = "int";
        t.Variables["GETNAMEINFO_TYPE_ARG1"] = "struct sockaddr *";
        t.Variables["GETNAMEINFO_TYPE_ARG7"] = "int";
        t.Variables["SEND_TYPE_ARG2"] = "void *";
        t.Variables["SEND_QUAL_ARG2"] = "const";
        t.Variables["SEND_TYPE_ARG4"] = "int";

        if (t.Variables["HAVE_FCNTL"] && t.Variables["HAVE_O_NONBLOCK"])
            t.Variables["HAVE_FCNTL_O_NONBLOCK"] = 1;

        if (t.Variables["HAVE_IOCTL"] && t.Variables["HAVE_FIONBIO"])
            t.Variables["HAVE_IOCTL_FIONBIO"] = 1;

        if (t.Variables["HAVE_IOCTLSOCKET"] && t.Variables["HAVE_FIONBIO"])
            t.Variables["HAVE_IOCTLSOCKET_FIONBIO"] = 1;

        if (t.Variables["HAVE_IOCTLSOCKET_CAMEL"] && t.Variables["HAVE_FIONBIO"])
            t.Variables["HAVE_IOCTLSOCKET_CAMEL_FIONBIO"] = 1;
    };

    auto &c_ares = addTarget<LibraryTarget>(s, "pvt.cppan.demo.c_ares", "1");
    {
        c_ares.setChecks("c_ares");
        c_ares +=
            "[^/]*\\.c"_rr,
            "[^/]*\\.cmake"_rr,
            "[^/]*\\.h"_rr;

        c_ares.Public +=
            "."_id;

        c_ares.Private += "CARES_BUILDING_LIBRARY"_d;
        c_ares.Private += "HAVE_CONFIG_H"_d;
        c_ares.Public += "HAVE_RECV"_d;
        c_ares.Public += "HAVE_RECVFROM"_d;
        c_ares.Public += "HAVE_SEND"_d;
        c_ares.Public += "HAVE_SENDTO"_d;
        c_ares.Public += "HAVE_STRUCT_ADDRINFO"_d;
        c_ares.Public += "HAVE_STRUCT_SOCKADDR_IN6"_d;
        c_ares.Public += "HAVE_STRUCT_TIMEVAL"_d;
        if (s.Settings.TargetOS.Type == OSType::Windows)
        {
            c_ares.Public += "HAVE_CLOSESOCKET"_d;
            c_ares.Public += "HAVE_IOCTLSOCKET_FIONBIO"_d;
        }
        c_ares.Public += sw::Static, "CARES_STATICLIB"_d;

        //
        if (c_ares.Variables["HAVE_SOCKLEN_T"])
            c_ares.Variables["CARES_TYPEOF_ARES_SOCKLEN_T"] = "socklen_t";
        else
            c_ares.Variables["CARES_TYPEOF_ARES_SOCKLEN_T"] = "int";

        recv_args(c_ares);

        c_ares.Variables["CARES_HAVE_SYS_TYPES_H"] = c_ares.Variables["HAVE_SYS_TYPES_H"];
        c_ares.Variables["CARES_HAVE_SYS_SOCKET_H"] = c_ares.Variables["HAVE_SYS_SOCKET_H"];
        c_ares.Variables["CARES_HAVE_WINDOWS_H"] = c_ares.Variables["HAVE_WINDOWS_H"];
        c_ares.Variables["CARES_HAVE_WS2TCPIP_H"] = c_ares.Variables["HAVE_WS2TCPIP_H"];
        c_ares.Variables["CARES_HAVE_WINSOCK2_H"] = c_ares.Variables["HAVE_WINSOCK2_H"];

        if (s.Settings.TargetOS.Type == OSType::Windows)
            c_ares.Public += "ws2_32.lib"_l, "Advapi32.lib"_l;

        if (s.Settings.TargetOS.Type == OSType::Windows)
        {
            c_ares.Public.Definitions["MSG_NOSIGNAL"] = 0;
        }

        c_ares.configureFile("ares_build.h.cmake", "ares_build.h");
        c_ares.configureFile("ares_config.h.cmake", "ares_config.h");
    }

    auto &libcurl = addTarget<LibraryTarget>(s, "pvt.cppan.demo.badger.curl.libcurl", "7");
    {
        libcurl.setChecks("libcurl");
        libcurl.setChecks("c_ares");
        libcurl +=
            "include/.*\\.cmake"_rr,
            "include/.*\\.h"_rr,
            "lib/.*\\.c"_rr,
            "lib/.*\\.h"_rr,
            "lib/curl_config.h.cmake";

        libcurl.Private +=
            "lib"_id;

        libcurl.Public +=
            "include"_id;

        libcurl.Private += "BUILDING_LIBCURL"_d;
        libcurl.Public += "HAVE_GSSAPI"_d;
        libcurl.Public += "HAVE_GSSGNU"_d;
        libcurl.Public += "HAVE_LIBSSH2_H"_d;
        libcurl.Public += "HAVE_LIBZ"_d;
        libcurl.Public += "HAVE_SOCKET"_d;
        libcurl.Public += "HAVE_ZLIB_H"_d;
        //libcurl.Public += "USE_ARES"_d;
        libcurl.Public += "USE_LIBSSH2"_d;
        libcurl.Public += "USE_NGHTTP2"_d;
        libcurl.Public += "USE_OPENSSL"_d;
        if (s.Settings.TargetOS.Type != OSType::Windows)
        {
            libcurl.Private += "HAVE_CONFIG_H"_d;
            libcurl.Public += "HAVE_GETADDRINFO_THREADSAFE"_d;
        }
        libcurl.Public += sw::Static, "CURL_STATICLIB"_d;

        libcurl.Public += c_ares, gss, libssh2, zlib, nghttp2;
        //libcurl.Public += c_ares;

        if (s.Settings.TargetOS.Type == OSType::Windows)
            libcurl.Public += "Wldap32.lib"_l;
        else
            libcurl.Public += "lber"_l, "ldap"_l;

        libcurl.Variables["OPERATING_SYSTEM"] = "${CMAKE_SYSTEM_NAME}";
        libcurl.Variables["OS"] = "\"${CMAKE_SYSTEM_NAME}\"";
        libcurl.Variables["HAVE_POSIX_STRERROR_R"] = "1";

        libcurl.Variables["CURL_SIZEOF_LONG"] = libcurl.Variables["SIZEOF_LONG"];

        if (libcurl.Variables["SIZEOF_LONG"] == 8)
        {
            libcurl.Variables["CURL_TYPEOF_CURL_OFF_T"] = "long";
            libcurl.Variables["CURL_SIZEOF_CURL_OFF_T"] = 8;
            libcurl.Definitions["CURL_SIZEOF_CURL_OFF_T"] = 8;
            libcurl.Variables["CURL_FORMAT_CURL_OFF_T"] = "ld";
            libcurl.Variables["CURL_FORMAT_CURL_OFF_TU"] = "lu";
            libcurl.Variables["CURL_FORMAT_OFF_T"] = "%ld";
            libcurl.Variables["CURL_SUFFIX_CURL_OFF_T"] = "L";
            libcurl.Variables["CURL_SUFFIX_CURL_OFF_TU"] = "UL";
        }

        if (libcurl.Variables["SIZEOF_LONG_LONG"] == 8)
        {
            libcurl.Variables["CURL_TYPEOF_CURL_OFF_T"] = "long long";
            libcurl.Variables["CURL_SIZEOF_CURL_OFF_T"] = 8;
            libcurl.Definitions["CURL_SIZEOF_CURL_OFF_T"] = 8;
            libcurl.Variables["CURL_FORMAT_CURL_OFF_T"] = "lld";
            libcurl.Variables["CURL_FORMAT_CURL_OFF_TU"] = "llu";
            libcurl.Variables["CURL_FORMAT_OFF_T"] = "%lld";
            libcurl.Variables["CURL_SUFFIX_CURL_OFF_T"] = "LL";
            libcurl.Variables["CURL_SUFFIX_CURL_OFF_TU"] = "ULL";
        }

        if (libcurl.Variables["CURL_TYPEOF_CURL_OFF_T"])
        {
            libcurl.Variables["CURL_TYPEOF_CURL_OFF_T"] = libcurl.Variables["ssize_t"];
            libcurl.Variables["CURL_SIZEOF_CURL_OFF_T"] = libcurl.Variables["SIZEOF_SSIZE_T"];
            libcurl.Variables["CURL_FORMAT_CURL_OFF_T"] = "ld";
            libcurl.Variables["CURL_FORMAT_CURL_OFF_TU"] = "lu";
            libcurl.Variables["CURL_FORMAT_OFF_T"] = "%ld";
            libcurl.Variables["CURL_SUFFIX_CURL_OFF_T"] = "L";
            libcurl.Variables["CURL_SUFFIX_CURL_OFF_TU"] = "UL";
        }

        if (libcurl.Variables["HAVE_LONG_LONG"])
        {
            libcurl.Variables["HAVE_LONGLONG"] = 1;
            libcurl.Variables["HAVE_LL"] = 1;
        }

        if (libcurl.Variables["HAVE_SOCKLEN_T"])
        {
            libcurl.Variables["CURL_TYPEOF_CURL_SOCKLEN_T"] = "socklen_t";
            libcurl.Variables["CURL_SIZEOF_CURL_SOCKLEN_T"] = libcurl.Variables["SIZEOF_SOCKLEN_T"];
        }
        else
        {
            libcurl.Variables["CURL_TYPEOF_CURL_SOCKLEN_T"] = "int";
            libcurl.Variables["CURL_SIZEOF_CURL_SOCKLEN_T"] = libcurl.Variables["SIZEOF_INT"];
        }

        for (auto &v : { "WS2TCPIP_H","SYS_TYPES_H","STDINT_H" ,"INTTYPES_H" ,"SYS_SOCKET_H" ,"SYS_POLL_H" })
        {
            libcurl.Variables["CURL_PULL_" + String(v)] = libcurl.Variables["HAVE_" + String(v)];
        }

        recv_args(libcurl);

        libcurl.configureFile("lib/curl_config.h.cmake", "curl_config.h");
    }

    auto &rhash = addTarget<LibraryTarget>(s, "pvt.cppan.demo.aleksey14.rhash", "1");
    {
        rhash.ApiName = "RHASH_API";
        rhash +=
            "librhash/.*\\.c"_rr,
            "librhash/.*\\.h"_rr,
            "win32/.*\\.h"_rr;

        rhash.Public +=
            "."_id,
            "librhash"_id;
    }

    auto &stacktrace = addTarget<LibraryTarget>(s, "pvt.cppan.demo.apolukhin.stacktrace", "master");
    {
        stacktrace +=
            "include/.*"_rr;

        stacktrace.Public +=
            "include"_id;

        stacktrace.Public +=
            *boost_targets["algorithm"],
            *boost_targets["align"],
            *boost_targets["array"],
            *boost_targets["assert"],
            *boost_targets["atomic"],
            *boost_targets["bind"],
            *boost_targets["chrono"],
            *boost_targets["concept_check"],
            *boost_targets["config"],
            *boost_targets["container"],
            *boost_targets["conversion"],
            *boost_targets["core"],
            *boost_targets["date_time"],
            *boost_targets["detail"],
            *boost_targets["endian"],
            *boost_targets["exception"],
            *boost_targets["filesystem"],
            *boost_targets["foreach"],
            *boost_targets["function"],
            *boost_targets["function_types"],
            *boost_targets["functional"],
            *boost_targets["fusion"],
            *boost_targets["integer"],
            *boost_targets["intrusive"],
            *boost_targets["io"],
            *boost_targets["iostreams"],
            *boost_targets["iterator"],
            *boost_targets["lambda"],
            *boost_targets["lexical_cast"],
            *boost_targets["locale"],
            *boost_targets["math"],
            *boost_targets["move"],
            *boost_targets["mpl"],
            *boost_targets["numeric"],
            *boost_targets["optional"],
            *boost_targets["phoenix"],
            *boost_targets["pool"],
            *boost_targets["predef"],
            *boost_targets["preprocessor"],
            *boost_targets["proto"],
            *boost_targets["random"],
            *boost_targets["range"],
            *boost_targets["ratio"],
            *boost_targets["rational"],
            *boost_targets["regex"],
            *boost_targets["serialization"],
            *boost_targets["smart_ptr"],
            *boost_targets["spirit"],
            *boost_targets["static_assert"],
            *boost_targets["system"],
            *boost_targets["thread"],
            *boost_targets["throw_exception"],
            *boost_targets["tokenizer"],
            *boost_targets["tti"],
            *boost_targets["tuple"],
            *boost_targets["type_index"],
            *boost_targets["type_traits"],
            *boost_targets["typeof"],
            *boost_targets["unordered"],
            *boost_targets["utility"],
            *boost_targets["variant"],
            *boost_targets["winapi"];
    }

    auto &date = addTarget<LibraryTarget>(s, "pvt.cppan.demo.howardhinnant.date.date", "2");
    /*{
        date +=
            "date.h";

        date.Public +=
            "."_id;
    }*/

    auto &sqlpp11 = addTarget<LibraryTarget>(s, "pvt.cppan.demo.rbock.sqlpp11", "0");
    {
        sqlpp11 +=
            "include/.*"_rr;

        sqlpp11.Public +=
            "include"_id;

        sqlpp11.Public += date;
    }

    auto &sqlpp11_connector_sqlite3 = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.rbock.sqlpp11_connector_sqlite3", "0");
    {
        sqlpp11_connector_sqlite3 +=
            "include/.*"_rr,
            "src/.*"_rr;

        sqlpp11_connector_sqlite3.Private +=
            "src"_id;

        sqlpp11_connector_sqlite3.Public +=
            "include"_id;

        sqlpp11_connector_sqlite3.Public += sqlpp11, sqlite3, date;
    }

    auto &turf = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.preshing.turf", "master");
    {
        turf +=
            "cmake/Macros.cmake",
            "cmake/turf_config.h.in",
            "turf/.*"_rr;

        turf.Public +=
            "."_id;

        turf.Variables["TURF_USERCONFIG"];
        turf.Variables["TURF_ENABLE_CPP11"] = "1";
        turf.Variables["TURF_WITH_BOOST"] = "FALSE";
        turf.Variables["TURF_WITH_EXCEPTIONS"] = "FALSE";
        if (s.Settings.Native.CompilerType == CompilerType::MSVC)
            turf.Variables["TURF_WITH_SECURE_COMPILER"] = "FALSE";
        turf.Variables["TURF_REPLACE_OPERATOR_NEW"] = "FALSE";

        turf.Variables["TURF_HAS_LONG_LONG"] = "1";
        turf.Variables["TURF_HAS_STDINT"] = "1";
        turf.Variables["TURF_HAS_NOEXCEPT"] = "1";
        turf.Variables["TURF_HAS_CONSTEXPR"] = "1";
        turf.Variables["TURF_HAS_OVERRIDE"] = "1";
        turf.Variables["TURF_HAS_STATIC_ASSERT"] = "1";
        turf.Variables["TURF_HAS_MOVE"] = "1";

        turf.configureFile("cmake/turf_config.h.in", "turf_config.h");
        turf.fileWriteOnce("turf_userconfig.h", "", true);
    }

    auto &junction = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.preshing.junction", "master");
    {
        junction +=
            "cmake/junction_config.h.in",
            "junction/.*"_rr;

        junction.Public +=
            "."_id;

        junction.Public += turf;

        junction.Variables["JUNCTION_TRACK_GRAMPA_STATS"] = "FALSE";
        junction.Variables["JUNCTION_USE_STRIPING"] = "TRUE";

        junction.configureFile("cmake/junction_config.h.in", "junction_config.h");
        junction.fileWriteOnce("junction_userconfig.h", "", true);
    }

    auto &argagg = addTarget<LibraryTarget>(s, "pvt.cppan.demo.vietjtnguyen.argagg", "0.4.6");
    {
        argagg.setChecks("argagg");
        argagg +=
            "include/.*"_rr;

        argagg.Public +=
            "include"_id;
    }

    auto &taywee_args = addTarget<LibraryTarget>(s, "pvt.cppan.demo.taywee.args", "6");
    {
        taywee_args += "args.hxx";
    }

    auto &fmt = addTarget<LibraryTarget>(s, "pvt.cppan.demo.fmt", "4");
    {
        fmt.setChecks("fmt");
        fmt +=
            "fmt/format.*"_rr,
            "fmt/ostream.*"_rr;

        fmt.Public +=
            "fmt"_id,
            "."_id;

        fmt.Private += sw::Shared, "FMT_EXPORT"_d;
        fmt.Public += sw::Shared, "FMT_SHARED"_d;
    }

    auto &flags = addTarget<LibraryTarget>(s, "pvt.cppan.demo.grisumbras.enum_flags", "master");

    auto &json = addTarget<LibraryTarget>(s, "pvt.cppan.demo.nlohmann.json", "3");

    auto &uv = addTarget<LibraryTarget>(s, "pvt.cppan.demo.libuv", "1");
    {
        uv.Private << sw::Shared << "BUILDING_UV_SHARED"_d;
        uv.Interface << sw::Shared << "USING_UV_SHARED"_d;
        uv += "src/.*"_r;
        if (s.Settings.TargetOS.Type == OSType::Windows)
        {
            uv += "src/win/.*"_rr;
            uv.Public += "iphlpapi.lib"_lib, "psapi.lib"_lib, "userenv.lib"_lib;
        }
        else
        {
            uv +=
                "src/unix/async.c",
                "src/unix/atomic-ops.h",
                "src/unix/core.c",
                "src/unix/dl.c",
                "src/unix/fs.c",
                "src/unix/getaddrinfo.c",
                "src/unix/getnameinfo.c",
                "src/unix/internal.h",
                "src/unix/loop-watcher.c",
                "src/unix/loop.c",
                "src/unix/pipe.c",
                "src/unix/poll.c",
                "src/unix/process.c",
                "src/unix/signal.c",
                "src/unix/spinlock.h",
                "src/unix/stream.c",
                "src/unix/tcp.c",
                "src/unix/thread.c",
                "src/unix/timer.c",
                "src/unix/tty.c",
                "src/unix/udp.c";

            switch (s.Settings.TargetOS.Type)
            {
            case OSType::AIX:
                break;
            case OSType::Android:
                break;
            case OSType::Macos:
                uv +=
                    "src/unix/darwin.c",
                    "src/unix/darwin-proctitle.c",
                    "src/unix/fsevents.c",
                    "src/unix/kqueue.c",
                    "src/unix/proctitle.c";
                break;
            case OSType::FreeBSD:
                break;
            case OSType::NetBSD:
                break;
            case OSType::OpenBSD:
                break;
            case OSType::SunOS:
                break;
            case OSType::Linux:
                uv +=
                    "src/unix/linux-core.c",
                    "src/unix/linux-inotify.c",
                    "src/unix/linux-syscalls.c",
                    "src/unix/linux-syscalls.h",
                    "src/unix/proctitle.c";
                break;
            }
        }
    }

    auto &pystring = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.imageworks.pystring", "1");
    pystring += "pystring.*"_rr;

    auto &ragel = addTarget<ExecutableTarget>(s, "pvt.cppan.demo.ragel", "6");
    {
        ragel += "aapl/.*"_rr;
        ragel += "ragel/.*\\.cpp"_rr;
        ragel += "ragel/.*\\.h"_rr;
        ragel += "aapl"_idir;
        ragel.writeFileOnce(ragel.BinaryPrivateDir / "config.h");
        if (s.Settings.TargetOS.Type == OSType::Windows)
            ragel.writeFileOnce(ragel.BinaryPrivateDir / "unistd.h");
    }

    auto rl = [&ragel](auto &t, const path &in)
    {
        auto o = t.BinaryDir / (in.filename().u8string() + ".cpp");

        auto c = std::make_shared<Command>();
        c->program = ragel.getOutputFile();
        c->args.push_back((t.SourceDir / in).u8string());
        c->args.push_back("-o");
        c->args.push_back(o.u8string());
        c->addInput(t.SourceDir / in);
        c->addOutput(o);
        t += o;
    };

    auto &winflexbison_common = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.lexxmark.winflexbison.common", "master");
    {
        winflexbison_common += "common/.*"_rr;
        winflexbison_common -= "common/m4/lib/regcomp.c";
        winflexbison_common -= "common/m4/lib/regexec.c";
        winflexbison_common -= ".*\\.def"_rr;
        winflexbison_common.Public += "common/m4/lib"_idir;
        winflexbison_common.Public += "common/misc"_idir;
    }

    auto &winflexbison_flex = addTarget<ExecutableTarget>(s, "pvt.cppan.demo.lexxmark.winflexbison.flex", "master");
    {
        winflexbison_flex += "flex/.*"_rr;
        winflexbison_flex -= "flex/src/libmain.c";
        winflexbison_flex -= "flex/src/libyywrap.c";
        winflexbison_flex += winflexbison_common;
    }

    auto &winflexbison_bison = addTarget<ExecutableTarget>(s, "pvt.cppan.demo.lexxmark.winflexbison.bison", "master");
    {
        //winflexbison_bison += "bison/data/[^/]*"_r;
        winflexbison_bison += "bison/data/m4sugar/.*"_rr;
        winflexbison_bison += "bison/src/.*"_rr;
        winflexbison_bison.Public += "bison/src"_idir;
        winflexbison_bison += winflexbison_common;
        winflexbison_bison.replaceInFileOnce("bison/src/config.h", "data", normalize_path(winflexbison_bison.SourceDir / "bison/data/"));
        winflexbison_bison.replaceInFileOnce("bison/src/main.c", "if (!last_divider)", "");
        winflexbison_bison.replaceInFileOnce("bison/src/main.c", "free(local_pkgdatadir);", "");
    }

    auto flex_bison = [&winflexbison_bison, &winflexbison_flex](auto &t, const path &f, const path &b, const Strings &flex_args = {}, const Strings &bison_args = {})
    {
        auto d = b.filename();
        auto bdir = t.BinaryPrivateDir / "fb" / d;

        auto o = bdir / (b.filename().u8string() + ".cpp");
        auto oh = bdir / (b.filename().u8string() + ".hpp");
        t += IncludeDirectory(oh.parent_path());

        fs::create_directories(bdir);

        {
            auto c = std::make_shared<Command>();
            c->program = winflexbison_bison.getOutputFile();
            c->working_directory = bdir;
            c->args.push_back("-o");
            c->args.push_back(o.u8string());
            c->args.push_back("--defines=" + oh.u8string());
            c->args.insert(c->args.end(), bison_args.begin(), bison_args.end());
            c->args.push_back((t.SourceDir / b).u8string());
            c->addInput(t.SourceDir / b);
            c->addOutput(o);
            c->addOutput(oh);
            t += o, oh;
        }

        {
            auto o = bdir / (f.filename().u8string() + ".cpp");

            auto c = std::make_shared<Command>();
            c->program = winflexbison_flex.getOutputFile();
            c->working_directory = bdir;
            c->args.push_back("-o");
            c->args.push_back(o.u8string());
            c->args.insert(c->args.end(), flex_args.begin(), flex_args.end());
            c->args.push_back((t.SourceDir / f).u8string());
            c->addInput(t.SourceDir / f);
            c->addInput(oh);
            c->addOutput(o);
            t += o;
        }
    };

    auto flex_bison_pair = [&flex_bison](auto &t, const String &type, const path &p)
    {
        auto name = p.filename().string();
        auto name_upper = boost::to_upper_copy(name);
        auto my_parser = name + "Parser";
        my_parser[0] = toupper(my_parser[0]);

        t.Definitions["HAVE_BISON_" + name_upper + "_PARSER"];

        Context ctx;
        ctx.addLine("#pragma once");
        ctx.addLine();
        ctx.addLine("#undef  THIS_PARSER_NAME");
        ctx.addLine("#undef  THIS_PARSER_NAME_UP");
        ctx.addLine("#undef  THIS_LEXER_NAME");
        ctx.addLine("#undef  THIS_LEXER_NAME_UP");
        ctx.addLine();
        ctx.addLine("#define THIS_PARSER_NAME       " + name);
        ctx.addLine("#define THIS_PARSER_NAME_UP    " + name_upper);
        ctx.addLine("#define THIS_LEXER_NAME        THIS_PARSER_NAME");
        ctx.addLine("#define THIS_LEXER_NAME_UP     THIS_PARSER_NAME_UP");
        ctx.addLine();
        ctx.addLine("#undef  MY_PARSER");
        ctx.addLine("#define MY_PARSER              " + my_parser);
        ctx.addLine();
        ctx.addLine("#define " + type);
        ctx.addLine("#include <primitives/helper/bison.h>");
        ctx.addLine("#undef  " + type);
        ctx.addLine();
        ctx.addLine("#include <" + name + ".yy.hpp>");

        t.writeFileOnce(t.BinaryPrivateDir / (name + "_parser.h"), ctx.getText());
        t.Definitions["HAVE_BISON_" + name_upper + "_PARSER"] = 1;

        auto f = p;
        auto b = p;
        flex_bison(t, f += ".ll", b += ".yy", { "--prefix=ll_" + name }, { "-Dapi.prefix={yy_" + name + "}" });
    };

    /// llvm

    auto &llvm_demangle = addTarget<StaticLibraryTarget>(s, "pvt.egorpugin.llvm.demangle", "master");
    {
        llvm_demangle +=
            "include/llvm/Demangle/.*"_rr,
            "lib/Demangle/.*\\.cpp"_rr,
            "lib/Demangle/.*\\.h"_rr;
    }

    auto &llvm_support_lite = addTarget<StaticLibraryTarget>(s, "pvt.egorpugin.llvm.support_lite", "master");
    {
        llvm_support_lite.setChecks("support_lite");
        llvm_support_lite +=
            "include/llvm-c/.*Types\\.h"_rr,
            "include/llvm-c/ErrorHandling.h",
            "include/llvm-c/Support.h",
            "include/llvm/ADT/.*\\.h"_rr,
            "include/llvm/Config/.*\\.cmake"_rr,
            "include/llvm/Support/.*"_rr,
            "lib/Support/.*\\.c"_rr,
            "lib/Support/.*\\.cpp"_rr,
            "lib/Support/.*\\.h"_rr,
            "lib/Support/.*\\.inc"_rr;
        llvm_support_lite -=
            "include/llvm/Support/.*def"_rr;
        llvm_support_lite.Private +=
            "lib"_id;
        llvm_support_lite.Public +=
            "include"_id;
        if (s.Settings.TargetOS.Type != OSType::Windows)
            llvm_support_lite.Private += "HAVE_PTHREAD_GETSPECIFIC"_d;
        llvm_support_lite.Public += llvm_demangle;

        llvm_support_lite += "LLVM_ENABLE_THREADS=1"_v;
        llvm_support_lite += "LLVM_HAS_ATOMICS=1"_v;
        if (s.Settings.TargetOS.Type == OSType::Windows)
            llvm_support_lite += "LLVM_HOST_TRIPLE=\"unknown-unknown-windows\""_v;
        else
        {
            llvm_support_lite += "LLVM_HOST_TRIPLE=\"unknown-unknown-unknown\""_v;
            llvm_support_lite += "LLVM_ON_UNIX=1"_v;
        }
        llvm_support_lite += "RETSIGTYPE=void"_v;

        llvm_support_lite += "LLVM_VERSION_MAJOR=0"_v;
        llvm_support_lite += "LLVM_VERSION_MINOR=0"_v;
        llvm_support_lite += "LLVM_VERSION_PATCH=1"_v;

        llvm_support_lite.configureFile("include/llvm/Config/config.h.cmake", "llvm/Config/config.h");
        llvm_support_lite.configureFile("include/llvm/Config/llvm-config.h.cmake", "llvm/Config/llvm-config.h");
        llvm_support_lite.configureFile("include/llvm/Config/abi-breaking.h.cmake", "llvm/Config/abi-breaking.h");
    }

    /// protobuf

    auto import_from_bazel = [](auto &t)
    {
        t.ImportFromBazel = true;
    };

    auto &protobuf_lite = addTarget<LibraryTarget>(s, "pvt.cppan.demo.google.protobuf.protobuf_lite", "3");
    import_from_bazel(protobuf_lite);
    protobuf_lite += "src/google/protobuf/.*\\.h"_rr;
    //protobuf_lite.Public += "src"_idir;
    protobuf_lite += sw::Shared, "LIBPROTOBUF_EXPORTS";
    protobuf_lite.Public += sw::Shared, "PROTOBUF_USE_DLLS";

    auto &protobuf = addTarget<LibraryTarget>(s, "pvt.cppan.demo.google.protobuf.protobuf", "3");
    import_from_bazel(protobuf);
    protobuf += ".*"_rr;
    protobuf += FileRegex(protobuf_lite.SourceDir, std::regex(".*"), true);
    protobuf.Public += protobuf_lite, zlib;
    //protobuf.Public += "src"_idir;
    protobuf += sw::Shared, "LIBPROTOBUF_EXPORTS";
    protobuf.Public += sw::Shared, "PROTOBUF_USE_DLLS";

    auto &protoc_lib = addTarget<LibraryTarget>(s, "pvt.cppan.demo.google.protobuf.protoc_lib", "3");
    import_from_bazel(protoc_lib);
    protoc_lib.Public += protobuf;
    protoc_lib += sw::Shared, "LIBPROTOC_EXPORTS";
    protoc_lib.Public += sw::Shared, "PROTOBUF_USE_DLLS";

    auto &protoc = addTarget<ExecutableTarget>(s, "pvt.cppan.demo.google.protobuf.protoc", "3");
    import_from_bazel(protoc);
    protoc.Public += protoc_lib;

    auto gen_pb = [&protoc](auto &t, const path &f)
    {
        auto n = f.filename().stem().u8string();
        auto d = f.parent_path();
        auto bdir = t.BinaryDir;

        auto o = bdir / n;
        auto ocpp = o;
        ocpp += ".pb.cc";
        auto oh = o;
        oh += ".pb.h";

        auto c = std::make_shared<Command>();
        c->program = protoc.getOutputFile();
        c->working_directory = bdir;
        c->args.push_back(f.u8string());
        c->args.push_back("--cpp_out=" + bdir.u8string());
        c->args.push_back("-I");
        c->args.push_back(d.u8string());
        c->args.push_back("-I");
        c->args.push_back((protoc.SourceDir / "src").u8string());
        c->addInput(f);
        c->addOutput(ocpp);
        c->addOutput(oh);
        t += ocpp, oh;
    };

    /// grpc

    auto setup_grpc = [&import_from_bazel](auto &t)
    {
        import_from_bazel(t);
        t += ".*"_rr;
        t.Public.IncludeDirectories.insert(t.SourceDir);
        t.Public.IncludeDirectories.insert(t.SourceDir / "include");
    };

    auto &grpcpp_config_proto = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpcpp_config_proto", "1");
    setup_grpc(grpcpp_config_proto);

    auto &grpc_plugin_support = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_plugin_support", "1");
    setup_grpc(grpc_plugin_support);
    grpc_plugin_support.Public += grpcpp_config_proto, protoc_lib;

    auto &grpc_cpp_plugin = addTarget<ExecutableTarget>(s, "pvt.cppan.demo.google.grpc.grpc_cpp_plugin", "1");
    setup_grpc(grpc_cpp_plugin);
    grpc_cpp_plugin.Public += grpc_plugin_support;

    auto gen_grpc = [&gen_pb, &grpc_cpp_plugin, &protoc](auto &t, const path &f)
    {
        gen_pb(t, f);

        auto n = f.filename().stem().u8string();
        auto d = f.parent_path();
        auto bdir = t.BinaryDir;

        auto o = bdir / n;
        auto ocpp = o;
        ocpp += ".grpc.pb.cc";
        auto oh = o;
        oh += ".grpc.pb.h";

        auto c = std::make_shared<Command>();
        c->program = protoc.getOutputFile();
        c->working_directory = bdir;
        c->args.push_back(f.u8string());
        c->args.push_back("--grpc_out=" + bdir.u8string());
        c->args.push_back("--plugin=protoc-gen-grpc=" + grpc_cpp_plugin.getOutputFile().u8string());
        c->args.push_back("-I");
        c->args.push_back(d.u8string());
        c->args.push_back("-I");
        c->args.push_back((protoc.SourceDir / "src").u8string());
        c->addInput(f);
        c->addInput(grpc_cpp_plugin.getOutputFile());
        c->addOutput(ocpp);
        c->addOutput(oh);
        t += ocpp, oh;
    };

    auto &gpr_codegen = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.gpr_codegen", "1");
    setup_grpc(gpr_codegen);
    if (s.Settings.TargetOS.Type == OSType::Windows)
        gpr_codegen.Public += "_WIN32_WINNT=0x0600"_d;

    auto &gpr_base = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.gpr_base", "1");
    setup_grpc(gpr_base);
    gpr_base.Public += gpr_codegen;

    auto &gpr = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.gpr", "1");
    setup_grpc(gpr);
    gpr.Public += gpr_base;

    auto &nanopb = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.nanopb", "0");
    nanopb += "[^/]*\\.[hc]"_rr;
    nanopb.Public += "PB_FIELD_32BIT"_d;

    auto &grpc_nanopb = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.third_party.nanopb", "1");
    grpc_nanopb += "third_party/nanopb/[^/]*\\.[hc]"_rr;
    grpc_nanopb.Public += "PB_FIELD_32BIT"_d;

    auto &atomic = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.atomic", "1");
    setup_grpc(atomic);
    atomic.Public += gpr;

    auto &grpc_codegen = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_codegen", "1");
    setup_grpc(grpc_codegen);
    grpc_codegen.Public += gpr_codegen;

    auto &grpc_trace = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_trace", "1");
    setup_grpc(grpc_trace);
    grpc_trace.Public += gpr, grpc_codegen;

    auto &inlined_vector = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.inlined_vector", "1");
    setup_grpc(inlined_vector);
    inlined_vector.Public += gpr_base;

    auto &debug_location = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.debug_location", "1");
    setup_grpc(debug_location);

    auto &ref_counted_ptr = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.ref_counted_ptr", "1");
    setup_grpc(ref_counted_ptr);
    gpr_base.Public += gpr_base;

    auto &ref_counted = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.ref_counted", "1");
    setup_grpc(ref_counted);
    ref_counted.Public += debug_location, gpr_base, grpc_trace, ref_counted_ptr;

    auto &orphanable = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.orphanable", "1");
    setup_grpc(orphanable);
    orphanable.Public += debug_location, gpr_base, grpc_trace, ref_counted_ptr;

    auto &grpc_base_c = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_base_c", "1");
    setup_grpc(grpc_base_c);
    grpc_base_c.Public += gpr_base, grpc_trace, inlined_vector, orphanable, ref_counted, zlib;

    auto &grpc_base = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_base", "1");
    setup_grpc(grpc_base);
    grpc_base.Public += grpc_base_c, atomic;

    auto &census = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.census", "1");
    setup_grpc(census);
    census.Public += grpc_base, grpc_nanopb;

    auto &grpc_client_authority_filter = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_client_authority_filter", "1");
    setup_grpc(grpc_client_authority_filter);
    grpc_client_authority_filter.Public += grpc_base;

    auto &grpc_deadline_filter = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_deadline_filter", "1");
    setup_grpc(grpc_deadline_filter);
    grpc_deadline_filter.Public += grpc_base;

    auto &grpc_client_channel = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_client_channel", "1");
    setup_grpc(grpc_client_channel);
    grpc_client_channel.Public += gpr_base, grpc_base, grpc_client_authority_filter, grpc_deadline_filter, inlined_vector,
        orphanable, ref_counted, ref_counted_ptr;

    auto &grpc_lb_subchannel_list = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_lb_subchannel_list", "1");
    setup_grpc(grpc_lb_subchannel_list);
    grpc_lb_subchannel_list.Public += grpc_base, grpc_client_channel;

    auto &grpc_lb_policy_pick_first = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_lb_policy_pick_first", "1");
    setup_grpc(grpc_lb_policy_pick_first);
    grpc_lb_policy_pick_first.Public += grpc_base, grpc_client_channel, grpc_lb_subchannel_list;

    auto &grpc_lb_policy_round_robin = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_lb_policy_round_robin", "1");
    setup_grpc(grpc_lb_policy_round_robin);
    grpc_lb_policy_round_robin.Public += grpc_lb_subchannel_list;

    auto &grpc_max_age_filter = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_max_age_filter", "1");
    setup_grpc(grpc_max_age_filter);
    grpc_max_age_filter.Public += grpc_base;

    auto &grpc_message_size_filter = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_message_size_filter", "1");
    setup_grpc(grpc_message_size_filter);
    grpc_message_size_filter.Public += grpc_base;

    auto &grpc_resolver_dns_ares = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_resolver_dns_ares", "1");
    setup_grpc(grpc_resolver_dns_ares);
    grpc_resolver_dns_ares.Public += grpc_base, grpc_client_channel, c_ares;

    auto &grpc_resolver_dns_native = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_resolver_dns_native", "1");
    setup_grpc(grpc_resolver_dns_native);
    grpc_resolver_dns_native.Public += grpc_base, grpc_client_channel;

    auto &grpc_resolver_fake = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_resolver_fake", "1");
    setup_grpc(grpc_resolver_fake);
    grpc_resolver_fake.Public += grpc_base, grpc_client_channel;

    auto &grpc_resolver_sockaddr = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_resolver_sockaddr", "1");
    setup_grpc(grpc_resolver_sockaddr);
    grpc_resolver_sockaddr.Public += grpc_base, grpc_client_channel;

    auto &grpc_server_backward_compatibility = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_server_backward_compatibility", "1");
    setup_grpc(grpc_server_backward_compatibility);
    grpc_server_backward_compatibility.Public += grpc_base;

    auto &grpc_server_load_reporting = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_server_load_reporting", "1");
    setup_grpc(grpc_server_load_reporting);
    grpc_server_load_reporting.Public += grpc_base;

    auto &grpc_http_filters = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_http_filters", "1");
    setup_grpc(grpc_http_filters);
    grpc_http_filters.Public += grpc_base;

    auto &grpc_transport_chttp2_alpn = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_transport_chttp2_alpn", "1");
    setup_grpc(grpc_transport_chttp2_alpn);
    grpc_transport_chttp2_alpn.Public += gpr;

    auto &grpc_transport_chttp2 = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_transport_chttp2", "1");
    setup_grpc(grpc_transport_chttp2);
    grpc_transport_chttp2.Public += gpr_base, grpc_base, grpc_http_filters, grpc_transport_chttp2_alpn;

    auto &grpc_transport_chttp2_client_connector = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_transport_chttp2_client_connector", "1");
    setup_grpc(grpc_transport_chttp2_client_connector);
    grpc_transport_chttp2_client_connector.Public += grpc_base, grpc_client_channel, grpc_transport_chttp2;

    auto &grpc_transport_chttp2_client_insecure = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_transport_chttp2_client_insecure", "1");
    setup_grpc(grpc_transport_chttp2_client_insecure);
    grpc_transport_chttp2_client_insecure.Public += grpc_base, grpc_client_channel, grpc_transport_chttp2, grpc_transport_chttp2_client_connector;

    auto &grpc_transport_chttp2_server = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_transport_chttp2_server", "1");
    setup_grpc(grpc_transport_chttp2_server);
    grpc_transport_chttp2_server.Public += grpc_base, grpc_transport_chttp2;

    auto &grpc_transport_chttp2_server_insecure = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_transport_chttp2_server_insecure", "1");
    setup_grpc(grpc_transport_chttp2_server_insecure);
    grpc_transport_chttp2_server_insecure.Public += grpc_base, grpc_transport_chttp2, grpc_transport_chttp2_server;

    auto &grpc_transport_inproc = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_transport_inproc", "1");
    setup_grpc(grpc_transport_inproc);
    grpc_transport_inproc.Public += grpc_base;

    auto &grpc_workaround_cronet_compression_filter = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_workaround_cronet_compression_filter", "1");
    setup_grpc(grpc_workaround_cronet_compression_filter);
    grpc_workaround_cronet_compression_filter.Public += grpc_server_backward_compatibility;

    auto &grpc_common = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_common", "1");
    setup_grpc(grpc_common);
    grpc_common.Public += census, grpc_base, grpc_client_authority_filter, grpc_deadline_filter, grpc_lb_policy_pick_first,
        grpc_lb_policy_round_robin, grpc_max_age_filter, grpc_message_size_filter, grpc_resolver_dns_ares, grpc_resolver_dns_native,
        grpc_resolver_fake, grpc_resolver_sockaddr, grpc_server_backward_compatibility, grpc_server_load_reporting, grpc_transport_chttp2_client_insecure,
        grpc_transport_chttp2_server_insecure, grpc_transport_inproc, grpc_workaround_cronet_compression_filter;

    auto &alts_proto = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.alts_proto", "1");
    setup_grpc(alts_proto);
    alts_proto.Public += nanopb;

    auto &alts_util = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.alts_util", "1");
    setup_grpc(alts_util);
    alts_util.Public += alts_proto, gpr, grpc_base;

    auto &tsi_interface = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.tsi_interface", "1");
    setup_grpc(tsi_interface);
    tsi_interface.Public += gpr, grpc_trace;

    auto &alts_frame_protector = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.alts_frame_protector", "1");
    setup_grpc(alts_frame_protector);
    alts_frame_protector.Public += gpr, grpc_base, tsi_interface, ssl;

    auto &tsi = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.tsi", "1");
    setup_grpc(tsi);
    tsi.Public += alts_frame_protector, alts_util, gpr, grpc_base, grpc_transport_chttp2_client_insecure, tsi_interface;

    auto &grpc_secure = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_secure", "1");
    setup_grpc(grpc_secure);
    grpc_secure.Public += alts_util, grpc_base, grpc_transport_chttp2_alpn, tsi;

    auto &grpc_lb_policy_grpclb_secure = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_lb_policy_grpclb_secure", "1");
    setup_grpc(grpc_lb_policy_grpclb_secure);
    grpc_lb_policy_grpclb_secure.Public += grpc_base, grpc_client_channel, grpc_resolver_fake, grpc_secure, grpc_nanopb;

    auto &grpc_transport_chttp2_client_secure = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_transport_chttp2_client_secure", "1");
    setup_grpc(grpc_transport_chttp2_client_secure);
    grpc_transport_chttp2_client_secure.Public += grpc_base, grpc_client_channel, grpc_secure, grpc_transport_chttp2, grpc_transport_chttp2_client_connector;

    auto &grpc_transport_chttp2_server_secure = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc_transport_chttp2_server_secure", "1");
    setup_grpc(grpc_transport_chttp2_server_secure);
    grpc_transport_chttp2_server_secure.Public += grpc_base, grpc_secure, grpc_transport_chttp2, grpc_transport_chttp2_server;

    auto &grpc = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpc", "1");
    setup_grpc(grpc);
    grpc.Public += grpc_common, grpc_lb_policy_grpclb_secure, grpc_secure, grpc_transport_chttp2_client_secure,
        grpc_transport_chttp2_server_secure;

    auto &grpcpp_codegen_base = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpcpp_codegen_base", "1");
    setup_grpc(grpcpp_codegen_base);
    grpcpp_codegen_base.Public += grpc_codegen;

    auto &grpcpp_base = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpcpp_base", "1");
    setup_grpc(grpcpp_base);
    grpcpp_base.Public += grpc, grpcpp_codegen_base;

    auto &grpcpp_codegen_base_src = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpcpp_codegen_base_src", "1");
    setup_grpc(grpcpp_codegen_base_src);
    grpcpp_codegen_base_src.Public += grpcpp_codegen_base;

    auto &grpcpp_codegen_proto = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpcpp_codegen_proto", "1");
    setup_grpc(grpcpp_codegen_proto);
    grpcpp_codegen_proto.Public += grpcpp_codegen_base, grpcpp_config_proto;

    auto &grpcpp = addTarget<StaticLibraryTarget>(s, "pvt.cppan.demo.google.grpc.grpcpp", "1");
    setup_grpc(grpcpp);
    grpcpp.Public += gpr, grpc, grpcpp_base, grpcpp_codegen_base, grpcpp_codegen_base_src, grpcpp_codegen_proto;

    /// primitives

    const path cppan2_base = path(__FILE__).parent_path().parent_path().parent_path().parent_path().parent_path();
    path primitives_base = getDirectories().storage_dir_tmp / "primitives";
    if (fs::exists("d:/dev/primitives"))
        primitives_base = "d:/dev/primitives";
    else if (!fs::exists(primitives_base))
        primitives::Command::execute({ "git", "clone", "https://github.com/egorpugin/primitives", primitives_base.u8string() });

    auto setup_primitives = [&primitives_base](auto &t)
    {
        auto n = t.getPackage().getPath().back();
        t.SourceDir = primitives_base / ("src/" + n);
        t.ApiName = "PRIMITIVES_" + boost::to_upper_copy(n) + "_API";
        t.CPPVersion = CPPLanguageStandard::CPP17;
        t += ".*"_rr; // explicit!
    };

    auto setup_primitives2 = [&primitives_base](auto &t, const path &subdir)
    {
        auto n = t.getPackage().getPath().back();
        t.SourceDir = primitives_base / ("src/" + subdir.u8string() + "/" + n);
        t.ApiName = "PRIMITIVES_" + boost::to_upper_copy(subdir.u8string() + "_" + n) + "_API";
        t.CPPVersion = CPPLanguageStandard::CPP17;
        t += ".*"_rr; // explicit!
    };

    // primitives
    auto &p_string = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.string", "master");
    p_string.Public += *boost_targets["algorithm"];
    setup_primitives(p_string);

    auto &p_filesystem = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.filesystem", "master");
    p_filesystem.Public += p_string, *boost_targets["filesystem"], *boost_targets["thread"], flags, uv;
    setup_primitives(p_filesystem);

    auto &p_templates = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.templates", "master");
    setup_primitives(p_templates);

    auto &p_context = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.context", "master");
    setup_primitives(p_context);

    auto &p_minidump = addTarget<StaticLibraryTarget>(s, "pvt.egorpugin.primitives.minidump", "master");
    setup_primitives(p_minidump);
    if (s.Settings.TargetOS.Type == OSType::Windows)
        p_minidump.Public += "dbghelp.lib"_lib;

    auto &p_executor = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.executor", "master");
    p_executor.Public += *boost_targets["asio"], *boost_targets["system"], p_templates, p_minidump;
    setup_primitives(p_executor);

    auto &p_command = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.command", "master");
    p_command.Public += p_filesystem, p_templates, *boost_targets["process"], uv;
    setup_primitives(p_command);
    if (s.Settings.TargetOS.Type == OSType::Windows)
        p_command.Public += "Shell32.lib"_l;

    auto &p_date_time = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.date_time", "master");
    p_date_time.Public += p_string, *boost_targets["date_time"];
    setup_primitives(p_date_time);

    auto &p_lock = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.lock", "master");
    p_lock.Public += p_filesystem, *boost_targets["interprocess"];
    setup_primitives(p_lock);

    auto &p_log = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.log", "master");
    p_log.Public += *boost_targets["log"];
    setup_primitives(p_log);

    auto &p_yaml = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.yaml", "master");
    p_yaml.Public += p_string, yaml_cpp;
    setup_primitives(p_yaml);

    auto &p_pack = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.pack", "master");
    p_pack.Public += p_filesystem, p_templates, libarchive;
    setup_primitives(p_pack);

    auto &p_http = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.http", "master");
    p_http.Public += p_filesystem, p_templates, libcurl;
    setup_primitives(p_http);
    if (s.Settings.TargetOS.Type == OSType::Windows)
        p_http.Public += "Winhttp.lib"_l;

    auto &p_hash = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.hash", "master");
    p_hash.Public += p_filesystem, rhash, crypto;
    setup_primitives(p_hash);

    auto &p_win32helpers = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.win32helpers", "master");
    p_win32helpers.Public += p_filesystem, *boost_targets["dll"], *boost_targets["algorithm"];
    setup_primitives(p_win32helpers);
    if (s.Settings.TargetOS.Type == OSType::Windows)
        p_win32helpers.Public += "UNICODE"_d;

    auto &p_db_common = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.db.common", "master");
    p_db_common.Public += p_filesystem, p_templates, pystring;
    setup_primitives2(p_db_common, "db");

    auto &p_db_sqlite3 = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.db.sqlite3", "master");
    p_db_sqlite3.Public += p_db_common, sqlite3;
    setup_primitives2(p_db_sqlite3, "db");

    auto &p_error_handling = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.error_handling", "master");
    setup_primitives(p_error_handling);

    auto &p_main = addTarget<StaticLibraryTarget>(s, "pvt.egorpugin.primitives.main", "master");
    p_main.Public += p_error_handling;
    setup_primitives(p_main);

    auto &p_settings = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.settings", "master");
    p_settings.Public += p_yaml, p_filesystem, p_templates, llvm_support_lite;
    setup_primitives(p_settings);
    flex_bison_pair(p_settings, "LALR1_CPP_VARIANT_PARSER", "src/settings");
    flex_bison_pair(p_settings, "LALR1_CPP_VARIANT_PARSER", "src/path");

    auto &p_sw_settings = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.sw.settings", "master");
    p_sw_settings.Public += p_settings;
    p_sw_settings.Interface += "src/sw.settings.program_name.cpp";
    setup_primitives2(p_sw_settings, "sw");

    auto &p_sw_main = addTarget<StaticLibraryTarget>(s, "pvt.egorpugin.primitives.sw.main", "master");
    p_sw_main.Public += p_main, p_sw_settings;
    setup_primitives2(p_sw_main, "sw");

    auto &p_tools_embedder = addTarget<ExecutableTarget>(s, "pvt.egorpugin.primitives.tools.embedder", "master");
    p_tools_embedder.SourceDir = primitives_base / "src" / "tools";
    p_tools_embedder += "embedder.cpp";
    p_tools_embedder.CPPVersion = CPPLanguageStandard::CPP17;
    p_tools_embedder += p_filesystem, p_sw_main;

    auto &p_tools_sqlite2cpp = addTarget<ExecutableTarget>(s, "pvt.egorpugin.primitives.tools.sqlpp11.sqlite2cpp", "master");
    p_tools_sqlite2cpp.SourceDir = primitives_base / "src" / "tools";
    p_tools_sqlite2cpp += "sqlpp11.sqlite2cpp.cpp";
    p_tools_sqlite2cpp.CPPVersion = CPPLanguageStandard::CPP17;
    p_tools_sqlite2cpp += p_filesystem, p_context, p_sw_main, sqlite3;

    auto gen_sql = [&p_tools_sqlite2cpp](auto &t, const auto &sql_file, const auto &out_file, const String &ns)
    {
        auto c = std::make_shared<Command>();
        c->program = p_tools_sqlite2cpp.getOutputFile();
        c->args.push_back(sql_file.u8string());
        c->args.push_back((t.BinaryDir / out_file).u8string());
        c->args.push_back(ns);
        c->addInput(sql_file);
        c->addOutput(t.BinaryDir / out_file);
        t += t.BinaryDir / out_file;
    };

    auto &p_version = addTarget<LibraryTarget>(s, "pvt.egorpugin.primitives.version", "master");
    p_version.Public += p_hash, p_templates, fmt, pystring;
    setup_primitives(p_version);
    rl(p_version, "src/version.rl");
    flex_bison_pair(p_version, "GLR_CPP_PARSER", "src/range");

    /// self

    // setting to local makes build files go to working dir
    //s.Local = true;

    {
        auto &support = s.addTarget<LibraryTarget>("support");
        support.CPPVersion = CPPLanguageStandard::CPP17;
        support.Public += p_http, p_hash, p_command, p_log, p_executor, *boost_targets["property_tree"], *boost_targets["stacktrace"], *boost_targets["dll"];
        support.SourceDir = cppan2_base / "src/support";
        support += ".*"_rr;
        support.ApiName = "SW_SUPPORT_API";
        if (s.Settings.TargetOS.Type == OSType::Windows)
            support.Public += "UNICODE"_d;

        auto &protos = s.addTarget<StaticLibraryTarget>("protos");
        protos.CPPVersion = CPPLanguageStandard::CPP17;
        protos.SourceDir = cppan2_base / "src" / "protocol";
        protos += ".*"_rr;
        protos.Public += protobuf, grpcpp, p_log;
        gen_grpc(protos, protos.SourceDir / "api.proto");

        auto &manager = s.addTarget<LibraryTarget>("manager");
        manager.ApiName = "SW_MANAGER_API";
        //manager.ExportIfStatic = true;
        manager.CPPVersion = CPPLanguageStandard::CPP17;
        manager.Public += support, protos, p_yaml, p_date_time, p_lock, p_pack, json,
            *boost_targets["variant"], *boost_targets["dll"], p_db_sqlite3, sqlpp11_connector_sqlite3, stacktrace, p_version, p_win32helpers;
        manager.SourceDir = cppan2_base;
        manager += "src/manager/.*"_rr, "include/manager/.*"_rr;
        manager.Public += "include"_idir, "src/manager"_idir;
        manager.Public += "VERSION_MAJOR=0"_d;
        manager.Public += "VERSION_MINOR=3"_d;
        manager.Public += "VERSION_PATCH=0"_d;
        {
            auto c = std::make_shared<Command>();
            c->program = p_tools_embedder.getOutputFile();
            c->working_directory = manager.SourceDir / "src/manager/inserts";
            c->args.push_back((manager.SourceDir / "src/manager/inserts/inserts.cpp.in").u8string());
            c->args.push_back((manager.BinaryDir / "inserts.cpp").u8string());
            c->addInput(manager.SourceDir / "src/builder/manager/inserts.cpp.in");
            c->addOutput(manager.BinaryDir / "inserts.cpp");
            manager += manager.BinaryDir / "inserts.cpp";
        }
        gen_sql(manager, manager.SourceDir / "src/manager/inserts/packages_db_schema.sql", "db_packages.h", "db::packages");
        gen_sql(manager, manager.SourceDir / "src/manager/inserts/service_db_schema.sql", "db_service.h", "db::service");

        auto &builder = s.addTarget<LibraryTarget>("builder");
        builder.ApiName = "SW_BUILDER_API";
        //builder.ExportIfStatic = true;
        builder.CPPVersion = CPPLanguageStandard::CPP17;
        builder.Public += manager, junction;
        builder.SourceDir = cppan2_base;
        builder += "src/builder/.*"_rr, "include/builder/.*"_rr;
        builder.Public += "include"_idir, "src/builder"_idir;
        builder -= "src/builder/db_sqlite.*"_rr;

        auto &cpp_driver = s.addTarget<LibraryTarget>("driver.cpp");
        cpp_driver.ApiName = "SW_DRIVER_CPP_API";
        //cpp_driver.ExportIfStatic = true;
        cpp_driver.CPPVersion = CPPLanguageStandard::CPP17;
        cpp_driver.Public += builder, *boost_targets["assign"], *boost_targets["uuid"], p_context;
        cpp_driver.SourceDir = cppan2_base;
        cpp_driver += "src/driver/cpp/.*"_rr, "include/driver/cpp/.*"_rr;
        cpp_driver.Public += "include"_idir, "src/driver/cpp"_idir;
        {
            auto c = std::make_shared<Command>();
            c->program = p_tools_embedder.getOutputFile();
            c->working_directory = cpp_driver.SourceDir / "src/driver/cpp/inserts";
            c->args.push_back((cpp_driver.SourceDir / "src/driver/cpp/inserts/inserts.cpp.in").u8string());
            c->args.push_back((cpp_driver.BinaryDir / "inserts.cpp").u8string());
            c->addInput(cpp_driver.SourceDir / "src/driver/cpp/inserts/inserts.cpp.in");
            c->addOutput(cpp_driver.BinaryDir / "inserts.cpp");
            cpp_driver += cpp_driver.BinaryDir / "inserts.cpp";
        }



        /*{
            auto in = builder.SourceDir.parent_path() / "inserts/inserts.cpp.in";
            auto out = builder.BinaryDir / "inserts.cpp";
            auto c = std::make_shared<Command>();
            c->program = inserter.getOutputFile();
            c->args.push_back(in.string());
            c->args.push_back(out.string());
            c->addInput(in);
            c->addOutput(out);
            c->working_directory = builder.SourceDir.parent_path();
            builder += out;
        }

        auto &client = s.addTarget<ExecutableTarget>("client");
        client.CPPVersion = CPPLanguageStandard::CPP17;
        client += builder, taywee_args;
        client.SourceDir = cppan2_base / "src/client";
        client += ".*"_rr;*/

        //s.TargetsToBuild.add(client);
    }
}

void check_self(Checker &c)
{
    {
        auto &s = c.addSet("libcharset");
        for (auto &f : { "nl_langinfo",
                        "setlocale" })
        {
            s.checkFunctionExists(f);
        }
        for (auto &f : { "dlfcn.h",
                        "inttypes.h",
                        "langinfo.h",
                        "langinfo.h",
                        "memory.h",
                        "stdint.h",
                        "stdlib.h",
                        "strings.h",
                        "string.h",
                        "sys/stat.h",
                        "sys/types.h",
                        "unistd.h" })
        {
            s.checkIncludeExists(f);
        }
    }

    {
        auto &s = c.addSet("libiconv");
        for (auto &f : { "alloca",
            "atoll",
            "canonicalize_file_name",
            "CFLocaleCopyCurrent",
            "CFPreferencesCopyAppValue",
            "dcgettext",
            "getcwd",
            "getc_unlocked",
            "lstat",
            "mbrtowc",
            "mbsinit",
            "memmove",
            "nl_langinfo",
            "readlink",
            "readlinkat",
            "realpath",
            "setenv",
            "setlocale",
            "strerror_r",
            "tsearch",
            "wcrtomb",
            "_NSGetExecutablePath", })
        {
            s.checkFunctionExists(f);
        }
        for (auto &f : { "alloca.h",
            "dlfcn.h",
            "inttypes.h",
            "langinfo.h",
            "mach-o/dyld.h",
            "memory.h",
            "search.h",
            "stdint.h",
            "stdlib.h",
            "strings.h",
            "string.h",
            "sys/bitypes.h",
            "sys/inttypes.h",
            "sys/stat.h",
            "sys/time.h",
            "sys/types.h",
            "unistd.h",
            "wchar.h" })
        {
            s.checkIncludeExists(f);
        }
        for (auto &f : {"wchar_t",
                        "long long int",
                        "sigset_t",
                        "unsigned long long int",
                        "_Bool"})
        {
            s.checkTypeSize(f);
        }
        auto &mb = s.checkSymbolExists("mbstate_t");
        mb.Parameters.Includes.push_back("wchar_t.h");
    }

    {
        auto &s = c.addSet("libxml2");
        for (auto &f : { "class",
            "dlopen",
            "finite",
            "fpclass",
            "fprintf",
            "fp_class",
            "ftime",
            "getaddrinfo",
            "gettimeofday",
            "isascii",
            "isinf",
            "isnan",
            "isnand",
            "localtime",
            "mmap",
            "munmap",
            "printf",
            "putenv",
            "rand",
            "rand_r",
            "signal",
            "stat",
            "strdup",
            "strerror",
            "strftime",
            "strndup",
            "time",
            "va_copy",
            "vfprintf",
            "vsnprintf",
            "vsprintf",
            "_stat",
            "__va_copy", })
        {
            s.checkFunctionExists(f);
        }
        for (auto &f : { "ansidecl.h",
            "arpa/inet.h",
            "arpa/nameser.h",
            "ctype.h",
            "dirent.h",
            "dlfcn.h",
            "dl.h",
            "errno.h",
            "fcntl.h",
            "float.h",
            "fp_class.h",
            "ieeefp.h",
            "inttypes.h",
            "limits.h",
            "malloc.h",
            "math.h",
            "memory.h",
            "nan.h",
            "ndir.h",
            "netdb.h",
            "netinet/in.h",
            "poll.h",
            "pthread.h",
            "resolv.h",
            "signal.h",
            "stdarg.h",
            "stdint.h",
            "stdlib.h",
            "strings.h",
            "string.h",
            "sys/dir.h",
            "sys/mman.h",
            "sys/ndir.h",
            "sys/select.h",
            "sys/socket.h",
            "sys/stat.h",
            "sys/timeb.h",
            "sys/time.h",
            "sys/types.h",
            "time.h",
            "unistd.h",
            "winsock2.h", })
        {
            s.checkIncludeExists(f);
        }
        for (auto &f : { "socklen_t" })
        {
            s.checkTypeSize(f);
        }
        auto &mb = s.checkSymbolExists("snprintf");
        mb.Parameters.Includes.push_back("stdio.h");
    }

    {
        auto &s = c.addSet("lzma");
        s.checkFunctionExists("CC_SHA256_Init");
        s.checkFunctionExists("clock_gettime");
        s.checkFunctionExists("futimens");
        s.checkFunctionExists("futimes");
        s.checkFunctionExists("futimesat");
        s.checkFunctionExists("posix_fadvise");
        s.checkFunctionExists("pthread_condattr_setclock");
        s.checkFunctionExists("SHA256Init");
        s.checkFunctionExists("SHA256_Init");
        s.checkFunctionExists("utime");
        s.checkFunctionExists("utimes");
        s.checkIncludeExists("CommonCrypto/CommonDigest.h");
        s.checkIncludeExists("fcntl.h");
        s.checkIncludeExists("immintrin.h");
        s.checkIncludeExists("inttypes.h");
        s.checkIncludeExists("limits.h");
        s.checkIncludeExists("memory.h");
        s.checkIncludeExists("minix/sha2.h");
        s.checkIncludeExists("sha256.h");
        s.checkIncludeExists("sha2.h");
        s.checkIncludeExists("stdbool.h");
        s.checkIncludeExists("stddef.h");
        s.checkIncludeExists("stdint.h");
        s.checkIncludeExists("stdlib.h");
        s.checkIncludeExists("strings.h");
        s.checkIncludeExists("string.h");
        s.checkIncludeExists("sys/stat.h");
        s.checkIncludeExists("sys/time.h");
        s.checkIncludeExists("sys/types.h");
        s.checkIncludeExists("unistd.h");
        s.checkTypeSize("CC_SHA256_CTX");
        s.checkTypeSize("int32_t");
        s.checkTypeSize("int64_t");
        s.checkTypeSize("SHA256_CTX");
        s.checkTypeSize("SHA2_CTX");
        s.checkTypeSize("size_t");
        s.checkTypeSize("uint16_t");
        s.checkTypeSize("uint32_t");
        s.checkTypeSize("uint64_t");
        s.checkTypeSize("uint8_t");
        s.checkTypeSize("uintptr_t");
        s.checkTypeSize("void *");
        s.checkTypeSize("_Bool");
        s.checkDeclarationExists("CLOCK_MONOTONIC");
        s.checkDeclarationExists("_mm_movemask_epi8");

        s.checkStructMemberExists("struct stat", "st_atimensec").Parameters.Includes.push_back("sys/stat.h");
        s.checkStructMemberExists("struct stat", "st_atimespec.tv_nsec").Parameters.Includes.push_back("sys/stat.h");
        s.checkStructMemberExists("struct stat", "st_atim.st__tim.tv_nsec").Parameters.Includes.push_back("sys/stat.h");
        s.checkStructMemberExists("struct stat", "st_atim.tv_nsec").Parameters.Includes.push_back("sys/stat.h");
        s.checkStructMemberExists("struct stat", "st_uatime").Parameters.Includes.push_back("sys/stat.h");
    }

    {
        auto &s = c.addSet("eccdata");
        s.checkFunctionExists("getline");
        s.checkFunctionExists("secure_getenv");
        s.checkIncludeExists("dlfcn.h");
        s.checkIncludeExists("time.h");
        s.checkTypeSize("long");
        s.checkTypeSize("size_t");
        s.checkTypeSize("uid_t");
        s.checkTypeSize("unsigned long");
        s.checkTypeSize("void *");
    }

    {
        auto &s = c.addSet("nettle");
        s.checkFunctionExists("getline");
        s.checkFunctionExists("secure_getenv");
        s.checkIncludeExists("dlfcn.h");
        s.checkIncludeExists("time.h");
        s.checkTypeSize("long");
        s.checkTypeSize("size_t");
        s.checkTypeSize("uid_t");
        s.checkTypeSize("unsigned long");
        s.checkTypeSize("void *");
        s.checkLibraryFunctionExists("dl", "dlopen");
        s.checkLibraryFunctionExists("gmp", "__gmpz_powm_sec");
        s.checkSourceCompiles("HAVE_TIME_WITH_SYS_TIME", R"xxx(
#include <time.h>
#include <sys/time.h>
int main() {return 0;}
)xxx");
    }

    {
        auto &s = c.addSet("libarchive");
        s.checkFunctionExists("acl_create_entry");
        s.checkFunctionExists("acl_get_fd_np");
        s.checkFunctionExists("acl_get_link");
        s.checkFunctionExists("acl_get_link_np");
        s.checkFunctionExists("acl_get_perm");
        s.checkFunctionExists("acl_get_perm_np");
        s.checkFunctionExists("acl_init");
        s.checkFunctionExists("acl_is_trivial_np");
        s.checkFunctionExists("acl_set_link_np");
        s.checkFunctionExists("acl_set_fd");
        s.checkFunctionExists("acl_set_fd_np");
        s.checkFunctionExists("acl_set_file");
        s.checkFunctionExists("arc4random_buf");
        s.checkFunctionExists("chflags");
        s.checkFunctionExists("chown");
        s.checkFunctionExists("chroot");
        s.checkFunctionExists("ctime_r");
        s.checkFunctionExists("cygwin_conv_path");
        s.checkFunctionExists("dirfd");
        s.checkFunctionExists("extattr_get_file");
        s.checkFunctionExists("extattr_list_file");
        s.checkFunctionExists("extattr_set_fd");
        s.checkFunctionExists("extattr_set_file");
        s.checkFunctionExists("fchdir");
        s.checkFunctionExists("fchflags");
        s.checkFunctionExists("fchmod");
        s.checkFunctionExists("fchown");
        s.checkFunctionExists("fcntl");
        s.checkFunctionExists("fdopendir");
        s.checkFunctionExists("fgetea");
        s.checkFunctionExists("fgetxattr");
        s.checkFunctionExists("flistea");
        s.checkFunctionExists("flistxattr");
        s.checkFunctionExists("fork");
        s.checkFunctionExists("fseeko");
        s.checkFunctionExists("fsetea");
        s.checkFunctionExists("fsetxattr");
        s.checkFunctionExists("fstat");
        s.checkFunctionExists("fstatat");
        s.checkFunctionExists("fstatfs");
        s.checkFunctionExists("fstatvfs");
        s.checkFunctionExists("ftruncate");
        s.checkFunctionExists("futimens");
        s.checkFunctionExists("futimes");
        s.checkFunctionExists("futimesat");
        s.checkFunctionExists("getea");
        s.checkFunctionExists("geteuid");
        s.checkFunctionExists("getgrgid_r");
        s.checkFunctionExists("getgrnam_r");
        s.checkFunctionExists("getpid");
        s.checkFunctionExists("getpwnam_r");
        s.checkFunctionExists("getpwuid_r");
        s.checkFunctionExists("getvfsbyname");
        s.checkFunctionExists("getxattr");
        s.checkFunctionExists("gmtime_r");
        s.checkFunctionExists("lchflags");
        s.checkFunctionExists("lchmod");
        s.checkFunctionExists("lchown");
        s.checkFunctionExists("lgetea");
        s.checkFunctionExists("lgetxattr");
        s.checkFunctionExists("link");
        s.checkFunctionExists("listea");
        s.checkFunctionExists("listxattr");
        s.checkFunctionExists("llistea");
        s.checkFunctionExists("llistxattr");
        s.checkFunctionExists("locale_charset");
        s.checkFunctionExists("localtime_r");
        s.checkFunctionExists("lsetea");
        s.checkFunctionExists("lsetxattr");
        s.checkFunctionExists("lstat");
        s.checkFunctionExists("lutimes");
        s.checkFunctionExists("mbrtowc");
        s.checkFunctionExists("memcmp");
        s.checkFunctionExists("memmove");
        s.checkFunctionExists("memset");
        s.checkFunctionExists("mkdir");
        s.checkFunctionExists("mkfifo");
        s.checkFunctionExists("mknod");
        s.checkFunctionExists("mkstemp");
        s.checkFunctionExists("nl_langinfo");
        s.checkFunctionExists("openat");
        s.checkFunctionExists("pipe");
        s.checkFunctionExists("PKCS5_PBKDF2_HMAC_SHA1");
        s.checkFunctionExists("poll");
        s.checkFunctionExists("posix_spawnp");
        s.checkFunctionExists("readlink");
        s.checkFunctionExists("readlinkat");
        s.checkFunctionExists("readpassphrase");
        s.checkFunctionExists("regcomp");
        s.checkFunctionExists("select");
        s.checkFunctionExists("setenv");
        s.checkFunctionExists("setlocale");
        s.checkFunctionExists("sigaction");
        s.checkFunctionExists("stat");
        s.checkFunctionExists("statfs");
        s.checkFunctionExists("statvfs");
        s.checkFunctionExists("strchr");
        s.checkFunctionExists("strdup");
        s.checkFunctionExists("strerror");
        s.checkFunctionExists("strerror_r");
        s.checkFunctionExists("strftime");
        s.checkFunctionExists("strncpy_s");
        s.checkFunctionExists("strrchr");
        s.checkFunctionExists("symlink");
        s.checkFunctionExists("timegm");
        s.checkFunctionExists("tzset");
        s.checkFunctionExists("unsetenv");
        s.checkFunctionExists("utime");
        s.checkFunctionExists("utimensat");
        s.checkFunctionExists("utimes");
        s.checkFunctionExists("vfork");
        s.checkFunctionExists("vprintf");
        s.checkFunctionExists("wcrtomb");
        s.checkFunctionExists("wcscmp");
        s.checkFunctionExists("wcscpy");
        s.checkFunctionExists("wcslen");
        s.checkFunctionExists("wctomb");
        s.checkFunctionExists("wmemcmp");
        s.checkFunctionExists("wmemcpy");
        s.checkFunctionExists("wmemmove");
        s.checkFunctionExists("_ctime64_s");
        s.checkFunctionExists("_fseeki64");
        s.checkFunctionExists("_get_timezone");
        s.checkFunctionExists("_localtime64_s");
        s.checkFunctionExists("_mkgmtime64");
        s.checkIncludeExists("acl/libacl.h");
        s.checkIncludeExists("attr/xattr.h");
        s.checkIncludeExists("Bcrypt.h");
        s.checkIncludeExists("copyfile.h");
        s.checkIncludeExists("ctype.h");
        s.checkIncludeExists("dirent.h");
        s.checkIncludeExists("errno.h");
        s.checkIncludeExists("expat.h");
        s.checkIncludeExists("ext2fs/ext2_fs.h");
        s.checkIncludeExists("fcntl.h");
        s.checkIncludeExists("grp.h");
        s.checkIncludeExists("inttypes.h");
        s.checkIncludeExists("io.h");
        s.checkIncludeExists("langinfo.h");
        s.checkIncludeExists("limits.h");
        s.checkIncludeExists("linux/fiemap.h");
        s.checkIncludeExists("linux/fs.h");
        s.checkIncludeExists("linux/magic.h");
        s.checkIncludeExists("linux/types.h");
        s.checkIncludeExists("localcharset.h");
        s.checkIncludeExists("locale.h");
        s.checkIncludeExists("md5.h");
        s.checkIncludeExists("memory.h");
        s.checkIncludeExists("paths.h");
        s.checkIncludeExists("pcreposix.h");
        s.checkIncludeExists("poll.h");
        s.checkIncludeExists("pthread.h");
        s.checkIncludeExists("pwd.h");
        s.checkIncludeExists("readpassphrase.h");
        s.checkIncludeExists("regex.h");
        s.checkIncludeExists("ripemd.h");
        s.checkIncludeExists("sha256.h");
        s.checkIncludeExists("sha512.h");
        s.checkIncludeExists("sha.h");
        s.checkIncludeExists("signal.h");
        s.checkIncludeExists("spawn.h");
        s.checkIncludeExists("stdarg.h");
        s.checkIncludeExists("stddef.h");
        s.checkIncludeExists("stdint.h");
        s.checkIncludeExists("stdlib.h");
        s.checkIncludeExists("strings.h");
        s.checkIncludeExists("string.h");
        s.checkIncludeExists("sys/acl.h");
        s.checkIncludeExists("sys/cdefs.h");
        s.checkIncludeExists("sys/ea.h");
        s.checkIncludeExists("sys/extattr.h");
        s.checkIncludeExists("sys/ioctl.h");
        s.checkIncludeExists("sys/mkdev.h");
        s.checkIncludeExists("sys/mount.h");
        s.checkIncludeExists("sys/param.h");
        s.checkIncludeExists("sys/poll.h");
        s.checkIncludeExists("sys/select.h");
        s.checkIncludeExists("sys/statfs.h");
        s.checkIncludeExists("sys/statvfs.h");
        s.checkIncludeExists("sys/stat.h");
        s.checkIncludeExists("sys/time.h");
        s.checkIncludeExists("sys/types.h");
        s.checkIncludeExists("sys/utime.h");
        s.checkIncludeExists("sys/utsname.h");
        s.checkIncludeExists("sys/vfs.h");
        s.checkIncludeExists("sys/xattr.h");
        s.checkIncludeExists("time.h");
        s.checkIncludeExists("unistd.h");
        s.checkIncludeExists("utime.h");
        s.checkIncludeExists("wchar.h");
        s.checkIncludeExists("wctype.h");
        s.checkIncludeExists("wincrypt.h");
        s.checkIncludeExists("windows.h");
        s.checkIncludeExists("winioctl.h");
        s.checkTypeSize("acl_permset_t");
        s.checkTypeSize("dev_t");
        s.checkTypeSize("gid_t");
        s.checkTypeSize("id_t");
        s.checkTypeSize("int16_t");
        s.checkTypeSize("int32_t");
        s.checkTypeSize("int64_t");
        s.checkTypeSize("intmax_t");
        s.checkTypeSize("intptr_t");
        s.checkTypeSize("long");
        s.checkTypeSize("mode_t");
        s.checkTypeSize("off_t");
        s.checkTypeSize("pid_t");
        s.checkTypeSize("size_t");
        s.checkTypeSize("ssize_t");
        s.checkTypeSize("uid_t");
        s.checkTypeSize("uint16_t");
        s.checkTypeSize("uint32_t");
        s.checkTypeSize("uint64_t");
        s.checkTypeSize("uint8_t");
        s.checkTypeSize("uintmax_t");
        s.checkTypeSize("uintptr_t");
        s.checkTypeSize("unsigned");
        s.checkTypeSize("vfsconf");
        s.checkTypeSize("void *");
        s.checkTypeSize("wchar_t");
        s.checkTypeSize("xvfsconf");
        s.checkLibraryFunctionExists("acl", "acl_get_file");
        s.checkLibraryFunctionExists("charset", "locale_charset");
        s.checkLibraryFunctionExists("md", "MD5Init");
        {
            auto &str = s.checkStructMemberExists("struct dirent", "d_namlen");
            str.Parameters.Includes.push_back("dirent.h");
        }
        {
            auto &str = s.checkStructMemberExists("struct statfs", "f_namemax");
        }
        {
            auto &str = s.checkStructMemberExists("struct statvfs", "f_iosize");
        }
        {
            auto &str = s.checkStructMemberExists("struct stat", "st_birthtime");
            str.Parameters.Includes.push_back("sys/stat.h");
        }
        {
            auto &str = s.checkStructMemberExists("struct stat", "st_birthtimespec.tv_nsec");
            str.Parameters.Includes.push_back("sys/stat.h");
        }
        {
            auto &str = s.checkStructMemberExists("struct stat", "st_blksize");
            str.Parameters.Includes.push_back("sys/stat.h");
        }
        {
            auto &str = s.checkStructMemberExists("struct stat", "st_flags");
            str.Parameters.Includes.push_back("sys/stat.h");
        }
        {
            auto &str = s.checkStructMemberExists("struct stat", "st_mtimespec.tv_nsec");
            str.Parameters.Includes.push_back("sys/stat.h");
        }
        {
            auto &str = s.checkStructMemberExists("struct stat", "st_mtime_n");
            str.Parameters.Includes.push_back("sys/stat.h");
        }
        {
            auto &str = s.checkStructMemberExists("struct stat", "st_mtime_usec");
            str.Parameters.Includes.push_back("sys/stat.h");
        }
        {
            auto &str = s.checkStructMemberExists("struct stat", "st_mtim.tv_nsec");
            str.Parameters.Includes.push_back("sys/stat.h");
        }
        {
            auto &str = s.checkStructMemberExists("struct stat", "st_umtime");
            str.Parameters.Includes.push_back("sys/stat.h");
        }
        {
            auto &str = s.checkStructMemberExists("struct tm", "tm_gmtoff");
            str.Parameters.Includes.push_back("time.h");
        }
        {
            auto &str = s.checkStructMemberExists("struct tm", "__tm_gmtoff");
            str.Parameters.Includes.push_back("time.h");
        }
        s.checkSourceCompiles("HAVE_READDIR_R", R"sw_xxx(#include <dirent.h>

 int main() {

DIR *dir; struct dirent e, *r;
            return(readdir_r(dir, &e, &r));

 ; return 0; })sw_xxx");
        s.checkSourceCompiles("HAVE_TIME_WITH_SYS_TIME", R"sw_xxx(
#include <time.h>
#include <sys/time.h>
int main() {return 0;}
)sw_xxx");
        s.checkSourceCompiles("MAJOR_IN_MKDEV", R"sw_xxx(
#include <sys/mkdev.h>
int main() { makedev(0, 0); return 0; }
)sw_xxx");
        s.checkSourceCompiles("MAJOR_IN_SYSMACROS", R"sw_xxx(
#include <sys/sysmacros.h>
int main() { makedev(0, 0); return 0; }
)sw_xxx");
        s.checkSourceCompiles("STDC_HEADERS", R"sw_xxx(
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <float.h>
int main() {return 0;}
)sw_xxx");
        s.checkDeclarationExists("ACL_USER");
        s.checkDeclarationExists("D_MD_ORDER");
        s.checkDeclarationExists("EFTYPE");
        s.checkDeclarationExists("EILSEQ");
        s.checkDeclarationExists("EXTATTR_NAMESPACE_USER");
        s.checkDeclarationExists("INT32_MAX");
        s.checkDeclarationExists("INT32_MIN");
        s.checkDeclarationExists("INT64_MAX");
        s.checkDeclarationExists("INT64_MIN");
        s.checkDeclarationExists("INTMAX_MAX");
        s.checkDeclarationExists("INTMAX_MIN");
        s.checkDeclarationExists("SIZE_MAX");
        s.checkDeclarationExists("SSIZE_MAX");
        s.checkDeclarationExists("UINT32_MAX");
        s.checkDeclarationExists("UINT64_MAX");
        s.checkDeclarationExists("UINTMAX_MAX");
    }

    {
        auto &s = c.addSet("intl");
        s.checkFunctionExists("alloca");
        s.checkFunctionExists("getcwd");
        s.checkFunctionExists("mempcpy");
        s.checkFunctionExists("stpcpy");
        s.checkFunctionExists("tsearch");
        s.checkFunctionExists("wcrtomb");
        s.checkFunctionExists("newlocale");
        s.checkFunctionExists("asprintf");
        s.checkFunctionExists("wprintf");
        s.checkIncludeExists("alloca.h");
        s.checkIncludeExists("argz.h");
        s.checkIncludeExists("features.h");
        s.checkIncludeExists("inttypes.h");
        s.checkIncludeExists("limits.h");
        s.checkIncludeExists("pthread.h");
        s.checkIncludeExists("stdint.h");
        s.checkIncludeExists("sys/param.h");
        s.checkIncludeExists("unistd.h");
        s.checkTypeSize("long long int");
        s.checkTypeSize("size_t");
        s.checkTypeSize("void *");
        s.checkTypeSize("wchar_t");
        {
            auto &c = s.checkSymbolExists("PTHREAD_MUTEX_RECURSIVE");
            c.Parameters.Includes.push_back("pthread.h");
        }
        {
            auto &c = s.checkSymbolExists("snprintf");
            c.Parameters.Includes.push_back("stdio.h");
        }
        {
            auto &c = s.checkSymbolExists("wctype_t");
            c.Parameters.Includes.push_back("wctype.h");
        }
        {
            auto &c = s.checkSymbolExists("wint_t");
            c.Parameters.Includes.push_back("wctype.h");
        }
    }

    {
        auto &s = c.addSet("gss");
        s.checkTypeSize("size_t");
        s.checkTypeSize("void *");
    }

    {
        auto &s = c.addSet("nghttp2");
        s.checkFunctionExists("accept4");
        s.checkFunctionExists("chown");
        s.checkFunctionExists("dup2");
        s.checkFunctionExists("error_at_line");
        s.checkFunctionExists("fork");
        s.checkFunctionExists("getcwd");
        s.checkFunctionExists("getpwnam");
        s.checkFunctionExists("localtime_r");
        s.checkFunctionExists("malloc");
        s.checkFunctionExists("memchr");
        s.checkFunctionExists("memmove");
        s.checkFunctionExists("memset");
        s.checkFunctionExists("realloc");
        s.checkFunctionExists("socket");
        s.checkFunctionExists("sqrt");
        s.checkFunctionExists("strchr");
        s.checkFunctionExists("strdup");
        s.checkFunctionExists("strerror");
        s.checkFunctionExists("strerror_r");
        s.checkFunctionExists("strndup");
        s.checkFunctionExists("strnlen");
        s.checkFunctionExists("strstr");
        s.checkFunctionExists("strtol");
        s.checkFunctionExists("strtoul");
        s.checkFunctionExists("timegm");
        s.checkFunctionExists("timerfd_create");
        s.checkFunctionExists("_Exit");
        s.checkIncludeExists("arpa/inet.h");
        s.checkIncludeExists("assert.h");
        s.checkIncludeExists("fcntl.h");
        s.checkIncludeExists("limits.h");
        s.checkIncludeExists("netdb.h");
        s.checkIncludeExists("netinet/in.h");
        s.checkIncludeExists("pwd.h");
        s.checkIncludeExists("syslog.h");
        s.checkIncludeExists("sys/socket.h");
        s.checkIncludeExists("sys/time.h");
        s.checkIncludeExists("time.h");
        s.checkTypeSize("int");
        s.checkTypeSize("int16_t");
        s.checkTypeSize("int32_t");
        s.checkTypeSize("int64_t");
        s.checkTypeSize("int8_t");
        s.checkTypeSize("off_t");
        s.checkTypeSize("pid_t");
        s.checkTypeSize("ptrdiff_t");
        s.checkTypeSize("size_t");
        s.checkTypeSize("ssize_t");
        s.checkTypeSize("time_t");
        s.checkTypeSize("uid_t");
        s.checkTypeSize("uint16_t");
        s.checkTypeSize("uint32_t");
        s.checkTypeSize("uint64_t");
        s.checkTypeSize("uint8_t");
        s.checkTypeSize("void *");
        s.checkLibraryFunctionExists("cunit", "CU_initialize_registry");
        s.checkLibraryFunctionExists("ev", "ev_time");
        s.checkStructMemberExists("struct tm", "tm_gmtoff");
    }

    {
        auto &s = c.addSet("crypto");
        s.checkTypeSize("size_t");
        s.checkTypeSize("void *");
    }

    {
        auto &s = c.addSet("ssl");
        s.checkTypeSize("size_t");
        s.checkTypeSize("void *");
    }

    {
        auto &s = c.addSet("libssh2");
        s.checkFunctionExists("alloca");
        s.checkFunctionExists("gettimeofday");
        s.checkFunctionExists("poll");
        s.checkFunctionExists("select");
        s.checkFunctionExists("strtoll");
        s.checkIncludeExists("arpa/inet.h");
        s.checkIncludeExists("errno.h");
        s.checkIncludeExists("fcntl.h");
        s.checkIncludeExists("inttypes.h");
        s.checkIncludeExists("netinet/in.h");
        s.checkIncludeExists("stdio.h");
        s.checkIncludeExists("stdlib.h");
        s.checkIncludeExists("stdint.h");
        s.checkIncludeExists("sys/ioctl.h");
        s.checkIncludeExists("sys/select.h");
        s.checkIncludeExists("sys/socket.h");
        s.checkIncludeExists("sys/time.h");
        s.checkIncludeExists("sys/uio.h");
        s.checkIncludeExists("sys/un.h");
        s.checkIncludeExists("unistd.h");
        s.checkIncludeExists("windows.h");
        s.checkIncludeExists("winsock2.h");
        s.checkIncludeExists("ws2tcpip.h");
        s.checkTypeSize("long");
        s.checkTypeSize("size_t");
        s.checkTypeSize("void *");
        {
            auto &mb = s.checkSymbolExists("snprintf");
            mb.Parameters.Includes.push_back("stdio.h");
        }
        s.checkSourceCompiles("STDC_HEADERS", R"sw_xxx(
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <float.h>
int main() {return 0;}
)sw_xxx");
        s.checkSourceCompiles("HAVE_O_NONBLOCK", R"sw_xxx(
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#if defined(sun) || defined(__sun__) || defined(__SUNPRO_C) || defined(__SUNPRO_CC)
# if defined(__SVR4) || defined(__srv4__)
#  define PLATFORM_SOLARIS
# else
#  define PLATFORM_SUNOS4
# endif
#endif
#if (defined(_AIX) || defined(__xlC__)) && !defined(_AIX41)
# define PLATFORM_AIX_V3
#endif
#if defined(PLATFORM_SUNOS4) || defined(PLATFORM_AIX_V3) || defined(__BEOS__)
#error \"O_NONBLOCK does not work on this platform\"
#endif
int main()
{
    int socket;
    int flags = fcntl(socket, F_SETFL, flags | O_NONBLOCK);
}
)sw_xxx");
        s.checkSourceCompiles("HAVE_FIONBIO", R"sw_xxx(
/* FIONBIO test (old-style unix) */
#include <unistd.h>
#include <stropts.h>
int main()
{
    int socket;
    int flags = ioctl(socket, FIONBIO, &flags);
}
)sw_xxx");
        s.checkSourceCompiles("HAVE_IOCTLSOCKET", R"sw_xxx(
/* ioctlsocket test (Windows) */
#undef inline
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
int main()
{
    SOCKET sd;
    unsigned long flags = 0;
    sd = socket(0, 0, 0);
    ioctlsocket(sd, FIONBIO, &flags);
}
)sw_xxx");
        s.checkSourceCompiles("HAVE_IOCTLSOCKET_CASE", R"sw_xxx(
/* IoctlSocket test (Amiga?) */
#include <sys/ioctl.h>
int main()
{
    int socket;
    int flags = IoctlSocket(socket, FIONBIO, (long)1);
}
)sw_xxx");
        s.checkSourceCompiles("HAVE_SO_NONBLOCK", R"sw_xxx(
/* SO_NONBLOCK test (BeOS) */
#include <socket.h>
int main()
{
    long b = 1;
    int socket;
    int flags = setsockopt(socket, SOL_SOCKET, SO_NONBLOCK, &b, sizeof(b));
}
)sw_xxx");
    }

    {
        auto &s = c.addSet("c_ares");
        s.checkFunctionExists("bitncmp");
        s.checkFunctionExists("closesocket");
        s.checkFunctionExists("connect");
        s.checkFunctionExists("fcntl");
        s.checkFunctionExists("fork");
        s.checkFunctionExists("geteuid");
        s.checkFunctionExists("gethostbyname");
        s.checkFunctionExists("getpass_r");
        s.checkFunctionExists("getppid");
        s.checkFunctionExists("getprotobyname");
        s.checkFunctionExists("getpwuid");
        s.checkFunctionExists("getpwuid_r");
        s.checkFunctionExists("getrlimit");
        s.checkFunctionExists("gettimeofday");
        s.checkFunctionExists("if_indextoname");
        s.checkFunctionExists("if_nametoindex");
        s.checkFunctionExists("inet_addr");
        s.checkFunctionExists("fnctl");
        s.checkFunctionExists("ioctl");
        s.checkFunctionExists("ldap_init_fd");
        s.checkFunctionExists("ldap_url_parse");
        s.checkFunctionExists("perror");
        s.checkFunctionExists("pipe");
        s.checkFunctionExists("poll");
        s.checkFunctionExists("select");
        s.checkFunctionExists("pthread_create");
        s.checkFunctionExists("recv");
        s.checkFunctionExists("recvfrom");
        s.checkFunctionExists("send");
        s.checkFunctionExists("sendto");
        s.checkFunctionExists("setlocale");
        s.checkFunctionExists("setmode");
        s.checkFunctionExists("setrlimit");
        s.checkFunctionExists("strcasecmp");
        s.checkFunctionExists("uname");
        s.checkFunctionExists("utime");
        s.checkFunctionExists("writev");
        s.checkIncludeExists("alloca.h");
        s.checkIncludeExists("arpa/inet.h");
        s.checkIncludeExists("arpa/nameser_compat.h");
        s.checkIncludeExists("arpa/nameser.h");
        s.checkIncludeExists("arpa/tftp.h");
        s.checkIncludeExists("assert.h");
        s.checkIncludeExists("crypto.h");
        s.checkIncludeExists("cyassl/error-ssl.h");
        s.checkIncludeExists("cyassl/options.h");
        s.checkIncludeExists("dlfcn.h");
        s.checkIncludeExists("errno.h");
        s.checkIncludeExists("err.h");
        s.checkIncludeExists("fcntl.h");
        s.checkIncludeExists("gssapi/gssapi_generic.h");
        s.checkIncludeExists("gssapi/gssapi.h");
        s.checkIncludeExists("gssapi/gssapi_krb5.h");
        s.checkIncludeExists("idn2.h");
        s.checkIncludeExists("inttypes.h");
        s.checkIncludeExists("io.h");
        s.checkIncludeExists("libgen.h");
        s.checkIncludeExists("librtmp/rtmp.h");
        s.checkIncludeExists("libssh2.h");
        s.checkIncludeExists("limits.h");
        s.checkIncludeExists("locale.h");
        s.checkIncludeExists("netdb.h");
        s.checkIncludeExists("netinet/in.h");
        s.checkIncludeExists("netinet/tcp.h");
        s.checkIncludeExists("net/if.h");
        s.checkIncludeExists("nghttp2/nghttp2.h");
        s.checkIncludeExists("openssl/crypto.h");
        s.checkIncludeExists("openssl/engine.h");
        s.checkIncludeExists("openssl/err.h");
        s.checkIncludeExists("openssl/pem.h");
        s.checkIncludeExists("openssl/pkcs12.h");
        s.checkIncludeExists("openssl/rsa.h");
        s.checkIncludeExists("openssl/ssl.h");
        s.checkIncludeExists("openssl/x509.h");
        s.checkIncludeExists("pem.h");
        s.checkIncludeExists("poll.h");
        s.checkIncludeExists("pwd.h");
        s.checkIncludeExists("rsa.h");
        s.checkIncludeExists("setjmp.h");
        s.checkIncludeExists("sgtty.h");
        s.checkIncludeExists("socket.h");
        s.checkIncludeExists("ssl.h");
        s.checkIncludeExists("stdbool.h");
        s.checkIncludeExists("stdint.h");
        s.checkIncludeExists("stdlib.h");
        s.checkIncludeExists("strings.h");
        s.checkIncludeExists("sys/filio.h");
        s.checkIncludeExists("sys/ioctl.h");
        s.checkIncludeExists("sys/param.h");
        s.checkIncludeExists("sys/poll.h");
        s.checkIncludeExists("sys/resource.h");
        s.checkIncludeExists("sys/select.h");
        s.checkIncludeExists("sys/socket.h");
        s.checkIncludeExists("sys/sockio.h");
        s.checkIncludeExists("sys/stat.h");
        s.checkIncludeExists("sys/time.h");
        s.checkIncludeExists("sys/types.h");
        s.checkIncludeExists("sys/uio.h");
        s.checkIncludeExists("sys/un.h");
        s.checkIncludeExists("sys/utime.h");
        s.checkIncludeExists("sys/wait.h");
        s.checkIncludeExists("termios.h");
        s.checkIncludeExists("termio.h");
        s.checkIncludeExists("time.h");
        s.checkIncludeExists("unistd.h");
        s.checkIncludeExists("utime.h");
        s.checkIncludeExists("windows.h");
        s.checkIncludeExists("winsock2.h");
        s.checkIncludeExists("ws2tcpip.h");
        s.checkIncludeExists("x509.h");
        s.checkTypeSize("int");
        s.checkTypeSize("long");
        s.checkTypeSize("off_t");
        s.checkTypeSize("short");
        s.checkTypeSize("signal");
        s.checkTypeSize("size_t");
        s.checkTypeSize("time_t");
        s.checkTypeSize("void *");
        s.checkLibraryFunctionExists("nsl", "gethostbyname");
        s.checkLibraryFunctionExists("pthread", "pthread_create");
        s.checkLibraryFunctionExists("resolve", "strcasecmp");
        s.checkSourceCompiles("HAVE_TIME_WITH_SYS_TIME", R"sw_xxx(
#include <sys/time.h>
#include <time.h>
int main() {return 0;}
)sw_xxx");
        s.checkSourceCompiles("STDC_HEADERS", R"sw_xxx(
#include <float.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
int main() {return 0;}
)sw_xxx");

        {
            for (auto &h : {"stdbool.h",
                            "sys/types.h",
                            "arpa/inet.h",
                            "arpa/nameser.h",
                            "netdb.h",
                            "net/if.h",
                            "netinet/in.h",
                            "netinet/tcp.h",
                            "signal.h",
                            "stdlib.h",
                            "string.h",
                            "strings.h",
                            "sys/ioctl.h",
                            "sys/select.h",
                            "sys/socket.h",
                            "sys/time.h",
                            "sys/uio.h",
                            "time.h",
                            "fcntl.h",
                            "unistd.h",
                            "winsock2.h",
                            "ws2tcpip.h",
                            "windows.h"})
            {
                auto &c = s.checkTypeSize("SOCKET", "HAVE_TYPE_SOCKET");
                c.Parameters.Includes.push_back(h);

                for (auto &t : {
                         "socklen_t",
                         "ssize_t",
                         "bool",
                         "sig_atomic_t",
                         "long long",
                         "struct addrinfo",
                         "struct in6_addr",
                         "struct sockaddr_in6",
                         "struct sockaddr_storage",
                         "struct timeval"})
                {
                    auto &c = s.checkTypeSize(t);
                    c.Parameters.Includes.push_back(h);
                }

                for (auto &se : {
                         "AF_INET6",
                         "O_NONBLOCK",
                         "FIONBIO",
                         "SIOCGIFADDR",
                         "MSG_NOSIGNAL",
                         "PF_INET6",
                         "SO_NONBLOCK"})
                {
                    auto &c = s.checkSymbolExists(se);
                    c.Parameters.Includes.push_back(h);
                }

                for (auto &f : {
                    "ioctl",
                    //"ioctlsocket",
                    "recv",
                    "recvfrom",
                    "send",
                    "sendto",
                    "socket" })
                {
                    auto &c = s.checkFunctionExists(f);
                    c.Parameters.Includes.push_back(h);
                }

                {
                    auto &c = s.checkFunctionExists("CloseSocket", "HAVE_CLOSESOCKET_CAMEL");
                    c.Parameters.Includes.push_back(h);
                }

                {
                    auto &c = s.checkFunctionExists("IoctlSocket", "HAVE_IOCTLSOCKET_CAMEL");
                    c.Parameters.Includes.push_back(h);
                }
            }
        }
    }

    {
        auto &s = c.addSet("libcurl");
        s.checkFunctionExists("fork");
        s.checkFunctionExists("geteuid");
        s.checkFunctionExists("gethostbyname");
        s.checkFunctionExists("getpass_r");
        s.checkFunctionExists("getppid");
        s.checkFunctionExists("getprotobyname");
        s.checkFunctionExists("getpwuid");
        s.checkFunctionExists("getpwuid_r");
        s.checkFunctionExists("getrlimit");
        s.checkFunctionExists("gettimeofday");
        s.checkFunctionExists("if_nametoindex");
        s.checkFunctionExists("inet_addr");
        s.checkFunctionExists("ldap_init_fd");
        s.checkFunctionExists("ldap_url_parse");
        s.checkFunctionExists("perror");
        s.checkFunctionExists("poll");
        s.checkFunctionExists("fnctl");
        s.checkFunctionExists("ioctl");
        s.checkFunctionExists("select");
        s.checkFunctionExists("pipe");
        s.checkFunctionExists("pthread_create");
        s.checkFunctionExists("setlocale");
        s.checkFunctionExists("setmode");
        s.checkFunctionExists("setrlimit");
        s.checkFunctionExists("uname");
        s.checkFunctionExists("writev");
        s.checkFunctionExists("utime");
        s.checkIncludeExists("alloca.h");
        s.checkIncludeExists("arpa/inet.h");
        s.checkIncludeExists("arpa/tftp.h");
        s.checkIncludeExists("assert.h");
        s.checkIncludeExists("crypto.h");
        s.checkIncludeExists("cyassl/error-ssl.h");
        s.checkIncludeExists("cyassl/options.h");
        s.checkIncludeExists("errno.h");
        s.checkIncludeExists("err.h");
        s.checkIncludeExists("fcntl.h");
        s.checkIncludeExists("gssapi/gssapi_generic.h");
        s.checkIncludeExists("gssapi/gssapi.h");
        s.checkIncludeExists("gssapi/gssapi_krb5.h");
        s.checkIncludeExists("idn2.h");
        s.checkIncludeExists("inttypes.h");
        s.checkIncludeExists("io.h");
        s.checkIncludeExists("libgen.h");
        s.checkIncludeExists("librtmp/rtmp.h");
        s.checkIncludeExists("libssh2.h");
        s.checkIncludeExists("limits.h");
        s.checkIncludeExists("locale.h");
        s.checkIncludeExists("netdb.h");
        s.checkIncludeExists("netinet/in.h");
        s.checkIncludeExists("netinet/tcp.h");
        s.checkIncludeExists("net/if.h");
        s.checkIncludeExists("nghttp2/nghttp2.h");
        s.checkIncludeExists("openssl/crypto.h");
        s.checkIncludeExists("openssl/engine.h");
        s.checkIncludeExists("openssl/err.h");
        s.checkIncludeExists("openssl/pem.h");
        s.checkIncludeExists("openssl/pkcs12.h");
        s.checkIncludeExists("openssl/rsa.h");
        s.checkIncludeExists("openssl/ssl.h");
        s.checkIncludeExists("openssl/x509.h");
        s.checkIncludeExists("pem.h");
        s.checkIncludeExists("poll.h");
        s.checkIncludeExists("pwd.h");
        s.checkIncludeExists("rsa.h");
        s.checkIncludeExists("setjmp.h");
        s.checkIncludeExists("sgtty.h");
        s.checkIncludeExists("socket.h");
        s.checkIncludeExists("ssl.h");
        s.checkIncludeExists("stdbool.h");
        s.checkIncludeExists("stdint.h");
        s.checkIncludeExists("stdlib.h");
        s.checkIncludeExists("sys/filio.h");
        s.checkIncludeExists("sys/ioctl.h");
        s.checkIncludeExists("sys/param.h");
        s.checkIncludeExists("sys/poll.h");
        s.checkIncludeExists("sys/resource.h");
        s.checkIncludeExists("sys/select.h");
        s.checkIncludeExists("sys/socket.h");
        s.checkIncludeExists("sys/sockio.h");
        s.checkIncludeExists("sys/stat.h");
        s.checkIncludeExists("sys/time.h");
        s.checkIncludeExists("sys/types.h");
        s.checkIncludeExists("sys/uio.h");
        s.checkIncludeExists("sys/un.h");
        s.checkIncludeExists("sys/utime.h");
        s.checkIncludeExists("sys/wait.h");
        s.checkIncludeExists("termios.h");
        s.checkIncludeExists("termio.h");
        s.checkIncludeExists("time.h");
        s.checkIncludeExists("unistd.h");
        s.checkIncludeExists("utime.h");
        s.checkIncludeExists("ws2tcpip.h");
        s.checkIncludeExists("x509.h");
        s.checkTypeSize("bool");
        s.checkTypeSize("int");
        s.checkTypeSize("long");
        s.checkTypeSize("long long");
        s.checkTypeSize("off_t");
        s.checkTypeSize("short");
        s.checkTypeSize("signal");
        s.checkTypeSize("size_t");
        s.checkTypeSize("socklen_t");
        s.checkTypeSize("ssize_t");
        s.checkTypeSize("time_t");
        s.checkTypeSize("void *");
        s.checkLibraryFunctionExists("pthread", "pthread_create");
        s.checkSourceCompiles("HAVE_TIME_WITH_SYS_TIME", R"sw_xxx(
#include <sys/time.h>
#include <time.h>
int main() {return 0;}
)sw_xxx");
        s.checkSourceCompiles("STDC_HEADERS", R"sw_xxx(
#include <float.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
int main() {return 0;}
)sw_xxx");
    }

    {
        auto &s = c.addSet("support_lite");
        s.checkFunctionExists("_alloca");
        s.checkFunctionExists("__alloca");
        s.checkFunctionExists("__ashldi3");
        s.checkFunctionExists("__ashrdi3");
        s.checkFunctionExists("__chkstk");
        s.checkFunctionExists("__chkstk_ms");
        s.checkFunctionExists("__cmpdi2");
        s.checkFunctionExists("__divdi3");
        s.checkFunctionExists("__fixdfdi");
        s.checkFunctionExists("__fixsfdi");
        s.checkFunctionExists("__floatdidf");
        s.checkFunctionExists("__lshrdi3");
        s.checkFunctionExists("__main");
        s.checkFunctionExists("__moddi3");
        s.checkFunctionExists("__udivdi3");
        s.checkFunctionExists("__umoddi3");
        s.checkFunctionExists("___chkstk");
        s.checkFunctionExists("___chkstk_ms");
        s.checkIncludeExists("CrashReporterClient.h");
        s.checkIncludeExists("dirent.h");
        s.checkIncludeExists("dlfcn.h");
        s.checkIncludeExists("errno.h");
        s.checkIncludeExists("fcntl.h");
        s.checkIncludeExists("fenv.h");
        s.checkIncludeExists("histedit.h");
        s.checkIncludeExists("inttypes.h");
        s.checkIncludeExists("link.h");
        s.checkIncludeExists("linux/magic.h");
        s.checkIncludeExists("linux/nfs_fs.h");
        s.checkIncludeExists("linux/smb.h");
        s.checkIncludeExists("mach/mach.h");
        s.checkIncludeExists("malloc.h");
        s.checkIncludeExists("malloc/malloc.h");
        s.checkIncludeExists("ndir.h");
        s.checkIncludeExists("pthread.h");
        s.checkIncludeExists("signal.h");
        s.checkIncludeExists("stdint.h");
        s.checkIncludeExists("sys/dir.h");
        s.checkIncludeExists("sys/ioctl.h");
        s.checkIncludeExists("sys/mman.h");
        s.checkIncludeExists("sys/ndir.h");
        s.checkIncludeExists("sys/param.h");
        s.checkIncludeExists("sys/resource.h");
        s.checkIncludeExists("sys/stat.h");
        s.checkIncludeExists("sys/time.h");
        s.checkIncludeExists("sys/types.h");
        s.checkIncludeExists("sys/uio.h");
        s.checkIncludeExists("termios.h");
        s.checkIncludeExists("unistd.h");
        s.checkIncludeExists("unwind.h");
        s.checkIncludeExists("valgrind/valgrind.h");
        s.checkTypeSize("int64_t");
        s.checkTypeSize("size_t");
        s.checkTypeSize("uint64_t");
        s.checkTypeSize("u_int64_t");
        s.checkTypeSize("void *");
        {
            auto &c = s.checkSymbolExists("dladdr");
            c.Parameters.Includes.push_back("dlfcn.h");
        }
        {
            auto &c = s.checkSymbolExists("dlopen");
            c.Parameters.Includes.push_back("dlfcn.h");
        }
        {
            auto &c = s.checkSymbolExists("futimens");
            c.Parameters.Includes.push_back("sys/stat.h");
        }
        {
            auto &c = s.checkSymbolExists("futimes");
            c.Parameters.Includes.push_back("sys/time.h");
        }
        {
            auto &c = s.checkSymbolExists("getcwd");
            c.Parameters.Includes.push_back("unistd.h");
        }
        {
            auto &c = s.checkSymbolExists("getpagesize");
            c.Parameters.Includes.push_back("unistd.h");
        }
        {
            auto &c = s.checkSymbolExists("getrlimit");
            c.Parameters.Includes.push_back("sys/types.h");
            c.Parameters.Includes.push_back("sys/time.h");
            c.Parameters.Includes.push_back("sys/resource.h");
        }
        {
            auto &c = s.checkSymbolExists("getrusage");
            c.Parameters.Includes.push_back("sys/resource.h");
        }
        {
            auto &c = s.checkSymbolExists("gettimeofday");
            c.Parameters.Includes.push_back("sys/time.h");
        }
        {
            auto &c = s.checkSymbolExists("isatty");
            c.Parameters.Includes.push_back("unistd.h");
        }
        {
            auto &c = s.checkSymbolExists("lseek64");
            c.Parameters.Includes.push_back("sys/types.h");
            c.Parameters.Includes.push_back("unistd.h");
        }
        {
            auto &c = s.checkSymbolExists("mallctl");
            c.Parameters.Includes.push_back("malloc_np.h");
        }
        {
            auto &c = s.checkSymbolExists("mallinfo");
            c.Parameters.Includes.push_back("malloc.h");
        }
        {
            auto &c = s.checkSymbolExists("malloc_zone_statistics");
            c.Parameters.Includes.push_back("malloc/malloc.h");
        }
        {
            auto &c = s.checkSymbolExists("mkdtemp");
            c.Parameters.Includes.push_back("stdlib.h");
            c.Parameters.Includes.push_back("unistd.h");
        }
        {
            auto &c = s.checkSymbolExists("mkstemp");
            c.Parameters.Includes.push_back("stdlib.h");
            c.Parameters.Includes.push_back("unistd.h");
        }
        {
            auto &c = s.checkSymbolExists("mktemp");
            c.Parameters.Includes.push_back("stdlib.h");
            c.Parameters.Includes.push_back("unistd.h");
        }
        {
            auto &c = s.checkSymbolExists("posix_fallocate");
            c.Parameters.Includes.push_back("fcntl.h");
        }
        {
            auto &c = s.checkSymbolExists("posix_spawn");
            c.Parameters.Includes.push_back("spawn.h");
        }
        {
            auto &c = s.checkSymbolExists("pread");
            c.Parameters.Includes.push_back("unistd.h");
        }
        {
            auto &c = s.checkSymbolExists("realpath");
            c.Parameters.Includes.push_back("stdlib.h");
        }
        {
            auto &c = s.checkSymbolExists("sbrk");
            c.Parameters.Includes.push_back("unistd.h");
        }
        {
            auto &c = s.checkSymbolExists("setenv");
            c.Parameters.Includes.push_back("stdlib.h");
        }
        {
            auto &c = s.checkSymbolExists("setrlimit");
            c.Parameters.Includes.push_back("sys/resource.h");
        }
        {
            auto &c = s.checkSymbolExists("sigaltstack");
            c.Parameters.Includes.push_back("signal.h");
        }
        {
            auto &c = s.checkSymbolExists("strerror");
            c.Parameters.Includes.push_back("string.h");
        }
        {
            auto &c = s.checkSymbolExists("strerror_r");
            c.Parameters.Includes.push_back("string.h");
        }
        {
            auto &c = s.checkSymbolExists("strtoll");
            c.Parameters.Includes.push_back("stdlib.h");
        }
        {
            auto &c = s.checkSymbolExists("sysconf");
            c.Parameters.Includes.push_back("unistd.h");
        }
        {
            auto &c = s.checkSymbolExists("writev");
            c.Parameters.Includes.push_back("sys/uio.h");
        }
        {
            auto &c = s.checkSymbolExists("_chsize_s");
            c.Parameters.Includes.push_back("io.h");
        }
        {
            auto &c = s.checkSymbolExists("_Unwind_Backtrace");
            c.Parameters.Includes.push_back("unwind.h");
        }
        {
            auto &c = s.checkSymbolExists("__GLIBC__");
            c.Parameters.Includes.push_back("stdio.h");
        }
    }
}

void build_self(Solution &s)
{
    s.Settings.Native.LibrariesType = LibraryType::Static;

    /*Packages pkgs;
    auto add_to_resolve = [&pkgs](const auto &p)
    {
        auto pkg = extractFromString(p).toPackage();
        auto d = pkg.getDirSrc();
        if (!fs::exists(d))
            pkgs[pkg.ppath.toString()] = pkg;
    };
    add_to_resolve("pvt.cppan.demo.taywee.args-6.1.0");
    resolveAllDependencies(pkgs);*/

    auto o = s.Local;
    s.Local = false;

    build_boost(s);
    build_other(s);

    s.Local = o;

    //resolve();

    //s.TargetsToBuild.add(*boost_targets["chrono"]);
    //s.TargetsToBuild.add(*boost_targets["filesystem"]);
    //s.TargetsToBuild.add(*boost_targets["iostreams"]);
}
