#
# cppan
#

################################################################################
#
# tests
#
################################################################################

macro(cppan_add_test___ name)
    set(bdir ${CMAKE_CURRENT_BINARY_DIR}/test/${name})
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${name}.yml ${bdir}/cppan.yml)
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/runtests.cmake ${bdir}/runtests.cmake)
    #execute_process(COMMAND echo ${ARGN} >> ${bdir}/runtests.cmake)
    file(APPEND ${bdir}/dummy.cpp "int main(){}")

    add_test(NAME ${name}
        COMMAND ${CMAKE_COMMAND}
            -DCPPAN_COMMAND=$<TARGET_FILE:client>
            -P ${bdir}/runtests.cmake
    )
endmacro()

macro(set_bdir name type)
    set(bdir ${CMAKE_CURRENT_BINARY_DIR}/${type}/${name})
endmacro()

macro(cppan_add_test name type)
    add_test(NAME ${name}-${type}
        COMMAND ${ARGN}
    )
endmacro()

macro(cppan_add_test_bdir name type)
    cppan_add_test(${name} ${type} $<TARGET_FILE:client> -d ${bdir} ${ARGN})
endmacro()

macro(cppan_add_test_simple name)
    set(type simple)
    set_bdir(${name} ${type})
    cppan_add_test_bdir(${name} ${type})

    execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${name}.yml ${bdir}/cppan.yml)
endmacro()

macro(cppan_add_test_generate name)
    set(type generate)
    set_bdir(${name} ${type})
    cppan_add_test_bdir(${name} ${type} --generate .)

    execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${name}.yml ${bdir}/cppan.yml)
    file(WRITE ${bdir}/dummy.cpp "int main(){}")
endmacro()

macro(cppan_add_test_build name)
    set(type build)
    set_bdir(${name} ${type})
    cppan_add_test_bdir(${name} ${type} --build .)

    execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${name}.yml ${bdir}/cppan.yml)
    file(WRITE ${bdir}/dummy.cpp "int main(){}")
endmacro()

macro(cppan_add_test_suite name)
    cppan_add_test_simple(${name})
    cppan_add_test_generate(${name})
    cppan_add_test_build(${name})
endmacro()

cppan_add_test_simple(zlib)
cppan_add_test_simple(zlib_no_cache)
cppan_add_test_suite(dep_in_dep_png_nanobp)
cppan_add_test_suite(dep_in_dep_png_nanobp_no_cache)

# ninja
find_program(n ninja)
find_program(nb ninja-build)
if (NOT WIN32 AND NOT ("${n}" STREQUAL "n-NOTFOUND" AND "${nb}" STREQUAL "nb-NOTFOUND"))
cppan_add_test_suite(dep_in_dep_png_nanobp_ninja)
# FIXME:
#cppan_add_test_suite(dep_in_dep_png_nanobp_ninja_strict)
cppan_add_test_suite(dep_in_dep_png_nanobp_no_cache_ninja)
cppan_add_test_suite(dep_in_dep_png_nanobp_no_cache_ninja_strict)
endif()

################################################################################
