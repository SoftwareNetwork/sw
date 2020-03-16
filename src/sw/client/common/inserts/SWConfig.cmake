# Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

################################################################################
#
# SWConfig.cmake
#
################################################################################

# increase this variable when file is changed
# and you need user to call 'sw setup' again to update this file
set(SW_CMAKE_VERSION 1)

########################################
# general settings
########################################

find_program(SW_EXECUTABLE sw)

# some standard cmake handling of vars
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SW DEFAULT_MSG SW_EXECUTABLE)
mark_as_advanced(SW_EXECUTABLE)

set(SW_DEPS_DIR "${CMAKE_BINARY_DIR}/.sw/cmake" CACHE STRING "SW local deps dir.")
set(SW_DEPS_FILE "${SW_DEPS_DIR}/sw.txt" CACHE STRING "SW local deps file.")

# clear deps before each run
file(WRITE ${SW_DEPS_FILE} "")

########################################
# MACRO sw_internal_fix_path
########################################

# currently convert cygwin paths into normal windows paths
macro(sw_internal_fix_path p)
    # fix cygwin paths
    if (CYGWIN AND "${${p}}" MATCHES "^/cygdrive/.*")
        string(REGEX REPLACE "^/cygdrive/(.)(.*)" "\\1:\\2" ${p} "${${p}}")
    endif()
endmacro()

########################################
# FUNCTION sw_internal_find_flag
########################################

function(sw_internal_find_flag in_flags f out)
    if (NOT "${${out}}" STREQUAL "")
        return()
    endif()
    if ("${in_flags}" STREQUAL "")
        set(${out} 0 PARENT_SCOPE)
        return()
    endif()
    set(flags ${in_flags})
    string(TOLOWER ${f} f)
    string(TOLOWER ${flags} flags)
    string(FIND "${flags}" "${f}" flags)
    if (NOT ${flags} EQUAL -1)
        set(${out} 1 PARENT_SCOPE)
    else()
        set(${out} 0 PARENT_SCOPE)
    endif()
endfunction()

########################################
# FUNCTION sw_add_package
########################################

# appends package(s) to deps file
function(sw_add_package)
    foreach(a ${ARGN})
        file(APPEND ${SW_DEPS_FILE} "${a}\n")
    endforeach()
endfunction()

########################################
# FUNCTION sw_execute
########################################

function(sw_execute)
    message(STATUS "sw: processing dependencies")

    #if (SW_FORCE)
        #set(SW_FORCE -s)
    #endif()

    if (NOT DEFINED SW_BUILD_SHARED_LIBS)
        set(SW_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})
    endif()

    set(stsh -static)
    if (SW_BUILD_SHARED_LIBS)
        set(stsh -shared)
    endif()

    set(platform)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(platform x64)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(platform x86)
    endif()

    set(mt_flag)
    if (MSVC)
        sw_internal_find_flag("${CMAKE_C_FLAGS_RELEASE}"              /MT       C_MTR        )
        sw_internal_find_flag("${CMAKE_C_FLAGS_RELWITHDEBINFO}"       /MT       C_MTRWDI     )
        sw_internal_find_flag("${CMAKE_C_FLAGS_MINSIZEREL}"           /MT       C_MTMSR      )
        sw_internal_find_flag("${CMAKE_C_FLAGS_DEBUG}"                /MTd      C_MTD        )
        sw_internal_find_flag("${CMAKE_CXX_FLAGS_RELEASE}"            /MT     CXX_MTR        )
        sw_internal_find_flag("${CMAKE_CXX_FLAGS_RELWITHDEBINFO}"     /MT     CXX_MTRWDI     )
        sw_internal_find_flag("${CMAKE_CXX_FLAGS_MINSIZEREL}"         /MT     CXX_MTMSR      )
        sw_internal_find_flag("${CMAKE_CXX_FLAGS_DEBUG}"              /MTd    CXX_MTD        )

        if (  C_MTR OR   C_MTRWDI OR   C_MTMSR OR   C_MTD OR
            CXX_MTR OR CXX_MTRWDI OR CXX_MTMSR OR CXX_MTD)
            set(mt_flag -mt)
        endif()
    endif()

    set(os)
    if (CYGWIN)
        set(os -os cygwin)
    endif()
    if (MINGW OR MSYS)
        set(os -os mingw)
    endif()

    set(compiler)
    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        set(compiler -compiler msvc)
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
        set(compiler -compiler gcc)
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "CLANG")
        set(compiler -compiler clang)
    else()
        # https://cmake.org/cmake/help/latest/variable/CMAKE_LANG_COMPILER_ID.html
        message(FATAL_ERROR "Compiler is not implemented: '${CMAKE_C_COMPILER_ID}' or '${CMAKE_CXX_COMPILER_ID}'")
    endif()

    set(sw_platform_args
        ${stsh}
        -platform ${platform}
        ${mt_flag}
        ${compiler}
        ${os}
    )

    set(wdir "${SW_DEPS_DIR}")
    set(depsfile "${SW_DEPS_FILE}")
    sw_internal_fix_path(wdir)
    sw_internal_fix_path(depsfile)

    set(swcmd
        ${SW_EXECUTABLE}
            ${sw_platform_args}
            -d "${wdir}"
            ${SW_FORCE}
            integrate
            -cmake-deps "${depsfile}"
            -cmake-file-version ${SW_CMAKE_VERSION}
    )

    if (SW_DEBUG)
        string(REPLACE ";" " " swcmd1 "${swcmd}")
        message("${swcmd1}")
    endif()

    execute_process(
        COMMAND ${swcmd}
        RESULT_VARIABLE ret
    )
    if (NOT ${ret} EQUAL 0)
        string(REPLACE ";" " " swcmd "${swcmd}")
        message("sw command: ${swcmd}")
        message(FATAL_ERROR "sw: non-zero exit code: ${ret}")
    endif()

    set(outdir)
    if (CMAKE_RUNTIME_OUTPUT_DIRECTORY)
        set(outdir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
    else()
        set(outdir "${CMAKE_BINARY_DIR}")
    endif()

    set(append_config 0)
    string(FIND "${CMAKE_GENERATOR}" "Visual Studio" found)
    if (NOT ${found} EQUAL -1)
        set(append_config 1)
    endif()
    if (XCODE)
        set(append_config 1)
    endif()
    if (append_config)
        set(cfg "$<CONFIG>")
        set(extendedcfg
                -config
                    $<$<CONFIG:Debug>:d>
                    $<$<CONFIG:MinSizeRel>:msr>
                    $<$<CONFIG:RelWithDebInfo>:rwdi>
                    $<$<CONFIG:Release>:r>
                    # in cmake default config is debug (non-optimized)
                    $<$<CONFIG:>:d>
            )
    elseif (CMAKE_BUILD_TYPE)
        set(cfg "${CMAKE_BUILD_TYPE}")
        if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
            set(extendedcfg -config d)
        elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
            set(extendedcfg -config r)
        elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
            set(extendedcfg -config rwdi)
        elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "MinSizeRel")
            set(extendedcfg -config msr)
        else()
            message(FATAL_ERROR "CMAKE_BUILD_TYPE is not implemented: '${CMAKE_BUILD_TYPE}'")
        endif()
    else()
        set(cfg)
        set(extendedcfg -config d)
    endif()
    set(outdir "${outdir}/${cfg}")

    string(SHA1 depshash "${sw_platform_args}")
    string(SUBSTRING "${depshash}" 0 8 depshash)

    set(SW_DEPS_DIR_CFG_STORAGE "${SW_DEPS_DIR}/deps")
    execute_process(COMMAND ${CMAKE_COMMAND} -E remove_directory "${SW_DEPS_DIR_CFG_STORAGE}")

    # fix dirs
    sw_internal_fix_path(SW_DEPS_DIR_CFG_STORAGE) # after execute_process
    sw_internal_fix_path(outdir)

    # create cmd
    set(swcmd
        ${SW_EXECUTABLE}
            ${sw_platform_args}
            -d "${wdir}"
            ${extendedcfg}
            build "@${depsfile}"
            -ide-copy-to-dir
                "${outdir}"
            -ide-fast-path
                "${SW_DEPS_DIR_CFG_STORAGE}/${cfg}-${depshash}.deps"
    )

    # add deps targets
    add_custom_target(sw_build_dependencies ALL
        COMMAND ${swcmd}
    )
    set_target_properties(sw_build_dependencies
        PROPERTIES
            FOLDER ". SW Predefined Targets"
            PROJECT_LABEL "BUILD_DEPENDENCIES"
    )

    # load our deps
    add_subdirectory(${SW_DEPS_DIR} ${SW_DEPS_DIR}/cmake_bdir)
endfunction()

################################################################################
