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
# generator
########################################

#set(gen_src
#    generator.cpp
#	schema.json
#)
#add_executable(generator ${gen_src})
#target_link_libraries(generator context cppan)

########################################
# common
########################################

#set(PARSER_SRC ${CMAKE_CURRENT_BINARY_DIR}/parser.h)

#add_custom_command(OUTPUT ${PARSER_SRC}
#	COMMAND generator ${CMAKE_CURRENT_SOURCE_DIR}/schema.json ${PARSER_SRC}
#	DEPENDS generator ${CMAKE_CURRENT_SOURCE_DIR}/schema.json
#)

set(common_src
    common.cpp
    common.h
    cppan.cpp
    cppan.h
    dependency.h
    filesystem.h
    enums.cpp
    enums.h
    property_tree.h
    project_path.cpp
    project_path.h

	#schema.json

	#${PARSER_SRC}
)
add_library(common ${common_src})
target_compile_definitions(common
    PRIVATE -DVERSION_MAJOR=${VERSION_MAJOR}
    PRIVATE -DVERSION_MINOR=${VERSION_MINOR}
    PRIVATE -DVERSION_PATCH=${VERSION_PATCH}
)
target_link_libraries(common context)# cppan)
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

add_subdirectory(client)

################################################################################
