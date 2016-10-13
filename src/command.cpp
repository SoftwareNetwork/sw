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

#include "command.h"

#include "filesystem.h"

#include <boost/algorithm/string.hpp>
#include <boost/process.hpp>

#include <iostream>
#include <mutex>
#include <thread>

#include "logger.h"
DECLARE_STATIC_LOGGER(logger, "process");

#ifndef _WIN32
#if defined(__APPLE__) && defined(__DYNAMIC__)
extern "C" { extern char ***_NSGetEnviron(void); }
#   define environ (*_NSGetEnviron())
#else
#   include <unistd.h>
#endif
#endif

bool has_executable_in_path(std::string &prog)
{
    bool ret = true;
    try
    {
        prog = boost::process::find_executable_in_path(prog);
    }
    catch (fs::filesystem_error &e)
    {
        LOG_WARN(logger, "'" << prog << "' is missing in your path environment variable. Error: " << e.what());
        ret = false;
    }
    return ret;
}

std::istream &safe_getline(std::istream &is, std::string &s)
{
    s.clear();

    std::istream::sentry se(is, true);
    std::streambuf* sb = is.rdbuf();

    for (;;) {
        int c = sb->sbumpc();
        switch (c) {
        case '\n':
            return is;
        case '\r':
            if (sb->sgetc() == '\n')
                sb->sbumpc();
            return is;
        case EOF:
            if (s.empty())
                is.setstate(std::ios::eofbit | std::ios::failbit);
            return is;
        default:
            s += (char)c;
        }
    }
}

namespace command
{

class stream_reader
{
public:
    stream_reader(boost::process::pistream &in, std::ostream &out, std::string &buffer, Options::Stream opts)
        : in(in), out(out), buffer(buffer), opts(opts)
    {
        t = std::move(std::thread([&t = *this] { t(); }));
    }

    ~stream_reader()
    {
        t.join();
        in.close();
    }

private:
    std::thread t;
    boost::process::pistream &in;
    std::ostream &out;
    std::string &buffer;
    Options::Stream opts;

    void operator()()
    {
        std::string s;
        while (::safe_getline(in, s))
        {
            s += "\n";
            if (opts.capture)
                buffer += s;
            if (opts.inherit)
                out << s;
        }
    }
};

Result execute(const Args &args, const Options &opts)
{
    using namespace boost::process;

    auto args_fixed = args;

#ifdef _WIN32
    if (args_fixed[0].rfind(".exe") != args_fixed[0].size() - 4)
        args_fixed[0] += ".exe";
#endif
    if (!has_executable_in_path(args_fixed[0]))
        throw std::runtime_error("Program '" + args_fixed[0] + "' not found");

#ifdef _WIN32
    boost::algorithm::replace_all(args_fixed[0], "/", "\\");
#endif

#ifndef NDEBUG
    {
        std::string s;
        for (auto &a : args_fixed)
            s += a + " ";
        s.resize(s.size() - 1);
        LOG_DEBUG(logger, "executing command: " << s);
    }
#endif

    context ctx;
    ctx.stdin_behavior = inherit_stream();

    auto set_behavior = [](auto &stream_behavior, auto &opts)
    {
        if (opts.capture)
            stream_behavior = capture_stream();
        else if (opts.inherit)
            stream_behavior = inherit_stream();
        else
            stream_behavior = silence_stream();
    };

    set_behavior(ctx.stdout_behavior, opts.out);
    set_behavior(ctx.stderr_behavior, opts.err);

    // copy env
#ifndef _WIN32
    auto env = environ;
    while (*env)
    {
        std::string s = *env;
        auto p = s.find("=");
        if (p != s.npos)
            ctx.environment[s.substr(0, p)] = s.substr(p + 1);
        env++;
    }
#endif

    Result r;

    try
    {
        child c = boost::process::launch(args_fixed[0], args_fixed, ctx);

        std::unique_ptr<stream_reader> rdout, rderr;

#define RUN_READER(x) \
        if (opts.x.capture) \
            rd ## x = std::make_unique<stream_reader>(c.get_std ## x(), std::c ## x, r.x, opts.x)

        RUN_READER(out);
        RUN_READER(err);

        r.rc = c.wait().exit_status();
    }
    catch (...)
    {
        LOG_FATAL(logger, "Command failed: " << args_fixed[0]);
        throw;
    }

    return r;
}

Result execute_and_capture(const Args &args, const Options &options)
{
    auto opts = options;
    opts.out.capture = true;
    opts.err.capture = true;
    return execute(args, opts);
}

Result execute_with_output(const Args &args, const Options &options)
{
    auto opts = options;
    opts.out.inherit = true;
    opts.err.inherit = true;
    return execute(args, opts);
}

} // namespace command
