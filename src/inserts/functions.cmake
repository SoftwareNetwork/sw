################################################################################
#
# CPPAN macros and functions
#
################################################################################

########################################
# FUNCTION set_win32
########################################

function(set_win32 var)
    if (WIN32)
        set(${var} "${ARGN}" PARENT_SCOPE)
    endif()
endfunction(set_win32)

########################################
# FUNCTION set_unix
########################################

function(set_unix var)
    if (UNIX)
        set(${var} "${ARGN}" PARENT_SCOPE)
    endif()
endfunction(set_unix)

########################################
# FUNCTION set_apple
########################################

function(set_apple var)
    if (APPLE)
        set(${var} "${ARGN}" PARENT_SCOPE)
    endif()
endfunction(set_apple)

########################################
# FUNCTION add_src_win32
########################################

function(add_src_win32 var)
    if (WIN32)
        set(${var} ${${var}} ${ARGN} PARENT_SCOPE)
    endif()
endfunction(add_src_win32)

########################################
# FUNCTION add_src_unix
########################################

function(add_src_unix var)
    if (UNIX)
        set(${var} ${${var}} ${ARGN} PARENT_SCOPE)
    endif()
endfunction(add_src_unix)

########################################
# FUNCTION add_src_unix_not_apple
########################################

function(add_src_unix_not_apple var)
    if (UNIX AND NOT APPLE)
        set(${var} ${${var}} ${ARGN} PARENT_SCOPE)
    endif()
endfunction(add_src_unix_not_apple)

########################################
# FUNCTION add_src_apple
########################################

function(add_src_apple var)
    if (APPLE)
        set(${var} ${${var}} ${ARGN} PARENT_SCOPE)
    endif()
endfunction(add_src_apple)

########################################
# FUNCTION remove_src
########################################

function(remove_src var)
    list(REMOVE_ITEM src "${CMAKE_CURRENT_SOURCE_DIR}/${var}")
endfunction(remove_src)

########################################
# FUNCTION remove_src_win32
########################################

function(remove_src_win32 var)
    if (WIN32)
        list(REMOVE_ITEM src "${CMAKE_CURRENT_SOURCE_DIR}/${var}")
    endif()
endfunction(remove_src_win32)

########################################
# FUNCTION remove_src_unix
########################################

function(remove_src_unix var)
    if (UNIX)
        list(REMOVE_ITEM src "${CMAKE_CURRENT_SOURCE_DIR}/${var}")
    endif()
endfunction(remove_src_unix)

########################################
# FUNCTION project_group
########################################

function(project_group target name)
    set_target_properties(${target} PROPERTIES FOLDER ${name})
endfunction(project_group)

########################################
# FUNCTION file_write_once
########################################

function(file_write_once f c)
    if (NOT EXISTS ${f})
        file(WRITE ${f} "${c}")
    endif()
endfunction(file_write_once)

################################################################################