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

# Use solution folders.
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake Targets")

################################################################################
#
# project settings
#
################################################################################

project(cppan C CXX)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 1)
set(VERSION_PATCH 6)

# Output directory settings
set(output_dir ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${output_dir})

#add_subdirectory(cppan)

set(Boost_USE_STATIC_LIBS ON)

find_package(Boost 1.60 REQUIRED filesystem program_options log system thread date_time)
find_package(BISON 3.0 REQUIRED)
find_package(FLEX REQUIRED)

if (NOT WIN32)
    find_package(LibArchive REQUIRED)
    find_package(CURL REQUIRED)
    find_package(ZLIB REQUIRED)
    find_package(OpenSSL REQUIRED)
endif()

################################################################################
#
# compiler & linker settings
#
################################################################################

if (CPPAN_TEST)
add_definitions(-DCPPAN_TEST)
endif()

if (WIN32)
    if (MSVC)
        set(disable_msvc_warnings "/W3 /wd4458 /wd4714 -D_SCL_SECURE_NO_WARNINGS -D_CRT_SECURE_NO_WARNINGS")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MP ${disable_msvc_warnings}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP ${disable_msvc_warnings}")

        if (MSVC_STATIC_RUNTIME)
            set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
            set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
        endif()
    endif(MSVC)
endif(WIN32)


if(UNIX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
endif()

################################################################################
#
# build
#
################################################################################

include_directories(${Boost_INCLUDE_DIRS})

link_directories(${Boost_LIBRARY_DIRS})

########################################
# subdirectories
########################################

add_subdirectory(dep)
add_subdirectory(src)

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
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE amd64)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Egor Pugin")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "C++ Archive Network (CPPAN) components")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libpqxx-4.0, gcc-5")

set(CPACK_RPM_COMPONENT_INSTALL 1)
set(CPACK_RPM_PACKAGE_SUMMARY CPPAN)
#set(CPACK_RPM_PACKAGE_NAME cppan)
set(CPACK_RPM_PACKAGE_ARCHITECTURE amd64)

set(CPACK_PACKAGE_VERSION_MAJOR ${VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${VERSION_PATCH})
set(CPACK_PACKAGE_VERSION ${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH})

set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")

if (WIN32)
add_custom_target(package_client
    COMMAND ${CMAKE_CPACK_COMMAND}
        -G ZIP
        -D CPACK_COMPONENTS_ALL=client
)
else()
add_custom_target(package_client
    COMMAND ${CMAKE_CPACK_COMMAND}
        -G DEB
        -D CPACK_COMPONENTS_ALL=client
    COMMAND ${CMAKE_CPACK_COMMAND}
        -G RPM
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
