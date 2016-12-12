#
# cppan
#

################################################################################

if (WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)
endif()

add_definitions(-DUSE_LOGGER=1)

if (NOT CPPAN_BUILD)
    if (WIN32)
        add_definitions(-DLIBARCHIVE_STATIC)
        include_directories(${PROJECT_SOURCE_DIR}/dep/libarchive)
        include_directories(${PROJECT_SOURCE_DIR}/dep/libressl/include)

        add_definitions(-DCURL_STATICLIB=1)
        include_directories(${PROJECT_SOURCE_DIR}/dep/curl/include)
        include_directories(${PROJECT_BINARY_DIR}/dep/curl/include)
        include_directories(${PROJECT_BINARY_DIR}/dep/curl/include/curl)
    else()
        include_directories(${LibArchive_INCLUDE_DIRS})
        include_directories(${CURL_INCLUDE_DIRS})
        include_directories(${OPENSSL_INCLUDE_DIR})
    endif()

    if (APPLE)
        add_definitions(-DLIBARCHIVE_STATIC)
        include_directories(${PROJECT_SOURCE_DIR}/dep/libarchive/libarchive)
    endif()

    include_directories(${PROJECT_SOURCE_DIR}/dep/yaml-cpp/include)
    include_directories(${PROJECT_SOURCE_DIR}/dep/sqlite3)
endif()

# process is still in deps
include_directories(${PROJECT_SOURCE_DIR}/dep/process)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

########################################
# context
########################################

set(context_src
    context/context.cpp
    context/context.h
)
add_library(context ${context_src})
target_include_directories(context INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/context)

########################################
# inserter
########################################

set(inserter_src
    inserter.cpp
)
add_executable(inserter ${inserter_src})
set_property(TARGET inserter PROPERTY FOLDER gen)
if (NOT CPPAN_BUILD)
    target_link_libraries(inserter ${Boost_LIBRARIES})
else()
    target_link_libraries(inserter pvt.cppan.demo.boost.filesystem)
endif()

########################################
# common
########################################

add_custom_target(inserts_gen
    COMMAND inserter ${CMAKE_CURRENT_SOURCE_DIR}/inserts.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/inserts.cpp
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/inserts/functions.cmake
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/inserts/cppan.h
        ${CMAKE_CURRENT_SOURCE_DIR}/inserts/build.cmake
        ${CMAKE_CURRENT_SOURCE_DIR}/inserts/functions.cmake
        ${CMAKE_CURRENT_SOURCE_DIR}/inserts/generate.cmake
        ${CMAKE_CURRENT_SOURCE_DIR}/inserts/exports.cmake
        ${CMAKE_CURRENT_SOURCE_DIR}/inserts/header.cmake
        ${CMAKE_CURRENT_SOURCE_DIR}/inserts/version.rc.in
        ${CMAKE_CURRENT_SOURCE_DIR}/inserts/branch.rc.in
)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/inserts.cpp PROPERTIES GENERATED TRUE)

add_executable(stamp_genenerator stamp_gen.cpp)
set_property(TARGET stamp_genenerator PROPERTY FOLDER gen)
set(stamp ${CMAKE_CURRENT_BINARY_DIR}/stamp.h.in)
set_source_files_properties(${stamp} PROPERTIES GENERATED True)

add_custom_target(gen_stamp
    COMMAND stamp_genenerator > ${stamp}
)
set_property(TARGET gen_stamp PROPERTY FOLDER gen)

file(GLOB common_src "common/*" "printers/*")
set(common_src ${common_src}
    ${CMAKE_CURRENT_BINARY_DIR}/inserts.cpp
    inserts.cpp.in

    ${stamp}
)
add_library(common ${common_src})
add_dependencies(common inserts_gen gen_stamp)

target_compile_definitions(common
    PRIVATE -DVERSION_MAJOR=${VERSION_MAJOR}
    PRIVATE -DVERSION_MINOR=${VERSION_MINOR}
    PRIVATE -DVERSION_PATCH=${VERSION_PATCH}
)
target_include_directories(common
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/common
)
target_link_libraries(common
    context
    bazel_parser
    extract_comments
)

if (NOT CPPAN_BUILD)
    if (WIN32)
        target_link_libraries(common
            archive_static
            libcurl
            crypto
            sqlite3
            yaml-cpp
            ${Boost_LIBRARIES}
        )
    elseif (APPLE)
        target_link_libraries(common
            yaml-cpp
            archive_static
            sqlite3
            ${CURL_LIBRARIES}
            ${OPENSSL_LIBRARIES}
            ${Boost_LIBRARIES}
        )
    else()
        target_link_libraries(common
            yaml-cpp
            sqlite3
            ${LibArchive_LIBRARIES}
            ${CURL_LIBRARIES}
            ${OPENSSL_LIBRARIES}
            ${Boost_LIBRARIES}
        )
    endif()
else()
    target_link_libraries(common cppan)
endif()

################################################################################

add_subdirectory(autotools)
add_subdirectory(bazel)
add_subdirectory(client)
add_subdirectory(comments)

################################################################################
