macro(exec_check CMD)
    execute_process(COMMAND ${CMD} RESULT_VARIABLE CMD_RESULT)
    if(CMD_RESULT)
        message(FATAL_ERROR "Error running ${CMD}")
    endif()
endmacro()

