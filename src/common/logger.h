/*
 * Copyright (C) 2016-2017, Egor Pugin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
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

struct LoggerSettings
{
    std::string log_level = "DEBUG";
    std::string log_file;
    bool simple_logger = false;
    bool print_trace = false;
    bool append = false;
};

void initLogger(LoggerSettings &s);
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
