set(REBUILD 1)

file(READ ${fn1} f1)
if (EXISTS ${fn2})
    file(READ ${fn2} f2)
    if (f1 STREQUAL f2)
        set(REBUILD 0)
    endif()
else()
    file(WRITE ${fn2} "${f1}")
endif()

if (NOT REBUILD AND EXISTS ${TARGET_FILE})
    return()
endif()

set(lock ${BUILD_DIR}/build.lock)

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

find_program(make make)

if (CONFIG)
    if (${make} STREQUAL "make-NOTFOUND")
        if (EXECUTABLE)
            execute_process(
                COMMAND ${CMAKE_COMMAND}
                    --build ${BUILD_DIR}
                    --config ${CONFIG}#Release # FIXME: always build exe with Release conf
            )
        else()
            execute_process(
                COMMAND ${CMAKE_COMMAND}
                    --build ${BUILD_DIR}
                    --config ${CONFIG}
            )
        endif()
    else()
        execute_process(
            COMMAND make -j${N_CORES} -C ${BUILD_DIR}
        )
    endif()
else()
    if (${make} STREQUAL "make-NOTFOUND")
        execute_process(
            COMMAND ${CMAKE_COMMAND}
                --build ${BUILD_DIR}
        )
    else()
        execute_process(
            COMMAND make -j${N_CORES} -C ${BUILD_DIR}
        )
    endif()
endif()

file(LOCK ${lock} RELEASE)
