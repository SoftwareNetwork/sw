################################################################################

find_program(CPPAN_EXECUTABLE cppan)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CPPAN DEFAULT_MSG CPPAN_EXECUTABLE)
mark_as_advanced(CPPAN_EXECUTABLE)

########################################

set(CPPAN_DEPS_DIR ${CMAKE_BINARY_DIR}/.cppan/cmake CACHE STRING "Cppan local deps file.")
set(CPPAN_DEPS_FILE ${CPPAN_DEPS_DIR}/cppan.yml CACHE STRING "Cppan local deps file.")

file(WRITE ${CPPAN_DEPS_FILE} "dependencies:\n")

########################################
# FUNCTION cppan_add_dependency
########################################

function(cppan_add_dependency d)
    if (${ARGC} EQUAL 1)
        file(APPEND ${CPPAN_DEPS_FILE} "    - ${d}\n")
    else()
        file(APPEND ${CPPAN_DEPS_FILE} "    - ${d}: ${ARGN}\n")
    endif()
endfunction()

########################################
# FUNCTION cppan_execute
########################################

function(cppan_execute)
    message(STATUS "cppan: processing dependencies")

    execute_process(
        COMMAND ${CPPAN_EXECUTABLE} -d "${CPPAN_DEPS_DIR}"
        RESULT_VARIABLE ret
    )
    if (NOT ${ret} EQUAL 0)
        message(FATAL_ERROR "cppan: non-zero exit code - ${ret}")
    endif()

    file(WRITE ${CPPAN_DEPS_FILE} "")
    add_subdirectory(${CPPAN_DEPS_DIR}/.cppan ${CPPAN_DEPS_DIR}/.cppan/cmake_bdir)
endfunction()

################################################################################
