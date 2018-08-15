// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <sw/builder/command.h>

#include "options.h"

#include <functional>

namespace sw
{

struct NativeExecutedTarget;

namespace cmd
{

namespace detail
{

struct tag_path
{
    path p;
};

} // namespace detail

template <class T>
struct tag_prog { T *p; };
struct tag_wdir : detail::tag_path {};
struct tag_in : detail::tag_path { std::vector<NativeExecutedTarget*> targets; };
struct tag_out : detail::tag_path { std::vector<NativeExecutedTarget*> targets; };
struct tag_stdout : detail::tag_path { std::vector<NativeExecutedTarget*> targets; };
struct tag_stderr : detail::tag_path { std::vector<NativeExecutedTarget*> targets; };
struct tag_end {};

template <class T>
tag_prog<T> prog(const T &t)
{
    return { (T*)&t };
}

inline tag_wdir wdir(const path &file)
{
    return { file };
}

inline tag_end end()
{
    return {};
}

template <class ... Args>
tag_in in(const path &file, Args && ... args)
{
    std::vector<NativeExecutedTarget*> targets;
    (targets.push_back(&args), ...);
    return { file, targets };
}

template <class ... Args>
tag_out out(const path &file, Args && ... args)
{
    std::vector<NativeExecutedTarget*> targets;
    (targets.push_back(&args), ...);
    return { file, targets };
}

template <class ... Args>
tag_stdout std_out(const path &file, Args && ... args)
{
    std::vector<NativeExecutedTarget*> targets;
    (targets.push_back(&args), ...);
    return { file, targets };
}

template <class ... Args>
tag_stderr std_err(const path &file, Args && ... args)
{
    std::vector<NativeExecutedTarget*> targets;
    (targets.push_back(&args), ...);
    return { file, targets };
}

} // namespace cmd

namespace driver::cpp
{

struct SW_DRIVER_CPP_API Command : ::sw::builder::Command
{
    using Base = ::sw::builder::Command;
    using LazyCallback = std::function<String(void)>;

    std::shared_ptr<Dependency> dependency;

    path getProgram() const override;
    void prepare() override;

    using Base::setProgram;
    void setProgram(const std::shared_ptr<Dependency> &d);
    void setProgram(const NativeTarget &t);

    void pushLazyArg(LazyCallback f);

private:
    std::map<int, LazyCallback> callbacks;
};

struct CommandBuilder
{
    std::shared_ptr<Command> c;
    std::vector<NativeExecutedTarget*> targets;
    bool stopped = false;
};

#define DECLARE_STREAM_OP(t) \
    SW_DRIVER_CPP_API        \
    CommandBuilder &operator<<(CommandBuilder &, const t &)

DECLARE_STREAM_OP(NativeExecutedTarget);
DECLARE_STREAM_OP(::sw::cmd::tag_in);
DECLARE_STREAM_OP(::sw::cmd::tag_out);
DECLARE_STREAM_OP(::sw::cmd::tag_stdout);
DECLARE_STREAM_OP(::sw::cmd::tag_stderr);
DECLARE_STREAM_OP(::sw::cmd::tag_wdir);
DECLARE_STREAM_OP(::sw::cmd::tag_end);
DECLARE_STREAM_OP(Command::LazyCallback);

template <class T>
CommandBuilder operator<<(std::shared_ptr<Command> &c, const T &t)
{
    CommandBuilder cb;
    cb.c = c;
    cb << t;
    return cb;
}

template <class T>
CommandBuilder &operator<<(CommandBuilder &cb, const cmd::tag_prog<T> &t)
{
    for (auto tgt : cb.targets)
    {
        auto d = *tgt + *t.p;
        d->Dummy = true;
    }
    cb.c->setProgram(*t.p);
    return cb;
}

template <class T>
CommandBuilder &operator<<(CommandBuilder &cb, const T &t)
{
    if constexpr (std::is_same_v<T, path>)
    {
        if (!cb.stopped)
            cb.c->args.push_back(t.u8string());
    }
    else if constexpr (std::is_same_v<T, String>)
    {
        if (!cb.stopped)
            cb.c->args.push_back(t);
    }
    else if constexpr (std::is_arithmetic_v<T>)
    {
        if (!cb.stopped)
            cb.c->args.push_back(std::to_string(t));
    }
    else if constexpr (std::is_base_of_v<NativeExecutedTarget, T>)
    {
        return cb << (const NativeExecutedTarget&)t;
    }
    else
    {
        // add static assert?
        if (!cb.stopped)
            cb.c->args.push_back(t);
    }
    return cb;
}

#undef DECLARE_STREAM_OP

} // namespace driver::cpp

namespace cmd
{

inline std::shared_ptr<::sw::driver::cpp::Command> command()
{
    return std::make_shared<::sw::driver::cpp::Command>();
}

} // namespace cmd

} // namespace sw
