#
# cppan
#

########################################
# cppan
########################################

file(GLOB cppan_src "*")

add_executable(client ${cppan_src})
target_link_libraries(client common
    pvt.cppan.demo.boost.program_options
    pvt.cppan.demo.yhirose.cpp_linenoise
)
set_property(DIRECTORY
    PROPERTY COTIRE_PREFIX_HEADER_IGNORE_PATH
        "d:/dev/cppan_storage/src/74/81/c442/linenoise.hpp"
        "d:/dev/cppan_storage/src/74/81/c442/"
        "${CMAKE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}")
cotire(client)
#target_link_libraries(client -static-libstdc++ -static-libgcc)

if (CPPAN_TEST)
add_custom_command(TARGET client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:client> $<TARGET_FILE_DIR:client>/cppanx${CMAKE_EXECUTABLE_SUFFIX}
)
endif()

set_target_properties(client PROPERTIES OUTPUT_NAME cppan)
if (WIN32)
set_target_properties(client PROPERTIES OUTPUT_NAME_DEBUG cppand)
endif()

if (WIN32)
install(TARGETS client RUNTIME
	DESTINATION .
	COMPONENT client
)
else()
install(TARGETS client RUNTIME
	DESTINATION /usr/local/bin
	COMPONENT client
)
endif()

################################################################################
