#
# cppan
#

########################################
# FUNCTION cppan_debug_message
########################################

function(cppan_debug_message)
    if (CPPAN_CMAKE_VERBOSE)
        message(STATUS "${CPPAN_DEBUG_STACK_SPACE}${ARGV}")
    endif()
endfunction(cppan_debug_message)

################################################################################
