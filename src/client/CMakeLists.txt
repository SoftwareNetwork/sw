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
# builder
########################################

BISON_TARGET(grammar
    "${CMAKE_CURRENT_SOURCE_DIR}/grammar.yy"
    "${CMAKE_CURRENT_BINARY_DIR}/grammar.cpp"
)
FLEX_TARGET(lexer
    "${CMAKE_CURRENT_SOURCE_DIR}/lexer.ll"
    "${CMAKE_CURRENT_BINARY_DIR}/lexer.cpp"
    COMPILE_FLAGS --header-file=${CMAKE_CURRENT_BINARY_DIR}/lexer.h
)
ADD_FLEX_BISON_DEPENDENCY(lexer grammar)

include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(parser_src
    ${BISON_grammar_OUTPUTS}
    ${FLEX_lexer_OUTPUTS}
    ${CMAKE_CURRENT_BINARY_DIR}/lexer.h
)
set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/lexer.h PROPERTIES GENERATED TRUE)

set(builder_src
    build.cpp
    build.h
    driver.cpp
    driver.h
    grammar.yy
    lexer.ll

    ${parser_src}
)

add_library(builder ${builder_src})

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
    builder
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
