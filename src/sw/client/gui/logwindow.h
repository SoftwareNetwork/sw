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

#pragma once

#include <boost/log/sinks.hpp>
#include <qmainwindow.h>

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
