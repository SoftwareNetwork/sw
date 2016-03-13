#
# cppan
#

file(GLOB_RECURSE cppan_src "*.cpp" "*.h")

add_executable(cppan ${cppan_src})
target_link_libraries(cppan
    common
    yaml-cpp
    ${Boost_LIBRARIES}
)
if (NOT WIN32)
target_link_libraries(cppan pthread)
endif()

if (WIN32)
install(TARGETS cppan RUNTIME
	DESTINATION .
	COMPONENT client
)
else()
install(TARGETS cppan RUNTIME
	DESTINATION /usr/local/bin
	COMPONENT client
)
endif()

