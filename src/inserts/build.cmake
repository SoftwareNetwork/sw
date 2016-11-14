set(REBUILD 1)

if (EXISTS ${fn1})
    file(READ ${fn1} f1)
    if (EXISTS ${fn2})
        file(READ ${fn2} f2)
        if (f1 STREQUAL f2)
            set(REBUILD 0)
        endif()
    else()
        file(WRITE ${fn2} "${f1}")
    endif()
endif()

if (NOT REBUILD AND EXISTS ${TARGET_FILE})
    return()
endif()

set(lock ${BUILD_DIR}/cppan_build.lock)

file(LOCK ${lock} RESULT_VARIABLE lock_result)
if (NOT ${lock_result} EQUAL 0)
    message(FATAL_ERROR "Lock error: ${lock_result}")
endif()

# double check
if (NOT REBUILD AND EXISTS ${TARGET_FILE})
    # release before exit
    file(LOCK ${lock} RELEASE)

    return()
endif()

execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${fn1} ${fn2})

# make could be found on win32 os from cygwin for example
# we deny it on msvc and other build systems except for cygwin
if (NOT WIN32 OR CYGWIN)
    find_program(make make)
endif()

if (CONFIG)
    if (NOT DEFINED make OR
        "${make}" STREQUAL "" OR
        "${make}" STREQUAL "make-NOTFOUND" OR
        XCODE)
        if (EXECUTABLE)
                if (CPPAN_BUILD_EXECUTABLES_WITH_SAME_CONFIG)
                    execute_process(
                        COMMAND ${CMAKE_COMMAND}
                            --build ${BUILD_DIR}
                            --config ${CONFIG}
                        RESULT_VARIABLE ret
                    )
                else()
                    execute_process(
                        COMMAND ${CMAKE_COMMAND}
                            --build ${BUILD_DIR}
                            --config Release
                        RESULT_VARIABLE ret
                    )
                endif()
        else()
                execute_process(
                    COMMAND ${CMAKE_COMMAND}
                        --build ${BUILD_DIR}
                        --config ${CONFIG}
                    RESULT_VARIABLE ret
                )
        endif()
    else()
        execute_process(
            COMMAND make -j${N_CORES} -C ${BUILD_DIR}
            RESULT_VARIABLE ret
        )
    endif()
else(CONFIG)
    if ("${make}" STREQUAL "make-NOTFOUND")
        execute_process(
            COMMAND ${CMAKE_COMMAND}
                --build ${BUILD_DIR}
            RESULT_VARIABLE ret
        )
    else()
        execute_process(
            COMMAND make -j${N_CORES} -C ${BUILD_DIR}
            RESULT_VARIABLE ret
        )
    endif()
endif(CONFIG)

file(LOCK ${lock} RELEASE)
