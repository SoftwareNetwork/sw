/*
 * SW - Build System and Package Manager
 * Copyright (C) 2019-2020 Egor Pugin
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "logwindow.h"

#include "sw_context.h"

#include <primitives/log.h>
#include <primitives/string.h>
#include <primitives/sw/cl.h>
#include <sw/client/common/sw_context.h>

#include <cl.llvm.h>

#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/log/common.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>

#include <qscrollbar.h>

BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", boost::log::trivial::severity_level)
void logFormatterSimple1(boost::log::record_view const& rec, boost::log::formatting_ostream& strm)
{
    strm << boost::format("%s")
        % rec[boost::log::expressions::smessage];
}

void qt_text_ostream_backend::consume(boost::log::record_view const &rec, string_type const &formatted_message)
{
    t = t + formatted_message.c_str();
    if (autoflush)
        flush();
}

void qt_text_ostream_backend::flush()
{
    emit updateText(t);
    t.clear();
}

LogWindow::LogWindow(SwGuiContext &swctx, QWidget *parent)
    : QPlainTextEdit(parent), swctx(swctx)
{
    setReadOnly(true);

    sink = boost::make_shared<text_sink>();
    sink->locked_backend()->auto_flush();
    //sink->locked_backend()->add_stream(w);
    sink->set_formatter(&logFormatterSimple1);

    String level;
    if (swctx.getOptions().trace)
        level = "TRACE";
    else if (gVerbose)
        level = "DEBUG";
    else
        level = "INFO";

    boost::log::trivial::severity_level sev;
    std::stringstream(boost::algorithm::to_lower_copy(level)) >> sev;
    sink->set_filter(boost::log::trivial::severity >= sev);

    connect(sink->locked_backend().get(), &qt_text_ostream_backend::updateText, this, &LogWindow::appendMessage);
    connect(this, &LogWindow::close, [this]()
    {
        stop();
    });

    // Register the sink in the logging core
    boost::log::core::get()->add_sink(sink);
}

void LogWindow::appendMessage(const QString &text)
{
    appendPlainText(text);
    verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

void LogWindow::stop()
{
    boost::log::core::get()->remove_sink(sink);
}
