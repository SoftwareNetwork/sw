/*
 * Copyright (c) 2016, Egor Pugin
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     1. Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *     2. Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *     3. Neither the name of the copyright holder nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "inserts.h"

const String cmake_functions = R"(################################################################################
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

################################################################################)";
