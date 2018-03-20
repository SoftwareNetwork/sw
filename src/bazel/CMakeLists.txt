#
# cppan
#

################################################################################

########################################
# bazel_parser
########################################

if (FLEX_FOUND AND BISON_FOUND)
    BISON_TARGET(grammar
        "${CMAKE_CURRENT_SOURCE_DIR}/grammar.yy"
        "${CMAKE_CURRENT_SOURCE_DIR}/grammar.cpp"
    )
    FLEX_TARGET(lexer
        "${CMAKE_CURRENT_SOURCE_DIR}/lexer.ll"
        "${CMAKE_CURRENT_SOURCE_DIR}/lexer.cpp"
        COMPILE_FLAGS "--header-file=${CMAKE_CURRENT_SOURCE_DIR}/lexer.h -L"
    )
    ADD_FLEX_BISON_DEPENDENCY(lexer grammar)

    set(parser_src
        ${BISON_grammar_OUTPUTS}
        ${FLEX_lexer_OUTPUTS}
        ${CMAKE_CURRENT_SOURCE_DIR}/lexer.h
    )
    set_source_files_properties(${CMAKE_CURRENT_SOURCE_DIR}/lexer.h PROPERTIES GENERATED TRUE)

    set(bazel_parser_src
        bazel.cpp
        bazel.h
        driver.cpp
        driver.h
	    grammar.yy
	    lexer.ll
    )
else()
    file(GLOB parser_src "*")
endif()

add_library(bazel_parser ${parser_src} ${bazel_parser_src})
target_link_libraries(bazel_parser
    common
    pvt.cppan.demo.imageworks.pystring
)
set_property(TARGET bazel_parser PROPERTY FOLDER parse)

########################################
# bazel_parser_test
########################################

add_executable(bazel_parser_test test/test.cpp)
target_link_libraries(bazel_parser_test bazel_parser)
set_property(TARGET bazel_parser_test PROPERTY FOLDER test)

################################################################################
