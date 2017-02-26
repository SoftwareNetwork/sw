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
    #pvt.cppan.demo.gnu.readline.readline
    pvt.cppan.demo.yhirose.cpp_linenoise
)

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
