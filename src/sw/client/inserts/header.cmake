#
# sw
#

########################################
# FUNCTION sw_debug_message
########################################

function(sw_debug_message)
    if (SW_CMAKE_VERBOSE)
        message(STATUS "${SW_DEBUG_STACK_SPACE}${ARGV}")
    endif()
endfunction(sw_debug_message)

################################################################################
