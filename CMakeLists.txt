#
# cppan
#

################################################################################
#
# cmake settings
#
################################################################################

cmake_minimum_required(VERSION 2.8.12)

# In-source builds are not possible and so disabled.
if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
    message(FATAL_ERROR
        "CMake generation for Firebird is not possible within the source directory!"
        "\n Remove the CMakeCache.txt file and try again from another folder, e.g.:"
        "\n "
        "\n rm CMakeCache.txt"
        "\n mkdir build"
        "\n cd build"
        "\n cmake .."
    )
endif()

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# Use solution folders.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake Targets")

################################################################################
#
# project settings
#
################################################################################

project(cppan_client C CXX)

if (NOT DEFINED VERSION_MAJOR)
    set(VERSION_MAJOR 0)
    set(VERSION_MINOR 2)
    set(VERSION_PATCH 5)
    set(BUILD_NUMBER 0)
    set(CPPAN_VERSION_STRING ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})

    find_program(git git)
    if (NOT "${git}" STREQUAL "git-NOTFOUND")
        execute_process(
            COMMAND git rev-list --count HEAD
            OUTPUT_VARIABLE BUILD_NUMBER2
            OUTPUT_STRIP_TRAILING_WHITESPACE
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        )
        if (BUILD_NUMBER2)
            set(BUILD_NUMBER ${BUILD_NUMBER2})
        endif()
    endif()
endif()

# Output directory settings
set(output_dir ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${output_dir})

find_package(BISON 3.0)
find_package(FLEX)

################################################################################
#
# compiler & linker settings
#
################################################################################

if (CPPAN_TEST)
add_definitions(/DCPPAN_TEST)
endif()

if (WIN32)
    add_definitions(/DUNICODE)
    if (MSVC)
        set(disable_msvc_warnings "/W3 /wd4458 /wd4714 /wd4127 /wd4275 /D_SCL_SECURE_NO_WARNINGS /D_CRT_SECURE_NO_WARNINGS /D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${disable_msvc_warnings}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${disable_msvc_warnings}")

        if (NOT CLANG)
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
        endif()

        if (MSVC_STATIC_RUNTIME)
            set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
            set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
        endif()
    endif(MSVC)
endif(WIN32)

if(UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1z")
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1z -stdlib=libc++")
    #set(CMAKE_EXE_LINKER_FLAGS "-lc++abi")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
endif()
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std:c++17")
endif()

if (CLANG)
    set(flags "-Wno-assume -Wno-potentially-evaluated-expression -Wno-delete-non-virtual-dtor")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flags}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flags}")
endif()

################################################################################
#
# build
#
################################################################################

if (NOT DEFINED CPPAN_BUILD_SHARED_LIBS)
    set(CPPAN_BUILD_SHARED_LIBS 0)
endif()
if (NOT WIN32)
    set(CPPAN_USE_CACHE 1)
endif()
add_subdirectory(.cppan)

add_subdirectory(src)

enable_testing()
add_subdirectory(test)

###############################################################################
#
# install & uninstall
#
###############################################################################

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)

add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

###############################################################################
#
# pack & distcheck
#
###############################################################################

if (WIN32)
    #set(CPACK_GENERATOR "ZIP")
    #set(CPACK_COMPONENTS_ALL client)
else()
    #set(CPACK_GENERATOR "ZIP;TGZ;DEB;RPM")
    set(CPACK_SOURCE_GENERATOR "TGZ")
    #set(CPACK_COMPONENTS_ALL client)
endif()

set(CPACK_ARCHIVE_COMPONENT_INSTALL 1)

set(CPACK_DEB_COMPONENT_INSTALL 1)
set(CPACK_DEBIAN_PACKAGE_NAME cppan)
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE amd64)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Egor Pugin")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "C++ Archive Network (CPPAN) components")
#set(CPACK_DEBIAN_PACKAGE_DEPENDS "gcc-5") # , cmake (>= 3.2)")

set(CPACK_RPM_COMPONENT_INSTALL 1)
set(CPACK_RPM_PACKAGE_SUMMARY CPPAN)
set(CPACK_RPM_PACKAGE_NAME cppan)
set(CPACK_RPM_PACKAGE_ARCHITECTURE amd64)

set(CPACK_PACKAGE_VERSION_MAJOR ${VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${VERSION_PATCH})
set(CPACK_PACKAGE_VERSION ${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH})

set(CPACK_PACKAGE_FILE_NAME "cppan-master-${CMAKE_SYSTEM_NAME}")

if (WIN32)
add_custom_target(package_client
    COMMAND ${CMAKE_CPACK_COMMAND}
        -G ZIP
        -D CPACK_COMPONENTS_ALL=client
)
elseif (APPLE)
add_custom_target(package_client
    COMMAND zip -FS -j -9 cppan-master-macOS-client.zip $<TARGET_FILE:client>
)
elseif (EXISTS /etc/centos-release)
add_custom_target(package_client
    COMMAND ${CMAKE_CPACK_COMMAND}
        -G RPM
        -D CPACK_COMPONENTS_ALL=client
)
else()
add_custom_target(package_client
    COMMAND ${CMAKE_CPACK_COMMAND}
        -G DEB
        -D CPACK_COMPONENTS_ALL=client
    COMMAND zip -FS -j -9 cppan-master-Linux-client.zip $<TARGET_FILE:client>
)
endif()

set_property(TARGET
    package_client
    uninstall
PROPERTY FOLDER misc)

include(CPack)

################################################################################
