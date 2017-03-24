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
# inserter
########################################

add_executable(inserter gen/inserter.cpp)
set_property(TARGET inserter PROPERTY FOLDER gen)
target_link_libraries(inserter pvt.egorpugin.primitives.filesystem)

########################################
# support
########################################

file(GLOB common_src "support/*")
set(common_src ${common_src})
add_library(support ${common_src})
add_dependencies(support inserts_gen gen_stamp)
target_include_directories(support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/support)

target_link_libraries(support
    pvt.egorpugin.primitives.context
    pvt.egorpugin.primitives.hash
    pvt.egorpugin.primitives.http
    pvt.egorpugin.primitives.log
    pvt.egorpugin.primitives.lock
)

########################################
# common
########################################

add_custom_target(inserts_gen
    COMMAND inserter ${CMAKE_CURRENT_SOURCE_DIR}/inserts/inserts.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/inserts.cpp
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
set_property(TARGET inserts_gen PROPERTY FOLDER gen)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/inserts.cpp PROPERTIES GENERATED TRUE)

add_executable(stamp_genenerator gen/stamp_gen.cpp)
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
    inserts/inserts.cpp.in

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
    support
    bazel_parser
    extract_comments
)

target_link_libraries(common
    pvt.cppan.demo.jbeder.yaml_cpp
    pvt.cppan.demo.sqlite3

    pvt.egorpugin.primitives.pack
    pvt.egorpugin.primitives.date_time
    pvt.egorpugin.primitives.executor
)

################################################################################

add_subdirectory(bazel)
add_subdirectory(client)
add_subdirectory(comments)

################################################################################
