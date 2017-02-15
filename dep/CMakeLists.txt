#
# cppan
#

if (WIN32)
set(ENABLE_TEST OFF)
set(ENABLE_CPIO OFF)
set(ENABLE_TAR OFF)
set(ENABLE_CAT OFF)
set(ENABLE_BZip2 OFF)

set(HAVE_WCSCPY 1)
set(HAVE_WCSLEN 1)

set(ZLIB_LIBRARY zlib)
set(ZLIB_INCLUDE_DIR zlib)

set(BUILD_CURL_EXE OFF)
set(BUILD_CURL_TESTS OFF)
set(BUILD_TESTING OFF)
set(CURL_STATICLIB ON)
set(ENABLE_MANUAL OFF)

set(OPENSSL_ROOT_DIR libressl)

include_directories(zlib)

add_subdirectory(curl)
add_subdirectory(libarchive)
add_subdirectory(zlib)
add_subdirectory(libressl)
endif(WIN32)

if (APPLE)
set(ENABLE_LZMA OFF)
set(ENABLE_TEST OFF)
set(ENABLE_CPIO OFF)
set(ENABLE_TAR OFF)
set(ENABLE_CAT OFF)
add_subdirectory(libarchive)
endif()

set(YAML_CPP_BUILD_TOOLS OFF)
set(YAML_CPP_BUILD_CONTRIB OFF)

add_subdirectory(yaml-cpp)

set_property(TARGET
    format
    yaml-cpp
PROPERTY FOLDER dep/yaml)

if (WIN32)
    set_property(TARGET
        zlib
    PROPERTY FOLDER dep)

    set_property(TARGET
        libcurl
    PROPERTY FOLDER dep)

    set_property(TARGET
        archive
        archive_static
    PROPERTY FOLDER dep/archive)

    set_property(TARGET
        ssl
        tls
        crypto
        openssl
    PROPERTY FOLDER dep/libressl)
endif()

add_subdirectory(sqlite3)

file(GLOB src "sha3/*")
add_library(sha3 ${src})
set_property(TARGET sha3 PROPERTY FOLDER dep)

