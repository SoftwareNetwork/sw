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
target_include_directories(support PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/support)

if (UNIX AND NOT APPLE AND NOT EXISTS "/etc/centos-release")
target_link_libraries(support backtrace)
endif()
target_link_libraries(support
    pvt.cppan.demo.boost.property_tree
    pvt.cppan.demo.boost.stacktrace
    pvt.egorpugin.primitives.context
    pvt.egorpugin.primitives.hash
    pvt.egorpugin.primitives.http
    pvt.egorpugin.primitives.log
    pvt.egorpugin.primitives.lock
    pvt.egorpugin.primitives.yaml
)

########################################
# common
########################################

set(gen_src
	${CMAKE_CURRENT_SOURCE_DIR}/inserts/inserts.cpp.in
    ${CMAKE_CURRENT_SOURCE_DIR}/inserts/cppan.h
    ${CMAKE_CURRENT_SOURCE_DIR}/inserts/build.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/inserts/functions.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/inserts/generate.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/inserts/exports.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/inserts/header.cmake
    ${CMAKE_CURRENT_SOURCE_DIR}/inserts/version.rc.in
    ${CMAKE_CURRENT_SOURCE_DIR}/inserts/branch.rc.in
    ${CMAKE_CURRENT_SOURCE_DIR}/inserts/CPPANConfig.cmake
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/inserts.cpp
    COMMAND inserter ${CMAKE_CURRENT_SOURCE_DIR}/inserts/inserts.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/inserts.cpp
    DEPENDS ${gen_src}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(stamp_genenerator gen/stamp_gen.cpp)
set_property(TARGET stamp_genenerator PROPERTY FOLDER gen)
set(stamp ${CMAKE_CURRENT_BINARY_DIR}/stamp.h.in)
set_source_files_properties(${stamp} PROPERTIES GENERATED True)

if (NOT CPPAN_TEST OR NOT EXISTS ${stamp} OR 1)
add_custom_target(gen_stamp
    COMMAND stamp_genenerator > ${stamp}
)
set_property(TARGET gen_stamp PROPERTY FOLDER gen)
add_dependencies(support gen_stamp)
endif()

file(GLOB common_src "common/*" "printers/*")
set(common_src ${common_src}
    ${CMAKE_CURRENT_BINARY_DIR}/inserts.cpp
	${gen_src}

    ${stamp}
)
add_library(common ${common_src})

target_compile_definitions(common
    PUBLIC -DVERSION_MAJOR=${VERSION_MAJOR}
    PUBLIC -DVERSION_MINOR=${VERSION_MINOR}
    PUBLIC -DVERSION_PATCH=${VERSION_PATCH}
    PUBLIC -DBUILD_NUMBER=${BUILD_NUMBER}
    PUBLIC -DCPPAN_VERSION_STRING="${CPPAN_VERSION_STRING}"
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
    pvt.cppan.demo.sqlite3
    pvt.cppan.demo.fmt
    pvt.cppan.demo.mpark.variant

    pvt.egorpugin.primitives.pack
    pvt.egorpugin.primitives.date_time
    pvt.egorpugin.primitives.executor
    pvt.egorpugin.primitives.command
    pvt.egorpugin.primitives.win32helpers
)
if (WIN32)
target_link_libraries(common
    pvt.cppan.demo.giovannidicanio.winreg
)
endif()

################################################################################

add_subdirectory(bazel)
add_subdirectory(client)
add_subdirectory(comments)

################################################################################
