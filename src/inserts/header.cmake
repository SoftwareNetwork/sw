#
# cppan
#

if (NOT CPPAN_CMAKE_VERBOSE)
    set(CPPAN_CMAKE_VERBOSE 0)
endif()

########################################
# FUNCTION cppan_debug_message
########################################

function(cppan_debug_message)
    if (CPPAN_CMAKE_VERBOSE)
        message(STATUS "${ARGV}")
    endif()
endfunction(cppan_debug_message)

################################################################################
