// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2019-2020 Egor Pugin <egor.pugin@gmail.com>

#include "logwindow.h"

#include "sw_context.h"

#include <primitives/log.h>
#include <primitives/string.h>
#include <primitives/sw/cl.h>
#include <sw/client/common/sw_context.h>
#include <sw/core/sw_context.h>

#include <cl.llvm.h>

#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <boost/log/common.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>

#include <qscrollbar.h>
#include <qstatusbar.h>
#include <qplaintextedit.h>
#include <qboxlayout.h>
#include <qpushbutton.h>

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
    : QMainWindow(parent), swctx(swctx)
{
    auto w = new QWidget;
    auto vl = new QVBoxLayout;
    w->setLayout(vl);
    setCentralWidget(w);

    edit = new QPlainTextEdit();
    edit->setReadOnly(true);
    vl->addWidget(edit);

    bc = new QPushButton("Cancel Operation");
    vl->addWidget(bc);
    connect(bc, &QPushButton::clicked, [this, &swctx]()
    {
        cancelled = true;
        emit hideCancelButton();
        stopOperation();
    });

    connect(this, &LogWindow::hideCancelButton, this, &LogWindow::hideCancelButtonSlot);

    sink = boost::make_shared<text_sink>();
    sink->locked_backend()->auto_flush();
    //sink->locked_backend()->add_stream(w);
    sink->set_formatter(&logFormatterSimple1);

    String level;
    if (swctx.getOptions().trace)
        level = "TRACE";
    else if (swctx.getOptions().verbose)
        level = "DEBUG";
    else
        level = "INFO";

    boost::log::trivial::severity_level sev;
    std::stringstream(boost::algorithm::to_lower_copy(level)) >> sev;
    sink->set_filter(boost::log::trivial::severity >= sev);

    connect(sink->locked_backend().get(), &qt_text_ostream_backend::updateText, this, &LogWindow::appendMessage);
    connect(this, &LogWindow::destroyed, [this]()
    {
        stopOperation();
        stopLogging();
    });

    // Register the sink in the logging core
    boost::log::core::get()->add_sink(sink);
}

void LogWindow::appendMessage(const QString &text)
{
    edit->appendPlainText(text);
    edit->verticalScrollBar()->setValue(edit->verticalScrollBar()->maximum());
}

void LogWindow::stopOperation()
{
    swctx.getContext().stop(tid);
}

void LogWindow::stopLogging()
{
    boost::log::core::get()->remove_sink(sink);
}

void LogWindow::hideCancelButtonSlot()
{
    bc->hide();
}

void LogWindow::closeEvent(QCloseEvent *event)
{
    cancelled = true;
    stopOperation();
    event->accept();
}
