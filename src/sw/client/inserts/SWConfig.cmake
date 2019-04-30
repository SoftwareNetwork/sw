################################################################################

find_program(SW_EXECUTABLE SW)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(SW DEFAULT_MSG SW_EXECUTABLE)
mark_as_advanced(SW_EXECUTABLE)

########################################

set(SW_DEPS_DIR ${CMAKE_BINARY_DIR}/.sw/cmake CACHE STRING "SW local deps dir.")
set(SW_DEPS_FILE ${SW_DEPS_DIR}/sw.txt CACHE STRING "SW local deps file.")

file(WRITE ${SW_DEPS_FILE} "")

########################################
# FUNCTION sw_add_dependency
########################################

function(sw_add_package)
    foreach(a ${ARGN})
        file(APPEND ${SW_DEPS_FILE} "${a}\n")
    endforeach()
endfunction()

########################################
# FUNCTION sw_execute
########################################

function(sw_execute)
    message(STATUS "sw: processing dependencies")

    if (SW_FORCE)
        set(SW_FORCE -s)
    endif()

    set(stsh -static)
    if (SW_BUILD_SHARED_LIBS)
        set(stsh -shared)
    endif()

    set(platform)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(platform x64)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(platform x86)
    endif()

    set(sw_platform_args
        ${stsh}
        -platform ${platform}
        #-compiler msvc
    )

    set(swcmd
        ${SW_EXECUTABLE}
            ${sw_platform_args}
            -d "${SW_DEPS_DIR}"
            ${SW_FORCE}
            integrate
            -cmake-deps "${SW_DEPS_FILE}"
    )

    execute_process(
        COMMAND ${swcmd}
        RESULT_VARIABLE ret
    )
    if (NOT ${ret} EQUAL 0)
        string(REPLACE ";" " " swcmd "${swcmd}")
        message("sw command: ${swcmd}")
        message(FATAL_ERROR "sw: non-zero exit code - ${ret}")
    endif()

    set(outdir)
    if (CMAKE_RUNTIME_OUTPUT_DIRECTORY)
        set(outdir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    else()
        set(outdir ${CMAKE_BINARY_DIR})
    endif()

    string(SHA1 depshash "${sw_platform_args}")
    string(SUBSTRING "${depshash}" 0 8 depshash)

    # add deps targets
    add_custom_target(sw_build_dependencies ALL
        COMMAND
            sw
                ${sw_platform_args}
                -config
                    $<$<CONFIG:Debug>:d>
                    $<$<CONFIG:MinSizeRel>:msr>
                    $<$<CONFIG:RelWithDebInfo>:rwdi>
                    $<$<CONFIG:Release>:r>
                build @${SW_DEPS_FILE}
                -ide-copy-to-dir
                ${outdir}/$<CONFIG>
                -ide-fast-path
                ${SW_DEPS_DIR}/$<CONFIG>-${depshash}.deps
    )
    set_target_properties(sw_build_dependencies
        PROPERTIES
            FOLDER ". SW Predefined Targets"
            PROJECT_LABEL "BUILD_DEPENDENCIES"
    )

    add_subdirectory(${SW_DEPS_DIR} ${SW_DEPS_DIR}/cmake_bdir)
endfunction()

################################################################################
