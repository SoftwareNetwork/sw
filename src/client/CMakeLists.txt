#
# cppan
#

########################################
# fix imports
########################################

set(src
    fix_imports.cpp
    fix_imports.h
)

add_library(fix_imports ${src})

########################################
# cppan
########################################

set(cppan_src
    main.cpp
    options.cpp
    options.h
)

add_executable(client ${cppan_src})
target_link_libraries(client
    common
    fix_imports
    yaml-cpp
    ${Boost_LIBRARIES}
)
set_target_properties(client PROPERTIES OUTPUT_NAME cppan)
set_target_properties(client PROPERTIES OUTPUT_NAME_DEBUG cppand)

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
