if (NOT EXECUTABLE)
    get_configuration(config)
else()
    get_configuration_exe(config)
endif()

set(build_dir ${current_dir}/build/${config})
set(export_dir ${build_dir}/exports)
set(import ${export_dir}/${variable_name}.cmake)
set(import_fixed ${export_dir}/${variable_name}-fixed.cmake)
set(aliases_file ${export_dir}/${variable_name}-aliases.cmake)

if (NOT EXISTS ${import} OR NOT EXISTS ${import_fixed})
    set(lock ${build_dir}/cppan_generate.lock)

    file(LOCK ${lock} TIMEOUT 0 RESULT_VARIABLE lock_result)
    if (NOT ${lock_result} EQUAL 0)
        message(STATUS "WARNING: Target: ${target}")
        message(STATUS "WARNING: Other project is being bootstrapped right now or you hit a circular deadlock.")
        message(STATUS "WARNING: If you aren't building other projects right now feel free to kill this process or it will be stopped in 90 seconds.")

        file(LOCK ${lock} TIMEOUT 90 RESULT_VARIABLE lock_result)

        if (NOT ${lock_result} EQUAL 0)
            message(FATAL_ERROR "Lock error: ${lock_result}")
        endif()
    endif()

    # double check
    if (NOT EXISTS ${import} OR NOT EXISTS ${import_fixed})
        message(STATUS "")
        message(STATUS "Preparing build tree for ${target} with config ${config}")
        message(STATUS "")

        #find_program(ninja ninja)
        #set(generator Ninja)
        set(generator ${CMAKE_GENERATOR})
        if (MSVC
            OR "${ninja}" STREQUAL "ninja-NOTFOUND"
            OR CYGWIN # for me it's not working atm
        )
            set(generator ${CMAKE_GENERATOR})
        endif()

        # copy cmake cache for faster bootstrapping
        if (NOT EXISTS ${build_dir}/CMakeFiles/${CMAKE_VERSION})
            if (EXECUTABLE)
                # TODO: for exe we should find simple host conf to copy
                # maybe create and store it in some place in storage
            else()
                execute_process(
                    COMMAND ${CMAKE_COMMAND} -E
                        copy_directory
                        ${CMAKE_BINARY_DIR}/CMakeFiles/${CMAKE_VERSION}
                        ${build_dir}/CMakeFiles/${CMAKE_VERSION}
                )
            endif()
        endif()

        # call cmake
        if (EXECUTABLE)
                execute_process(
                    COMMAND ${CMAKE_COMMAND}
                        -H${current_dir} -B${build_dir}
                        #-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
                        #-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
                        #-G "${generator}"
                        -DOUTPUT_DIR=${config}
                        -DCPPAN_BUILD_SHARED_LIBS=0 # TODO: try to work 0->1
                )
        else(EXECUTABLE)
            if (CMAKE_TOOLCHAIN_FILE)
                execute_process(
                    COMMAND ${CMAKE_COMMAND}
                        -H${current_dir} -B${build_dir}
                        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
                        -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
                        -G "${generator}"
                        -DOUTPUT_DIR=${config}
                        -DCPPAN_BUILD_SHARED_LIBS=${CPPAN_BUILD_SHARED_LIBS}
                )
            else(CMAKE_TOOLCHAIN_FILE)
                execute_process(
                    COMMAND ${CMAKE_COMMAND}
                        -H${current_dir} -B${build_dir}
                        -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
                        -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
                        -G "${generator}"
                        -DOUTPUT_DIR=${config}
                        -DCPPAN_BUILD_SHARED_LIBS=${CPPAN_BUILD_SHARED_LIBS}
                )
            endif(CMAKE_TOOLCHAIN_FILE)
        endif(EXECUTABLE)

        file(WRITE ${aliases_file} "${aliases}")
        execute_process(
            COMMAND cppan internal-fix-imports ${target} ${aliases_file} ${import} ${import_fixed}
        )
    endif()

    file(LOCK ${lock} RELEASE)
endif()
