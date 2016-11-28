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
target_link_libraries(fix_imports context)

########################################
# cppan
########################################

set(cppan_src
    main.cpp
    build.cpp
    build.h
    options.cpp
    options.h
)

add_executable(client ${cppan_src})
target_link_libraries(client
    autotools
    common
    fix_imports
)

if (NOT CPPAN_BUILD)
target_link_libraries(client
    yaml-cpp
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
