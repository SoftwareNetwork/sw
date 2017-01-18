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

#include "logger.h"

#include <boost/format.hpp>
#include <boost/log/common.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

DECLARE_STATIC_LOGGER(logger, "logger");

BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)
BOOST_LOG_ATTRIBUTE_KEYWORD(thread_id, "ThreadID", boost::log::attributes::current_thread_id::value_type)
BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", boost::log::trivial::severity_level)

typedef boost::log::sinks::text_file_backend tfb;
typedef boost::log::sinks::synchronous_sink<tfb> sfs;

boost::shared_ptr<tfb> backend;
boost::shared_ptr<tfb> backend_debug;
boost::shared_ptr<tfb> backend_trace;

boost::shared_ptr<
    boost::log::sinks::synchronous_sink<
    boost::log::sinks::text_ostream_backend
    >> c_log;

void logFormatter(boost::log::record_view const& rec, boost::log::formatting_ostream& strm)
{
    static boost::thread_specific_ptr<boost::posix_time::time_facet> tss(0);
    if (!tss.get())
    {
        tss.reset(new boost::posix_time::time_facet);
        tss->set_iso_extended_format();
    }
    strm.imbue(std::locale(strm.getloc(), tss.get()));

    std::string s;
    boost::log::formatting_ostream ss(s);
    ss << "[" << rec[severity] << "]";

    strm << "[" << rec[timestamp] << "] " <<
            boost::format("[%08x] %-9s %s")
            % rec[thread_id]
            % s
            % rec[boost::log::expressions::smessage];
}

void logFormatterSimple(boost::log::record_view const& rec, boost::log::formatting_ostream& strm)
{
    strm << boost::format("%s")
        % rec[boost::log::expressions::smessage];
}

void initLogger(LoggerSettings &s)
{
    try
    {
        bool disable_log = s.log_level == "";

        boost::algorithm::to_lower(s.log_level);

        boost::log::trivial::severity_level level;
        std::stringstream(s.log_level) >> level;

        boost::log::trivial::severity_level trace;
        std::stringstream("trace") >> trace;

        if (!disable_log)
        {
            auto c_log = boost::log::add_console_log();
            ::c_log = c_log;
            if (s.simple_logger)
                c_log->set_formatter(&logFormatterSimple);
            else
                c_log->set_formatter(&logFormatter);
            c_log->set_filter(boost::log::trivial::severity >= level);
        }

        if (s.log_file != "")
        {
            auto open_mode = std::ios_base::out;
            if (s.append)
                open_mode = std::ios_base::app;

            // input
            if (level > boost::log::trivial::severity_level::trace)
            {
                auto backend = boost::make_shared<tfb>
                    (
                        boost::log::keywords::file_name = s.log_file + ".log." + s.log_level,
                        boost::log::keywords::rotation_size = 10 * 1024 * 1024,
                        boost::log::keywords::open_mode = open_mode//,
                        //boost::log::keywords::auto_flush = true
                        );
                ::backend = backend;

                auto sink = boost::make_shared<sfs>(backend);
                if (s.simple_logger)
                    sink->set_formatter(&logFormatterSimple);
                else
                    sink->set_formatter(&logFormatter);
                sink->set_filter(boost::log::trivial::severity >= level);
                boost::log::core::get()->add_sink(sink);
            }

            if (level == boost::log::trivial::severity_level::trace || s.print_trace)
            {
                auto add_logger = [&s](auto severity, const auto &name, auto &g_backend)
                {
                    auto backend = boost::make_shared<tfb>
                        (
                            boost::log::keywords::file_name = s.log_file + ".log." + name,
                            boost::log::keywords::rotation_size = 10 * 1024 * 1024,
                            // always append to trace, do not recreate
                            boost::log::keywords::open_mode = std::ios_base::app//,
                                                                                //boost::log::keywords::auto_flush = true
                            );
                    g_backend = backend;

                    auto sink_trace = boost::make_shared<sfs>(backend);
                    // trace to file always has complex format
                    sink_trace->set_formatter(&logFormatter);
                    sink_trace->set_filter(boost::log::trivial::severity >= severity);
                    boost::log::core::get()->add_sink(sink_trace);
                };

                add_logger(boost::log::trivial::severity_level::debug, "debug", ::backend_debug);
                add_logger(boost::log::trivial::severity_level::trace, "trace", ::backend_trace);
            }
        }
        boost::log::add_common_attributes();
    }
    catch (const std::exception &e)
    {
        LOG_ERROR(logger, "logger initialization failed with exception " << e.what() << ", will use default logger settings");
    }
}

void loggerFlush()
{
    if (c_log)
        c_log->flush();
    if (backend)
        backend->flush();
    if (backend_trace)
        backend_trace->flush();
}
