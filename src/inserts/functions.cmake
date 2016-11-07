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
# FUNCTION add_src
########################################

function(add_src var)
    list(APPEND src "${CMAKE_CURRENT_SOURCE_DIR}/${var}")
    set(src ${src} PARENT_SCOPE)
endfunction(add_src)

########################################
# FUNCTION add_src_win32
########################################

function(add_src_win32 var)
    if (WIN32)
        list(APPEND src "${CMAKE_CURRENT_SOURCE_DIR}/${var}")
        set(src ${src} PARENT_SCOPE)
    endif()
endfunction(add_src_win32)

########################################
# FUNCTION add_src_unix
########################################

function(add_src_unix var)
    if (UNIX)
        list(APPEND src "${CMAKE_CURRENT_SOURCE_DIR}/${var}")
        set(src ${src} PARENT_SCOPE)
    endif()
endfunction(add_src_unix)

########################################
# FUNCTION add_src_unix_not_apple
########################################

function(add_src_unix_not_apple var)
    if (UNIX AND NOT APPLE)
        list(APPEND src "${CMAKE_CURRENT_SOURCE_DIR}/${var}")
        set(src ${src} PARENT_SCOPE)
    endif()
endfunction(add_src_unix_not_apple)

########################################
# FUNCTION add_src_apple
########################################

function(add_src_apple var)
    if (APPLE)
        list(APPEND src "${CMAKE_CURRENT_SOURCE_DIR}/${var}")
        set(src ${src} PARENT_SCOPE)
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
# FUNCTION remove_src_dir
########################################

function(remove_src_dir var)
    file(GLOB_RECURSE rm "${var}")
    if (rm)
        list(REMOVE_ITEM src ${rm})
    endif()
    set(src ${src} PARENT_SCOPE)
endfunction(remove_src_dir)

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
    # multiple instances safe
    set(once ${f}.cppan.once)
    if (NOT EXISTS ${once})
        file_write_safe(${f} "${c}")
        file(WRITE ${once} "")
    endif()
endfunction(file_write_once)

########################################
# FUNCTION file_write_safe
########################################

function(file_write_safe f c)
    set(lock ${f}.lock)
    file(
        LOCK ${lock}
        GUARD FUNCTION # CMake bug workaround https://gitlab.kitware.com/cmake/cmake/issues/16295
        RESULT_VARIABLE lock_result
    )

    file(WRITE ${f} "${c}")

    file(LOCK ${lock} RELEASE)
endfunction(file_write_safe)

########################################
# FUNCTION find_flag
########################################

function(find_flag in_flags f out)
    if (NOT ${${out}} STREQUAL "")
        return()
    endif()
    set(flags ${in_flags})
    string(TOLOWER ${f} f)
    string(TOLOWER ${flags} flags)
    string(FIND "${flags}" "${f}" flags)
    if (NOT ${flags} EQUAL -1)
        set(${out} 1 PARENT_SCOPE)
    else()
        set(${out} 0 PARENT_SCOPE)
    endif()
endfunction(find_flag)

########################################
# FUNCTION get_configuration
########################################

function(get_configuration out)
    set(mt_flag)
    if (MSVC)
        find_flag(${CMAKE_CXX_FLAGS_RELEASE} /MT MTR)
        find_flag(${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MT MTRWDI)
        find_flag(${CMAKE_CXX_FLAGS_MINSIZEREL} /MT MTMSR)
        find_flag(${CMAKE_CXX_FLAGS_DEBUG} /MTd MTD)

        if (MTR OR MTRWDI OR MTMSR OR MTD)
            set(mt_flag -mt)
            set(CPPAN_MT_BUILD 1 CACHE STRING "MT (static crt) flag" FORCE)
        else()
            set(CPPAN_MT_BUILD 0 CACHE STRING "MT (static crt) flag" FORCE)
        endif()
    endif()

    set(cyg)
    if (CYGWIN)
        set(cyg cyg-)
    endif()

    set(config ${cyg}${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_CXX_COMPILER_ID})
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
# FUNCTION get_configuration_with_generator
########################################

function(get_configuration_with_generator out)
    get_configuration(config)

    set(generator ${CMAKE_GENERATOR})
    string(REPLACE " " "-" generator "${generator}")
    string(REPLACE "_" "-" generator "${generator}") # to made rfind('_') possible
    if (NOT "${generator}" STREQUAL "")
        set(config ${config}_${generator})
    endif()
    string(TOLOWER ${config} config)

    set(${out} ${config} PARENT_SCOPE)
endfunction(get_configuration_with_generator)

########################################
# FUNCTION get_configuration_exe
########################################

function(get_configuration_exe out)
    set(cyg)
    if (CYGWIN)
        set(cyg cyg-)
    endif()

    set(config ${cyg}${CMAKE_HOST_SYSTEM_PROCESSOR})
    string(TOLOWER ${config} config)
    set(${out} ${config} PARENT_SCOPE)
endfunction(get_configuration_exe)

########################################
# FUNCTION get_configuration_variables
########################################

function(get_configuration_variables)
    get_configuration(config_lib)
    get_configuration_with_generator(config_lib_gen)
    get_configuration_exe(config_exe)

    if (NOT EXECUTABLE OR LOCAL_PROJECT)
        set(config ${config_lib})
        set(config_dir ${config_lib_gen})
    else()
        set(config ${config_exe})
        set(config_dir ${config_exe})
    endif()

    set(config_lib ${config_lib} PARENT_SCOPE)
    set(config_lib_gen ${config_lib_gen} PARENT_SCOPE)
    set(config_exe ${config_exe} PARENT_SCOPE)
    set(config ${config} PARENT_SCOPE)
    set(config_dir ${config_dir} PARENT_SCOPE)
endfunction(get_configuration_variables)

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

function(add_variable array variable)
    list(APPEND ${array}_TYPES "STRING")
    list(APPEND ${array}_KEYS "${variable}")
    if ("${${variable}}" STREQUAL "")
        list(APPEND ${array}_VALUES "0")
    else()
        list(APPEND ${array}_VALUES "${${variable}}")
    endif()

    set(${array}_TYPES ${${array}_TYPES} PARENT_SCOPE)
    set(${array}_KEYS ${${array}_KEYS} PARENT_SCOPE)
    set(${array}_VALUES ${${array}_VALUES} PARENT_SCOPE)
endfunction(add_variable)

########################################
# FUNCTION unique_array
########################################

function(unique_array array)
    list(REMOVE_DUPLICATES ${array}_TYPES)
    list(REMOVE_DUPLICATES ${array}_KEYS)
    list(REMOVE_DUPLICATES ${array}_VALUES)

    set(${array}_TYPES ${${array}_TYPES} PARENT_SCOPE)
    set(${array}_KEYS ${${array}_KEYS} PARENT_SCOPE)
    set(${array}_VALUES ${${array}_VALUES} PARENT_SCOPE)
endfunction(unique_array)

########################################
# FUNCTION read_variables_file
########################################

function(read_variables_file array f)
    if (NOT EXISTS ${f})
        return()
    endif()

    set(lock ${f}.lock)
    file(
        LOCK ${lock}
        GUARD FUNCTION # CMake bug workaround https://gitlab.kitware.com/cmake/cmake/issues/16295
        RESULT_VARIABLE lock_result
    )
    if (NOT ${lock_result} EQUAL 0)
        message(FATAL_ERROR "Lock error: ${lock_result}")
    endif()

    file(STRINGS ${f} vars)
    file(LOCK ${lock} RELEASE)

    list(LENGTH vars N)
    if (N EQUAL 0)
        return()
    endif()
    math(EXPR N "${N}-1")
    foreach(i RANGE ${N})
        list(GET vars ${i} var)
        list(GET var 0 t)
        list(GET var 1 k)
        list(GET var 2 v)
        set(${k} "${v}" CACHE ${t} "Cached variable" FORCE)

        add_variable(${array} ${k})
    endforeach()

    # keep unique
    unique_array(${array})

    set(${array}_TYPES ${${array}_TYPES} PARENT_SCOPE)
    set(${array}_KEYS ${${array}_KEYS} PARENT_SCOPE)
    set(${array}_VALUES ${${array}_VALUES} PARENT_SCOPE)
endfunction(read_variables_file)

########################################
# FUNCTION write_variables_file
########################################

function(write_variables_file array f)
    set(lock ${f}.lock)
    file(
        LOCK ${lock}
        GUARD FUNCTION # CMake bug workaround https://gitlab.kitware.com/cmake/cmake/issues/16295
        RESULT_VARIABLE lock_result
    )
    if (NOT ${lock_result} EQUAL 0)
        message(FATAL_ERROR "Lock error: ${lock_result}")
    endif()

    # keep unique
    unique_array(${array})

    list(LENGTH ${array}_TYPES N)
    math(EXPR N "${N}-1")
    file(WRITE ${f} "")
    foreach(i RANGE ${N})
        list(GET ${array}_TYPES ${i} type)
        list(GET ${array}_KEYS ${i} key)
        list(GET ${array}_VALUES ${i} value)
        set(vars "${type}" "${key}" "${value}")
        file(APPEND ${f} "${vars}\n")
    endforeach()

    file(LOCK ${lock} RELEASE)
endfunction(write_variables_file)

########################################
# FUNCTION add_check_variable
########################################

function(add_check_variable v)
    add_variable(CPPAN_VARIABLES ${v})

    set(CPPAN_VARIABLES_TYPES ${CPPAN_VARIABLES_TYPES} PARENT_SCOPE)
    set(CPPAN_VARIABLES_KEYS ${CPPAN_VARIABLES_KEYS} PARENT_SCOPE)
    set(CPPAN_VARIABLES_VALUES ${CPPAN_VARIABLES_VALUES} PARENT_SCOPE)

    set(CPPAN_NEW_VARIABLE_ADDED 1 PARENT_SCOPE)
endfunction(add_check_variable)

########################################
# FUNCTION read_check_variables_file
########################################

function(read_check_variables_file f)
    read_variables_file(CPPAN_VARIABLES ${f})

    set(CPPAN_VARIABLES_TYPES ${CPPAN_VARIABLES_TYPES} PARENT_SCOPE)
    set(CPPAN_VARIABLES_KEYS ${CPPAN_VARIABLES_KEYS} PARENT_SCOPE)
    set(CPPAN_VARIABLES_VALUES ${CPPAN_VARIABLES_VALUES} PARENT_SCOPE)
endfunction(read_check_variables_file)

########################################
# FUNCTION write_check_variables_file
########################################

function(write_check_variables_file f)
    write_variables_file(CPPAN_VARIABLES ${f})
endfunction(write_check_variables_file)

########################################
# FUNCTION set_c_sources_as_cpp
########################################

function(set_c_sources_as_cpp)
    if (MSVC)
        file(GLOB_RECURSE csrc "*.c")
        set_source_files_properties(${csrc} PROPERTIES LANGUAGE CXX)
    endif()
endfunction(set_c_sources_as_cpp)

########################################
# FUNCTION add_win32_version_info
########################################

function(add_win32_version_info dir)
    if (WIN32 OR CYGWIN)
        if (NOT EXECUTABLE AND NOT LIBRARY_TYPE STREQUAL SHARED)
            return()
        endif()

        if (NOT CYGWIN)
            # cygwin won't have this var, so it will be eliminated from rc file
            set(PACKAGE_BUILD_CONFIG PACKAGE_BUILD_CONFIG)
        endif()

        if (PACKAGE_IS_BRANCH)
            set(rcfile_in ${STORAGE_DIR_ETC_STATIC}/branch.rc.in)
        else(PACKAGE_IS_VERSION)
            set(rcfile_in ${STORAGE_DIR_ETC_STATIC}/version.rc.in)
        endif()
        set(rcfile ${CMAKE_CURRENT_BINARY_DIR}/version.rc)

        configure_file(${rcfile_in} ${rcfile} @ONLY)

        set(src ${src} ${rcfile} PARENT_SCOPE)
    endif()
endfunction(add_win32_version_info)

########################################
# FUNCTION check_result_variable
########################################

function(check_result_variable ret)
    if (${ret} EQUAL 0)
        return()
    endif()
    message(FATAL_ERROR "Last CMake execute_process() called failed with error: ${ret}")
endfunction(check_result_variable)

################################################################################
