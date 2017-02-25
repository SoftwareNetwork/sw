#
# cppan
#

########################################
# cppan
########################################

file(GLOB cppan_src "*")

add_executable(client ${cppan_src})
target_link_libraries(client common)

if (NOT CPPAN_BUILD)
target_link_libraries(client
    yaml-cpp
    #readline
    ${Boost_LIBRARIES}
)
endif()

set_target_properties(client PROPERTIES OUTPUT_NAME cppan)
if (WIN32)
set_target_properties(client PROPERTIES OUTPUT_NAME_DEBUG cppand)
endif()

if (NOT WIN32)
target_link_libraries(client
    pthread
)
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
