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
    set(src ${src} PARENT_SCOPE)
endfunction(remove_src)

########################################
# FUNCTION remove_src_win32
########################################

function(remove_src_win32 var)
    if (WIN32)
        list(REMOVE_ITEM src "${CMAKE_CURRENT_SOURCE_DIR}/${var}")
        set(src ${src} PARENT_SCOPE)
    endif()
endfunction(remove_src_win32)

########################################
# FUNCTION remove_src_unix
########################################

function(remove_src_unix var)
    if (UNIX)
        list(REMOVE_ITEM src "${CMAKE_CURRENT_SOURCE_DIR}/${var}")
        set(src ${src} PARENT_SCOPE)
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

########################################
# FUNCTION find_flag
########################################

function(find_flag in_flags f out)
    if (NOT ${${out}} STREQUAL "")
        return()
    endif()
    set(flags ${in_flags})
    string(TOLOWER ${flags} flags)
    string(FIND "${flags}" "${f}" flags)
    if (NOT ${flags} EQUAL -1)
        set(${out} -mt PARENT_SCOPE)
    endif()
endfunction(find_flag)

########################################
# FUNCTION get_configuration
########################################

function(get_configuration out)
    set(mt_flag)
    if (MSVC)
        find_flag(${CMAKE_CXX_FLAGS_RELEASE} /mt mt_flag)
        find_flag(${CMAKE_CXX_FLAGS_DEBUG} /mtd mt_flag)
    endif()

    set(ninja_dir)
    if (CMAKE_GENERATOR STREQUAL Ninja)
        set(ninja_dir -ninja)
    endif()

    set(config ${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_CXX_COMPILER_ID}${ninja_dir})
    string(REGEX MATCH "[0-9]+\\.[0-9]" version "${CMAKE_CXX_COMPILER_VERSION}")
    if (CMAKE_SIZEOF_VOID_P)
        math(EXPR bits "${CMAKE_SIZEOF_VOID_P} * 8")
    elseif(SIZEOF_VOID_P)
        math(EXPR bits "${SIZEOF_VOID_P} * 8")
    else()
        set(bits unk)
    endif()

    set(dll)
    if (CPPAN_BUILD_SHARED_LIBS)
        set(dll -dll)
    endif()

    set(config ${config}-${version}-${bits}${mt_flag}${dll})
    string(TOLOWER ${config} config)

    set(${out} ${config} PARENT_SCOPE)
endfunction(get_configuration)

########################################
# FUNCTION get_number_of_cores
########################################

function(get_number_of_cores NC)
    include(ProcessorCount)
    ProcessorCount(N)
    if(N EQUAL 0)
        set(N 2)
    endif()
    set(${NC} ${N} PARENT_SCOPE)
endfunction(get_number_of_cores)

########################################
# FUNCTION add_variable
########################################

function(add_variable v)
    list(APPEND CPPAN_VARIABLES_TYPES "STRING")
    list(APPEND CPPAN_VARIABLES_KEYS "${v}")
    if ("${${v}}" STREQUAL "")
        list(APPEND CPPAN_VARIABLES_VALUES "0")
    else()
        list(APPEND CPPAN_VARIABLES_VALUES "${${v}}")
    endif()

    set(CPPAN_VARIABLES_TYPES ${CPPAN_VARIABLES_TYPES} PARENT_SCOPE)
    set(CPPAN_VARIABLES_KEYS ${CPPAN_VARIABLES_KEYS} PARENT_SCOPE)
    set(CPPAN_VARIABLES_VALUES ${CPPAN_VARIABLES_VALUES} PARENT_SCOPE)

    set(CPPAN_NEW_VARIABLE_ADDED 1 PARENT_SCOPE)
endfunction(add_variable)

########################################
# FUNCTION read_variables_file
########################################

function(read_variables_file f)
    set(lock ${f}.lock)
    file(LOCK ${lock} RESULT_VARIABLE lock_result)
    if (NOT ${lock_result} EQUAL 0)
        message(FATAL_ERROR "Lock error: ${lock_result}")
    endif()

    if (NOT EXISTS ${f})
        file(LOCK ${lock} RELEASE)
        return()
    endif()

    file(STRINGS ${f} vars)
    list(LENGTH vars N)
    if (N EQUAL 0)
        file(LOCK ${lock} RELEASE)
        return()
    endif()
    math(EXPR N "${N}-1")
    foreach(i RANGE ${N})
        list(GET vars ${i} var)
        list(GET var 0 t)
        list(GET var 1 k)
        list(GET var 2 v)
        set(${k} "${v}" CACHE ${t} "Cached variable" FORCE)

        add_variable(${k})
        set(CPPAN_VARIABLES_TYPES ${CPPAN_VARIABLES_TYPES} PARENT_SCOPE)
        set(CPPAN_VARIABLES_KEYS ${CPPAN_VARIABLES_KEYS} PARENT_SCOPE)
        set(CPPAN_VARIABLES_VALUES ${CPPAN_VARIABLES_VALUES} PARENT_SCOPE)
    endforeach()

    file(LOCK ${lock} RELEASE)
endfunction(read_variables_file)

########################################
# FUNCTION write_variables_file
########################################

function(write_variables_file f)
    set(lock ${f}.lock)
    file(LOCK ${lock} RESULT_VARIABLE lock_result)
    if (NOT ${lock_result} EQUAL 0)
        message(FATAL_ERROR "Lock error: ${lock_result}")
    endif()

    list(LENGTH CPPAN_VARIABLES_TYPES N)
    math(EXPR N "${N}-1")
    file(WRITE ${f} "")
    foreach(i RANGE ${N})
        list(GET CPPAN_VARIABLES_TYPES ${i} type)
        list(GET CPPAN_VARIABLES_KEYS ${i} key)
        list(GET CPPAN_VARIABLES_VALUES ${i} value)
        set(vars "${type}" "${key}" "${value}")
        file(APPEND ${f} "${vars}\n")
    endforeach()

    file(LOCK ${lock} RELEASE)
endfunction(write_variables_file)

################################################################################
