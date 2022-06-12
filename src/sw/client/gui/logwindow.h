// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2019-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <boost/log/sinks.hpp>
#include <qmainwindow.h>

#include <thread>

namespace sinks = boost::log::sinks;
class qt_text_ostream_backend :
    public QObject,
    public sinks::basic_formatted_sink_backend<
        char,
        sinks::combine_requirements<sinks::synchronized_feeding, sinks::flushing>::type>
{
    Q_OBJECT
public:
    //! Base type
    typedef sinks::basic_formatted_sink_backend<
        char,
        sinks::combine_requirements< sinks::synchronized_feeding, sinks::flushing >::type
    > base_type;

    //! Character type
    typedef typename base_type::char_type char_type;
    //! String type to be used as a message text holder
    typedef typename base_type::string_type string_type;
    //! Output stream type
    typedef std::basic_ostream< char_type > stream_type;

public:
    void auto_flush(bool enable = true) { autoflush = enable; }
    void set_auto_newline_mode(sinks::auto_newline_mode mode) {}
    void consume(boost::log::record_view const &rec, string_type const &formatted_message);
    void flush();

signals:
    void updateText(const QString &);

private:
    bool autoflush = false;
    QString t;
};

class QPlainTextEdit;
struct SwGuiContext;

class LogWindow : public QMainWindow
{
    Q_OBJECT

public:
    std::thread::id tid;
    bool cancelled = false;

    LogWindow(SwGuiContext &swctx, QWidget *parent = 0);

    void appendMessage(const QString &text);
    void stopLogging();
    void stopOperation();

    void closeEvent(QCloseEvent *event) override;

Q_SIGNALS:
    void hideCancelButton();

private:
    using text_sink = boost::log::sinks::synchronous_sink<qt_text_ostream_backend>;

    SwGuiContext &swctx;
    QPlainTextEdit *edit;
    class QPushButton *bc;
    boost::shared_ptr<text_sink> sink;

    void hideCancelButtonSlot();
};
