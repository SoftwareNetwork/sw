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

#pragma once

#if USE_LOGGER

#include <boost/log/trivial.hpp>

#define LOGGER_GLOBAL_INIT
#define LOGGER_GLOBAL_DESTROY
#define LOGGER_CONFIGURE(loglevel, filename) initLogger(loglevel, filename)

#define CREATE_LOGGER(name) const char *name
#define GET_LOGGER(module) module
#define INIT_LOGGER(name, module) name = GET_LOGGER(module)
#define DECLARE_LOGGER(name, module) CREATE_LOGGER(name)(GET_LOGGER(module))
#define DECLARE_STATIC_LOGGER(name, module) static DECLARE_LOGGER(name, module)

//#define LOG(logger, level, message)
#ifdef LOGGER_PRINT_MODULE
#define LOG_BOOST_LOG_MESSAGE(logger, message) "[" << logger << "] " << message
#else
#define LOG_BOOST_LOG_MESSAGE(logger, message) message
#endif
#define LOG_TRACE(logger, message) BOOST_LOG_TRIVIAL(trace) << LOG_BOOST_LOG_MESSAGE(logger, message)
#define LOG_DEBUG(logger, message) BOOST_LOG_TRIVIAL(debug) << LOG_BOOST_LOG_MESSAGE(logger, message)
#define LOG_INFO(logger, message) BOOST_LOG_TRIVIAL(info) << LOG_BOOST_LOG_MESSAGE(logger, message)
#define LOG_WARN(logger, message) BOOST_LOG_TRIVIAL(warning) << LOG_BOOST_LOG_MESSAGE(logger, message)
#define LOG_ERROR(logger, message) BOOST_LOG_TRIVIAL(error) << LOG_BOOST_LOG_MESSAGE(logger, message)
#define LOG_FATAL(logger, message) BOOST_LOG_TRIVIAL(fatal) << LOG_BOOST_LOG_MESSAGE(logger, message)

#define LOG_FLUSH() loggerFlush()

void initLogger(std::string logLevel = "DEBUG", std::string logFile = "", bool simple_logger = false);
void loggerFlush();

#else // !USE_LOGGER

#define LOGGER_GLOBAL_INIT
#define LOGGER_GLOBAL_DESTROY
#define LOGGER_CONFIGURE(loglevel, filename)

#define CREATE_LOGGER(name)
#define GET_LOGGER(module)
#define INIT_LOGGER(name, module)
#define DECLARE_LOGGER(name, module)
#define DECLARE_STATIC_LOGGER(name, module)

//#define LOG(logger, level, message)
#define LOG_TRACE(logger, message)
#define LOG_DEBUG(logger, message)
#define LOG_INFO(logger, message)
#define LOG_WARN(logger, message)
#define LOG_ERROR(logger, message)
#define LOG_FATAL(logger, message)

#define LOG_FLUSH()

#define IS_LOG_TRACE_ENABLED(logger)
#define IS_LOG_DEBUG_ENABLED(logger)
#define IS_LOG_INFO_ENABLED(logger)
#define IS_LOG_WARN_ENABLED(logger)
#define IS_LOG_ERROR_ENABLED(logger)
#define IS_LOG_FATAL_ENABLED(logger)

#endif
