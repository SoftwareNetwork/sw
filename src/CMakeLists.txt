#
# cppan
#

################################################################################

if (WIN32)
    add_definitions(-D_WIN32_WINNT=0x0601)
endif()

add_definitions(-DUSE_LOGGER=1)

# process is still in deps
include_directories(${PROJECT_SOURCE_DIR}/dep/process)

# for dirs & stamps
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
target_link_libraries(inserter pvt.cppan.demo.boost.filesystem)

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

target_link_libraries(common
    pvt.cppan.demo.boost.log
    pvt.cppan.demo.badger.curl.libcurl
    pvt.cppan.demo.libarchive.libarchive
    pvt.cppan.demo.jbeder.yaml_cpp
    pvt.cppan.demo.sqlite3
    pvt.cppan.demo.coruus.keccak_tiny_unrolled
)

################################################################################

add_subdirectory(bazel)
add_subdirectory(client)
add_subdirectory(comments)

################################################################################
