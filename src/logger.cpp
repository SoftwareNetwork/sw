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

void initLogger(std::string logLevel, std::string logFile, bool simple_logger)
{
    try
    {
        bool disable_log = logLevel == "";

        boost::algorithm::to_lower(logLevel);

        boost::log::trivial::severity_level level;
        std::stringstream(logLevel) >> level;

        std::string logLevelTrace = "trace";
        boost::log::trivial::severity_level trace;
        std::stringstream(logLevelTrace) >> trace;

        if (!disable_log)
        {
            auto c_log = boost::log::add_console_log();
            if (simple_logger)
                c_log->set_formatter(&logFormatterSimple);
            else
                c_log->set_formatter(&logFormatter);
            c_log->set_filter(boost::log::trivial::severity >= level);
        }

        if (logFile != "")
        {
            typedef boost::log::sinks::text_file_backend tfb;
            typedef boost::log::sinks::synchronous_sink<tfb> sfs;

            auto backend = boost::make_shared<tfb>
            (
                boost::log::keywords::file_name = logFile + ".log." + logLevel,
                boost::log::keywords::rotation_size = 10 * 1024 * 1024,
                //boost::log::keywords::open_mode = std::ios_base::app,
                boost::log::keywords::auto_flush = true
            );
            auto sink = boost::make_shared<sfs>(backend);
            if (simple_logger)
                sink->set_formatter(&logFormatterSimple);
            else
                sink->set_formatter(&logFormatter);
            sink->set_filter(boost::log::trivial::severity >= level);
            boost::log::core::get()->add_sink(sink);

#ifndef NDEBUG
            auto backend_trace = boost::make_shared<tfb>
            (
                boost::log::keywords::file_name = logFile + ".log." + logLevelTrace,
                boost::log::keywords::rotation_size = 10 * 1024 * 1024,
                //boost::log::keywords::open_mode = std::ios_base::app,
                boost::log::keywords::auto_flush = true
            );
            auto sink_trace = boost::make_shared<sfs>(backend_trace);
            if (simple_logger)
                sink_trace->set_formatter(&logFormatterSimple);
            else
                sink_trace->set_formatter(&logFormatter);
            sink_trace->set_filter(boost::log::trivial::severity >= trace);
            boost::log::core::get()->add_sink(sink_trace);
#endif
        }
        boost::log::add_common_attributes();
    }
    catch (const std::exception &e)
    {
        LOG_ERROR(logger, "logger initialization failed with exception " << e.what() << ", will use default logger settings");
    }
}
