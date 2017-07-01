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
    list(APPEND src ${SDIR}/${var})
    set(src ${src} PARENT_SCOPE)
endfunction(add_src)

########################################
# FUNCTION add_src_win32
########################################

function(add_src_win32 var)
    if (WIN32)
        list(APPEND src ${SDIR}/${var})
        set(src ${src} PARENT_SCOPE)
    endif()
endfunction(add_src_win32)

########################################
# FUNCTION add_src_unix
########################################

function(add_src_unix var)
    if (UNIX)
        list(APPEND src ${SDIR}/${var})
        set(src ${src} PARENT_SCOPE)
    endif()
endfunction(add_src_unix)

########################################
# FUNCTION add_src_unix_not_apple
########################################

function(add_src_unix_not_apple var)
    if (UNIX AND NOT APPLE)
        list(APPEND src ${SDIR}/${var})
        set(src ${src} PARENT_SCOPE)
    endif()
endfunction(add_src_unix_not_apple)

########################################
# FUNCTION add_src_apple
########################################

function(add_src_apple var)
    if (APPLE)
        list(APPEND src ${SDIR}/${var})
        set(src ${src} PARENT_SCOPE)
    endif()
endfunction(add_src_apple)

########################################
# FUNCTION add_src_dir
########################################

function(add_src_dir var)
    file(GLOB_RECURSE add ${SDIR}/${var})
    if (add)
        set(src ${src} ${add} PARENT_SCOPE)
    endif()
endfunction(add_src_dir)

########################################
# FUNCTION remove_src
########################################

function(remove_src var)
    list(REMOVE_ITEM src ${SDIR}/${var})
    set(src ${src} PARENT_SCOPE)
endfunction(remove_src)

########################################
# FUNCTION remove_src_win32
########################################

function(remove_src_win32 var)
    if (WIN32)
        list(REMOVE_ITEM src ${SDIR}/${var})
        set(src ${src} PARENT_SCOPE)
    endif()
endfunction(remove_src_win32)

########################################
# FUNCTION remove_src_unix
########################################

function(remove_src_unix var)
    if (UNIX)
        list(REMOVE_ITEM src ${SDIR}/${var})
        set(src ${src} PARENT_SCOPE)
    endif()
endfunction(remove_src_unix)

########################################
# FUNCTION remove_src_dir
########################################

function(remove_src_dir var)
    file(GLOB_RECURSE rm ${SDIR}/${var})
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
    string(SHA1 h "${c}")
    if (NOT EXISTS ${once})
        file_write_safe(${f} "${c}")
        file_write_safe(${once} "${h}")
        return()
    endif()
    file(READ ${once} h2)
    if (NOT "${h}" STREQUAL "${h2}")
        file_write_safe(${f} "${c}")
        file_write_safe(${once} "${h}")
        return()
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
# FUNCTION prepare_config_part
########################################

function(prepare_config_part o i)
    string(REPLACE " " "_" i "${i}")
    string(REPLACE "${CPPAN_CONFIG_PART_DELIMETER}" "_" i "${i}")
    string(TOLOWER ${i} i)
    set(${o} "${i}" PARENT_SCOPE)
endfunction(prepare_config_part)

########################################
# FUNCTION get_config_hash
########################################

function(get_config_hash c o)
    string(${CPPAN_CONFIG_HASH_METHOD} h "${c}")
    string(SUBSTRING "${h}" 0 ${CPPAN_CONFIG_HASH_SHORT_LENGTH} h)
    set(${o} "${h}" PARENT_SCOPE)
endfunction(get_config_hash)

########################################
# FUNCTION get_configuration_unhashed
########################################

function(get_configuration_unhashed out)
    set(mt_flag)
    if (MSVC)
        find_flag(${CMAKE_C_FLAGS_RELEASE}              /MT       C_MTR        )
        find_flag(${CMAKE_C_FLAGS_RELWITHDEBINFO}       /MT       C_MTRWDI     )
        find_flag(${CMAKE_C_FLAGS_MINSIZEREL}           /MT       C_MTMSR      )
        find_flag(${CMAKE_C_FLAGS_DEBUG}                /MTd      C_MTD        )
        find_flag(${CMAKE_CXX_FLAGS_RELEASE}            /MT     CXX_MTR        )
        find_flag(${CMAKE_CXX_FLAGS_RELWITHDEBINFO}     /MT     CXX_MTRWDI     )
        find_flag(${CMAKE_CXX_FLAGS_MINSIZEREL}         /MT     CXX_MTMSR      )
        find_flag(${CMAKE_CXX_FLAGS_DEBUG}              /MTd    CXX_MTD        )

        if (  C_MTR OR   C_MTRWDI OR   C_MTMSR OR   C_MTD OR
            CXX_MTR OR CXX_MTRWDI OR CXX_MTMSR OR CXX_MTD)
            set(mt_flag ${CPPAN_CONFIG_PART_DELIMETER}mt)
            set(CPPAN_MT_BUILD 1 CACHE STRING "MT (static crt) flag" FORCE)
        else()
            set(CPPAN_MT_BUILD 0 CACHE STRING "MT (static crt) flag" FORCE)
        endif()
    endif()

    prepare_config_part(system ${CMAKE_SYSTEM_NAME})
    prepare_config_part(processor ${CMAKE_SYSTEM_PROCESSOR})
    prepare_config_part(compiler ${CMAKE_CXX_COMPILER_ID})
    set(config ${system}${CPPAN_CONFIG_PART_DELIMETER}${processor}${CPPAN_CONFIG_PART_DELIMETER}${compiler})

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
        set(dll ${CPPAN_CONFIG_PART_DELIMETER}dll)
    endif()

    set(toolset)
    if (CMAKE_GENERATOR_TOOLSET)
        prepare_config_part(toolset ${CMAKE_GENERATOR_TOOLSET})
        set(toolset ${CPPAN_CONFIG_PART_DELIMETER}${toolset})
    endif()

    set(msvc_arch)
    if (MSVC AND (MSVC_C_ARCHITECTURE_ID OR MSVC_CXX_ARCHITECTURE_ID))
        prepare_config_part(msvc_arch ${MSVC_C_ARCHITECTURE_ID})
        set(msvc_arch ${CPPAN_CONFIG_PART_DELIMETER}${msvc_arch})

        # can they differ?
        if (MSVC_CXX_ARCHITECTURE_ID AND NOT "${MSVC_C_ARCHITECTURE_ID}" STREQUAL "${MSVC_CXX_ARCHITECTURE_ID}")
            prepare_config_part(msvc_arch2 ${MSVC_CXX_ARCHITECTURE_ID})
            set(msvc_arch ${msvc_arch}${CPPAN_CONFIG_PART_DELIMETER}${msvc_arch2})
        endif()
    endif()

    # add suffix (configuration) to distinguish build types
    # for non VS/XCODE builds
    set(configuration)
    if (NOT (XCODE OR VISUAL_STUDIO))
        set(configuration ${CPPAN_CONFIG_PART_DELIMETER}${CMAKE_BUILD_TYPE})
    endif()

    set(config ${config}${CPPAN_CONFIG_PART_DELIMETER}${version})
    set(config ${config}${CPPAN_CONFIG_PART_DELIMETER}${bits}${msvc_arch}${mt_flag}${dll}${toolset})
    set(config ${config}${configuration})

    set(${out} ${config} PARENT_SCOPE)
endfunction(get_configuration_unhashed)

########################################
# FUNCTION get_configuration_with_generator_unhashed
########################################

function(get_configuration_with_generator_unhashed out)
    get_configuration_unhashed(config)

    prepare_config_part(generator ${CMAKE_GENERATOR})
    if (NOT "${generator}" STREQUAL "")
        set(config ${config}${CPPAN_CONFIG_PART_DELIMETER}${generator})
    endif()

    set(${out} ${config} PARENT_SCOPE)
endfunction(get_configuration_with_generator_unhashed)

########################################
# FUNCTION get_configuration_exe_unhashed
########################################

function(get_configuration_exe_unhashed out)
    prepare_config_part(system ${CMAKE_SYSTEM_NAME})
    prepare_config_part(processor ${CMAKE_HOST_SYSTEM_PROCESSOR})
    set(config ${system}${CPPAN_CONFIG_PART_DELIMETER}${processor})

    # add generator to executables since we're using the same generator as for libraries
    prepare_config_part(generator ${CMAKE_GENERATOR})
    if (NOT "${generator}" STREQUAL "")
        set(config ${config}${CPPAN_CONFIG_PART_DELIMETER}${generator})
    endif()

    set(${out} ${config} PARENT_SCOPE)
endfunction(get_configuration_exe_unhashed)

########################################
# FUNCTION get_configuration_variables_unhashed
########################################

function(get_configuration_variables_unhashed)
    get_configuration_unhashed(config_lib)
    get_configuration_with_generator_unhashed(config_lib_gen)
    get_configuration_exe_unhashed(config_exe)

    if (NOT EXECUTABLE)
        set(config ${config_lib})
        set(config_dir ${config_lib_gen})
    else()
        set(config ${config_exe})
        set(config_dir ${config_exe})
    endif()

    set(config_unhashed ${config} PARENT_SCOPE)
    set(config_lib_unhashed ${config_lib} PARENT_SCOPE)
    set(config_lib_gen_unhashed ${config_lib_gen} PARENT_SCOPE)
    set(config_exe_unhashed ${config_exe} PARENT_SCOPE)
    set(config_dir_unhashed ${config_dir} PARENT_SCOPE)
endfunction(get_configuration_variables_unhashed)

########################################
# FUNCTION get_configuration
########################################

function(get_configuration out)
    get_configuration_unhashed(config)
    get_config_hash(${config} config)
    set(${out} ${config} PARENT_SCOPE)
endfunction(get_configuration)

########################################
# FUNCTION get_configuration_with_generator
########################################

function(get_configuration_with_generator out)
    get_configuration_with_generator_unhashed(config)
    get_config_hash(${config} config)
    set(${out} ${config} PARENT_SCOPE)
endfunction(get_configuration_with_generator)

########################################
# FUNCTION get_configuration_exe
########################################

function(get_configuration_exe out)
    get_configuration_exe_unhashed(config)
    get_config_hash(${config} config)
    set(${out} ${config} PARENT_SCOPE)
endfunction(get_configuration_exe)

########################################
# FUNCTION get_configuration_variables
########################################

function(get_configuration_variables)
    get_configuration(config_lib)
    get_configuration_with_generator(config_lib_gen)
    get_configuration_exe(config_exe)

    if (NOT EXECUTABLE)
        set(config ${config_lib})
        set(config_dir ${config_lib_gen})
    else()
        set(config ${config_exe})
        set(config_dir ${config_exe})
    endif()

    set(config ${config} PARENT_SCOPE)
    set(config_lib ${config_lib} PARENT_SCOPE)
    set(config_lib_gen ${config_lib_gen} PARENT_SCOPE)
    set(config_exe ${config_exe} PARENT_SCOPE)
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
    list(FIND ${array}_KEYS "${variable}" found)
    if(NOT found EQUAL -1)
        # check if new value is active, then replace
        list(GET ${array}_VALUES ${found} v)
        if (NOT v AND ${variable})
            #message(STATUS "Replacing element in array")
            #message(STATUS "Old value: ${v}")
            #message(STATUS "New value: ${${variable}}")
            #message(STATUS "Old array: ${${array}_VALUES}")
            list(REMOVE_AT ${array}_VALUES ${found})
            list(INSERT ${array}_VALUES ${found} "${${variable}}")
            #message(STATUS "New array: ${${array}_VALUES}")
            set(${array}_VALUES ${${array}_VALUES} PARENT_SCOPE)
        endif()

        return()
    endif()

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
# FUNCTION clear_variables
########################################

function(clear_variables array)
    set(${array}_TYPES)
    set(${array}_KEYS)
    set(${array}_VALUES)

    set(${array}_TYPES ${${array}_TYPES} PARENT_SCOPE)
    set(${array}_KEYS ${${array}_KEYS} PARENT_SCOPE)
    set(${array}_VALUES ${${array}_VALUES} PARENT_SCOPE)
endfunction(clear_variables)

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
    message(FATAL_ERROR "Last execute_process() with message '${ARGN}' failed with error: ${ret}")
endfunction(check_result_variable)

########################################
# FUNCTION replace_in_file_once
########################################

function(replace_in_file_once f from to)
    string(SHA1 h "${f}${from}${to}")
    string(SUBSTRING "${h}" 0 5 h)
    set(h ${f}.${h})

    if (EXISTS ${h})
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

    # double check
    if (EXISTS ${h})
        return()
    endif()

    file(READ ${f} fc)
    string(REPLACE "${from}" "${to}" fc "${fc}")
    file(WRITE "${f}" "${fc}")

    # create flag file
    file(WRITE ${h} "")
endfunction(replace_in_file_once)

########################################
# FUNCTION delete_in_file_once
########################################

function(delete_in_file_once f from)
    replace_in_file_once(${f} "${from}" "")
endfunction(delete_in_file_once)

########################################
# FUNCTION push_front_to_file_once
########################################

function(push_front_to_file_once f what)
    string(SHA1 h "${f}${what}")
    string(SUBSTRING "${h}" 0 5 h)
    set(h ${f}.${h})

    if (EXISTS ${h})
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

    # double check
    if (EXISTS ${h})
        return()
    endif()

    file(READ ${f} fc)
    file(WRITE "${f}" "${what}\n\n${fc}")

    # create flag file
    file(WRITE ${h} "")
endfunction(push_front_to_file_once)

########################################
# FUNCTION push_back_to_file_once
########################################

function(push_back_to_file_once f what)
    string(SHA1 h "${f}${what}")
    string(SUBSTRING "${h}" 0 5 h)
    set(h ${f}.${h})

    if (EXISTS ${h})
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

    # double check
    if (EXISTS ${h})
        return()
    endif()

    file(READ ${f} fc)
    file(WRITE "${f}" "${fc}\n\n${what}")

    # create flag file
    file(WRITE ${h} "")
endfunction(push_back_to_file_once)

########################################
# FUNCTION cppan_include
########################################

# this functions prevents changing variables in current scope
function(cppan_include f)
    include(${f})
endfunction(cppan_include)

########################################
# FUNCTION check_type_alignment
########################################

function(check_type_alignment TYPE LANG NAME)
    if (DEFINED ${NAME})
        return()
    endif()

    message(STATUS "Check alignment of ${TYPE} in ${LANG}")

    set(INCLUDE_HEADERS
        "#include <stddef.h>
        #include <stdio.h>
        #include <stdlib.h>")

    foreach(f ${CMAKE_REQUIRED_INCLUDES})
        set(INCLUDE_HEADERS "${INCLUDE_HEADERS}\n#include <${f}>\n")
    endforeach()

    if (HAVE_STDINT_H)
        set(INCLUDE_HEADERS "${INCLUDE_HEADERS}\n#include <stdint.h>\n")
    endif()

    file(WRITE "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/c_get_${NAME}_alignment.${LANG}"
        "${INCLUDE_HEADERS}
        int main()
        {
            char diff;
            struct foo {char a; ${TYPE} b;};
            struct foo *p = (struct foo *) malloc(sizeof(struct foo));
            diff = ((char *)&p->b) - ((char *)&p->a);
            return diff;
        }"
    )

    try_run(${NAME} COMPILE_RESULT "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/"
        "${CMAKE_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/CMakeTmp/c_get_${NAME}_alignment.${LANG}"
        COMPILE_OUTPUT_VARIABLE "${NAME}_COMPILE_VAR")

    message(STATUS "Check alignment of ${TYPE} in ${LANG}: ${${NAME}}")

    set(${NAME} ${${NAME}} CACHE STRING "Alignment of type: ${TYPE}" FORCE)
endfunction(check_type_alignment)

########################################
# FUNCTION copy_file_once
########################################

# this functions prevents changing variables in current scope
function(copy_file_once from to)
    if (NOT EXISTS ${to})
        execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${from} ${to})
    endif()
endfunction(copy_file_once)

########################################
# FUNCTION find_moc_targets
########################################

function(find_moc_targets out)
    set(o)
    foreach(fn ${ARGN})
        file(READ ${fn} f)
        string(FIND "${f}" "Q_OBJECT" i)
        if (NOT ${i} EQUAL -1)
            set(o ${o} ${fn})
        else()
            string(FIND "${f}" "Q_GADGET" i)
            if (NOT ${i} EQUAL -1)
                set(o ${o} ${fn})
            endif()
        endif()
    endforeach()
    set(${out} ${o} PARENT_SCOPE)
endfunction()

########################################
# FUNCTION set_src_header_only
########################################

function(set_src_header_only s)
    set(src2 ${src})
    list(FILTER src2 INCLUDE REGEX "${s}")
    list(LENGTH src2 N)
    if (N EQUAL 0)
        return()
    endif()
    set_source_files_properties(${src2} PROPERTIES HEADER_FILE_ONLY True)
    #set(src ${src} ${src2} PARENT_SCOPE)
endfunction()

########################################
# FUNCTION set_src_compiled
########################################

function(set_src_compiled s)
    set(src2 ${src})
    list(FILTER src2 INCLUDE REGEX "${s}")
    list(LENGTH src2 N)
    if (N EQUAL 0)
        return()
    endif()
    set_source_files_properties(${src2} PROPERTIES HEADER_FILE_ONLY False)
    #set(src ${src} ${src2} PARENT_SCOPE)
endfunction()

########################################
# FUNCTION moc_cpp_file
########################################

function(moc_cpp_file f)
    get_filename_component(n ${f} NAME_WE)
    qt5_create_moc_command(${SDIR}/${f} ${BDIR}/${n}.moc "" "" ${this} "")
    set(src ${src} ${BDIR}/${n}.moc PARENT_SCOPE)
endfunction()

########################################
# FUNCTION create_directory
########################################

function(create_directory d)
    execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${d})
endfunction()

########################################
# FUNCTION set_cache_var
########################################

function(set_cache_var variable value)
    set(${variable} ${value} CACHE STRING "" FORCE)
endfunction()

################################################################################
