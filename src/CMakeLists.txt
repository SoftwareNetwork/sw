#
# cppan
#

################################################################################

if (WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)
endif()

add_definitions(-DUSE_LOGGER=1)

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

include_directories(${PROJECT_SOURCE_DIR}/dep/yaml-cpp/include)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

########################################
# context
########################################

set(context_src
    context.cpp
    context.h
)
add_library(context ${context_src})

########################################
# inserter
########################################

set(inserter_src
    inserter.cpp
)
add_executable(inserter ${inserter_src})
target_link_libraries(inserter ${Boost_LIBRARIES})

########################################
# common
########################################

add_custom_target(inserts_gen
    COMMAND inserter ${CMAKE_CURRENT_SOURCE_DIR}/inserts.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/inserts.cpp
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/inserts/functions.cmake
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/inserts/functions.cmake
        ${CMAKE_CURRENT_SOURCE_DIR}/inserts/cppan.h
)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/inserts.cpp PROPERTIES GENERATED TRUE)

add_executable(stamp_gen stamp_gen.cpp)
set(stamp ${CMAKE_CURRENT_BINARY_DIR}/stamp.h)
set_source_files_properties(${stamp} PROPERTIES GENERATED True)

set(common_src
    access_table.cpp
    access_table.h
    common.cpp
    common.h
    config.cpp
    config.h
    dependency.cpp
    dependency.h
    filesystem.cpp
    filesystem.h
    enums.cpp
    enums.h
    log.cpp
    log.h
    package.cpp
    package.h
    property_tree.h
    project.cpp
    project.h
    project_path.cpp
    project_path.h
    response.cpp
    response.h
    source.cpp
    source.h
    yaml.h
    
    printers/printer.cpp
    printers/printer.h
    printers/cmake.cpp
    printers/cmake.h

    ${CMAKE_CURRENT_BINARY_DIR}/inserts.cpp
    inserts.cpp.in
    inserts.h
    
    ${stamp}
    
    cppan.natvis
)
add_library(common ${common_src})
add_dependencies(common inserts_gen)
add_custom_command(TARGET common PRE_BUILD
    COMMAND stamp_gen > ${stamp}
)
target_compile_definitions(common
    PRIVATE -DVERSION_MAJOR=${VERSION_MAJOR}
    PRIVATE -DVERSION_MINOR=${VERSION_MINOR}
    PRIVATE -DVERSION_PATCH=${VERSION_PATCH}
)
target_link_libraries(common context bazel_parser)
if (WIN32)
    target_link_libraries(common
        archive_static
        libcurl
        crypto
        yaml-cpp
        ${Boost_LIBRARIES}
    )
else()
    target_link_libraries(common
        yaml-cpp
        ${LibArchive_LIBRARIES}
        ${CURL_LIBRARIES}
        ${OPENSSL_LIBRARIES}
        ${Boost_LIBRARIES}
    )
endif()

################################################################################

add_subdirectory(bazel)
add_subdirectory(client)

################################################################################
