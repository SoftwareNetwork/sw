macro(exec_check CMD)
    execute_process(COMMAND ${CMD} RESULT_VARIABLE CMD_RESULT)
    if(CMD_RESULT)
        message(FATAL_ERROR "Error running ${CMD}")
    endif()
endmacro()

execute_process(COMMAND ${CMAKE_COMMAND} -E copy ${COPY_FROM} ${COPY_TO})
exec_check(${CPPAN_COMMAND})
