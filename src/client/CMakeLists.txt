#
# cppan
#

file(GLOB_RECURSE cppan_src "*.cpp" "*.h")

add_executable(client ${cppan_src})
target_link_libraries(client
    common
    yaml-cpp
    ${Boost_LIBRARIES}
)

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

