########################################

if (NOT EXISTS ${BUILD_DIR})
    message(STATUS "Build dir does not exists for package ${PACKAGE_STRING} (${BUILD_DIR})")
    message(STATUS "Re-run cppan to fix this warning.")
    return()
endif()

set(REBUILD 1)

if (EXISTS ${fn1})
    file(READ ${fn1} f1)
    if (EXISTS ${fn2})
        file(READ ${fn2} f2)
        if ("${f1}" STREQUAL "${f2}")
            set(REBUILD 0)
        endif()
    else()
        file(WRITE ${fn2} "${f1}")
    endif()
endif()

set(TARGET_INFO_FILE ${BUILD_DIR}/cppan_target_info_${CONFIG}.cmake)

set(TARGET_FILE)
if (EXISTS ${TARGET_INFO_FILE})
    include(${TARGET_INFO_FILE})
endif()

if (NOT REBUILD AND EXISTS ${TARGET_FILE})
    return()
endif()

set(lock ${BUILD_DIR}/cppan_build.lock)

file(
    LOCK ${lock}
    # cannot make GUARD FILE here: https://gitlab.kitware.com/cmake/cmake/issues/16480
    #GUARD FILE # CMake bug workaround https://gitlab.kitware.com/cmake/cmake/issues/16295
    RESULT_VARIABLE lock_result
)
if (NOT ${lock_result} EQUAL 0)
    message(FATAL_ERROR "Lock error: ${lock_result}")
endif()

# double check
if (NOT REBUILD AND EXISTS ${TARGET_FILE})
    # release before exit
    file(LOCK ${lock} RELEASE)

    return()
endif()

# save file
execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${fn1} ${fn2})

########################################
# preparation is over, build now
########################################

# TODO: add good message, close enough to generate message
#message(STATUS "Building ${target} (${config_unhashed} - ${config_dir} - ${generator})")

# make could be found on win32 os from cygwin for example
# we deny it on msvc and other build systems except for cygwin
if (NOT WIN32 OR CYGWIN)
    find_program(make make)
endif()

# set vars
set(OUTPUT_QUIET)
set(ERROR_QUIET)
if (DEFINED CPPAN_BUILD_VERBOSE AND NOT CPPAN_BUILD_VERBOSE)
    set(OUTPUT_QUIET OUTPUT_QUIET)
    set(ERROR_QUIET ERROR_QUIET)
endif()

# TODO: maybe provide better way of parallel build
# from cppan's build() call
# Q: how to pass -j options to cmake --build?
# A: cmake --build .  *more args* -- -jN
# maybe add similar options to cppan? '-- pass all args after two dashes to cmake build'
set(parallel)
if (MULTICORE)
    #message(STATUS "this is multicore build")
    #set(parallel "-j ${N_CORES}") # temporary
endif()
if (VISUAL_STUDIO AND CLANG)
    #message(STATUS "this is clang build")
    #get_number_of_cores(N_CORES)
    #set(parallel "/maxcpucount:${N_CORES}") # for msbuild
endif()

# TODO: replace other make calls with ${CMAKE_MAKE_PROGRAM} ?

if (NINJA)
    set(cmd ninja)
    if (CMAKE_MAKE_PROGRAM)
        find_program(cmd2 ${CMAKE_MAKE_PROGRAM})
        if (NOT ${cmd2} STREQUAL "cmd2-NOTFOUND")
            set(cmd ${CMAKE_MAKE_PROGRAM})
        endif()
    else()
        find_program(cmd2 ${cmd})
        if (${cmd2} STREQUAL "cmd2-NOTFOUND")
            find_program(cmd2 ninja-build)
            if (NOT ${cmd2} STREQUAL "cmd2-NOTFOUND")
                set(cmd ${cmd2})
            endif()
        endif()
    endif()

    cppan_debug_message("COMMAND ${cmd} -C ${BUILD_DIR}")
    execute_process(
        COMMAND ${cmd} -C ${BUILD_DIR}
        ${OUTPUT_QUIET}
        ${ERROR_QUIET}
        RESULT_VARIABLE ret
    )
elseif (CONFIG)
    if (NOT DEFINED make OR
        "${make}" STREQUAL "" OR
        "${make}" STREQUAL "make-NOTFOUND" OR
        XCODE)
        if (EXECUTABLE)
                if (CPPAN_BUILD_EXECUTABLES_WITH_SAME_CONFIGURATION)
                    cppan_debug_message("COMMAND ${CMAKE_COMMAND}
                            --build ${BUILD_DIR}
                            --config ${CONFIG}
                            #-- ${parallel}")
                    execute_process(
                        COMMAND ${CMAKE_COMMAND}
                            --build ${BUILD_DIR}
                            --config ${CONFIG}
                            #-- ${parallel}
                        ${OUTPUT_QUIET}
                        ${ERROR_QUIET}
                        RESULT_VARIABLE ret
                    )
                else()
                    cppan_debug_message("COMMAND ${CMAKE_COMMAND}
                            --build ${BUILD_DIR}
                            --config Release
                            #-- ${parallel}")
                    execute_process(
                        COMMAND ${CMAKE_COMMAND}
                            --build ${BUILD_DIR}
                            --config Release
                            #-- ${parallel}
                        ${OUTPUT_QUIET}
                        ${ERROR_QUIET}
                        RESULT_VARIABLE ret
                    )
                endif()
        else()
                cppan_debug_message("COMMAND ${CMAKE_COMMAND}
                        --build ${BUILD_DIR}
                        --config ${CONFIG}
                        #-- ${parallel}")
                execute_process(
                    COMMAND ${CMAKE_COMMAND}
                        --build ${BUILD_DIR}
                        --config ${CONFIG}
                        #-- ${parallel}
                    ${OUTPUT_QUIET}
                    ${ERROR_QUIET}
                    RESULT_VARIABLE ret
                )
        endif()
    else()
        cppan_debug_message("COMMAND make ${parallel} -C ${BUILD_DIR}")
        execute_process(
            COMMAND make ${parallel} -C ${BUILD_DIR}
            ${OUTPUT_QUIET}
            ${ERROR_QUIET}
            RESULT_VARIABLE ret
        )
    endif()
else()
    if ("${make}" STREQUAL "make-NOTFOUND")
        cppan_debug_message("COMMAND ${CMAKE_COMMAND}
                --build ${BUILD_DIR}
                #-- ${parallel}")
        execute_process(
            COMMAND ${CMAKE_COMMAND}
                --build ${BUILD_DIR}
                #-- ${parallel}
            ${OUTPUT_QUIET}
            ${ERROR_QUIET}
            RESULT_VARIABLE ret
        )
    else()
        cppan_debug_message("COMMAND make ${parallel} -C ${BUILD_DIR}")
        execute_process(
            COMMAND make ${parallel} -C ${BUILD_DIR}
            ${OUTPUT_QUIET}
            ${ERROR_QUIET}
            RESULT_VARIABLE ret
        )
    endif()
endif()

check_result_variable(${ret})

file(LOCK ${lock} RELEASE)

########################################
