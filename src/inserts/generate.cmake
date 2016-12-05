get_configuration_variables()

set(build_dir_name build)
set(build_dir ${current_dir}/${build_dir_name}/${config_dir})
set(export_dir ${build_dir}/exports)
set(import ${export_dir}/${variable_name}.cmake)
set(import_fixed ${export_dir}/${variable_name}-fixed.cmake)
set(aliases_file ${export_dir}/${variable_name}-aliases.cmake)
set(variables_file ${build_dir}.gen.vars)
set(lock ${build_dir}/cppan_generate.lock)

########################################

if (NOT EXISTS ${import} OR NOT EXISTS ${import_fixed})
    file(
        LOCK ${lock}
        GUARD FILE # CMake bug workaround https://gitlab.kitware.com/cmake/cmake/issues/16295
        TIMEOUT 0
        RESULT_VARIABLE lock_result
    )
    if (NOT ${lock_result} EQUAL 0)
        message(STATUS "WARNING: Target: ${target}")
        message(STATUS "WARNING: Other project is being bootstrapped right now or you hit a circular deadlock.")
        message(STATUS "WARNING: If you aren't building other projects right now feel free to kill this process or it will be stopped in 90 seconds.")

        file(
            LOCK ${lock}
            GUARD FILE # CMake bug workaround https://gitlab.kitware.com/cmake/cmake/issues/16295
            TIMEOUT 90
            RESULT_VARIABLE lock_result
        )

        if (NOT ${lock_result} EQUAL 0)
            message(FATAL_ERROR "Lock error: ${lock_result}")
        endif()
    endif()

    # double check
    if (NOT EXISTS ${import} OR NOT EXISTS ${import_fixed})
        set(generator ${CMAKE_GENERATOR})

        # copy cmake cache for faster bootstrapping
        set(to ${build_dir}/CMakeFiles/${CMAKE_VERSION})
        if (NOT EXISTS ${to})
            if (EXECUTABLE)
                set(from ${storage_cfg_dir}/${config_exe}/CMakeFiles/${CMAKE_VERSION})
            else()
                set(from ${CMAKE_BINARY_DIR}/CMakeFiles/${CMAKE_VERSION})
            endif()

            if (EXISTS ${from})
                execute_process(
                    COMMAND ${CMAKE_COMMAND} -E copy_directory ${from} ${to}
                    RESULT_VARIABLE ret
                )
                check_result_variable(${ret})
            endif()
        endif()

        # prepare variables for child process
        set(OUTPUT_DIR ${config})
        if (EXECUTABLE)
            # TODO: try to work 0->1 <- why? maybe left as is?
            set(CPPAN_BUILD_SHARED_LIBS 0)
        endif()

        if (NOT CPPAN_COMMAND)
            message(FATAL_ERROR "cppan command '${CPPAN_COMMAND}' not found - ${CMAKE_CURRENT_LIST_FILE} - ${target}")
        endif()

        #
        add_variable(GEN_CHILD_VARS OUTPUT_DIR)
        add_variable(GEN_CHILD_VARS CPPAN_BUILD_SHARED_LIBS)
        add_variable(GEN_CHILD_VARS CPPAN_BUILD_EXECUTABLES_WITH_SAME_CONFIG)
        add_variable(GEN_CHILD_VARS CPPAN_COMMAND)
        add_variable(GEN_CHILD_VARS CPPAN_MT_BUILD)
        add_variable(GEN_CHILD_VARS CPPAN_CMAKE_VERBOSE)
        write_variables_file(GEN_CHILD_VARS ${variables_file})
        #

        message(STATUS "")
        message(STATUS "Preparing build tree for ${target} (${config})")
        message(STATUS "")

        # call cmake
        if (EXECUTABLE)
                cppan_debug_message("COMMAND ${CMAKE_COMMAND}
                        -H${current_dir} -B${build_dir}
                        -DVARIABLES_FILE=${variables_file}")
                execute_process(
                    COMMAND ${CMAKE_COMMAND}
                        -H${current_dir} -B${build_dir}
                        -DVARIABLES_FILE=${variables_file}
                    RESULT_VARIABLE ret
                )
                check_result_variable(${ret})
        else()
            if (CMAKE_TOOLCHAIN_FILE)
                cppan_debug_message("COMMAND ${CMAKE_COMMAND}
                        -H${current_dir} -B${build_dir}
                        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
                        -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
                        -G \"${generator}\"
                        -DVARIABLES_FILE=${variables_file}")
                execute_process(
                    COMMAND ${CMAKE_COMMAND}
                        -H${current_dir} -B${build_dir}
                        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
                        -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
                        -G "${generator}"
                        -DVARIABLES_FILE=${variables_file}
                    RESULT_VARIABLE ret
                )
                check_result_variable(${ret})
            else(CMAKE_TOOLCHAIN_FILE)
                cppan_debug_message("COMMAND ${CMAKE_COMMAND}
                        -H${current_dir} -B${build_dir}
                        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
                        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
                        -G \"${generator}\"
                        -DVARIABLES_FILE=${variables_file}")
                execute_process(
                    COMMAND ${CMAKE_COMMAND}
                        -H${current_dir} -B${build_dir}
                        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
                        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
                        -G "${generator}"
                        -DVARIABLES_FILE=${variables_file}
                    RESULT_VARIABLE ret
                )
                check_result_variable(${ret})
            endif(CMAKE_TOOLCHAIN_FILE)
        endif()

        # store ${import} file hash
        file(MD5 ${import} md5)
        file(WRITE ${import}.md5 "${md5}")

        # fix
        file(WRITE ${aliases_file} "${aliases}")
        cppan_debug_message("COMMAND ${CPPAN_COMMAND} internal-fix-imports ${target} ${aliases_file} ${import} ${import_fixed}")
        execute_process(
            COMMAND ${CPPAN_COMMAND} internal-fix-imports ${target} ${aliases_file} ${import} ${import_fixed}
            RESULT_VARIABLE ret
        )
        check_result_variable(${ret})
    endif()

    file(LOCK ${lock} RELEASE)
endif()
