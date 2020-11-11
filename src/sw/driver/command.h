// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include "options.h"

#include <sw/builder/command.h>
#include <sw/builder/file.h>

#include <functional>

namespace sw
{

struct Target;
struct SwManagerContext;
struct RuleData;

namespace cmd
{

struct Prefix
{
    String v;

    explicit Prefix(const String &s)
        : v(s)
    {
    }
};

namespace detail
{

struct tag_path
{
    path p;

    void populate(const path &f) { p = f; }
};

struct tag_files
{
    FilesOrdered files;

    void populate(const path &f) { files.push_back(f); }
    void populate(const Files &f) { files.insert(files.end(), f.begin(), f.end()); }
    void populate(const FilesOrdered &f) { files.insert(files.end(), f.begin(), f.end()); }
};

struct tag_targets
{
    std::vector<Target *> targets;

    void populate(Target &t) { targets.push_back(&t); }
};

// in/out data tags
struct tag_do_not_add_to_targets {};
struct tag_skip {};
struct tag_append {};
struct tag_normalize_path {};

struct tag_files_data
{
    bool add_to_targets = true;
    String prefix;
    bool skip = false; // keep order for ADD macro
    bool normalize = false;

    void populate(tag_normalize_path) { normalize = true; }
    void populate(tag_do_not_add_to_targets) { add_to_targets = false; }
    void populate(tag_skip) { skip = true; }
    void populate(const Prefix &p) { prefix = p.v; }
};

struct tag_io_file : tag_path, tag_files_data
{
    using tag_path::populate;
    using tag_files_data::populate;
};

struct tag_io_files : tag_files, tag_files_data
{
    using tag_files::populate;
    using tag_files_data::populate;
};

struct tag_out_err
{
    bool append = false;
    void populate(tag_append) { append = true; }
};

template <class T, class First, class... Args>
void populate(T &t, const First &arg, Args &&... args)
{
    t.populate(arg);
    if constexpr (sizeof...(Args) > 0)
        populate(t, std::forward<Args>(args)...);
}

template <class T, class... Args>
T in_out(const String &name, Args &&... args)
{
    T t;
    if constexpr (sizeof...(Args) > 0)
        populate(t, std::forward<Args>(args)...);
    if (t.files.empty())
        throw std::logic_error("At least one file must be specified for cmd::" + name);
    return t;
}

} // namespace detail

static detail::tag_do_not_add_to_targets DoNotAddToTargets;
static detail::tag_skip Skip;
static detail::tag_append Append;
static detail::tag_normalize_path NormalizePath;
// TODO: add target list tag, this will simplify lots of code

struct tag_prog_dep { DependencyPtr d; };
struct tag_prog_prog { path p; };
struct tag_prog_rule { const RuleData &rd; };
struct tag_prog_tgt { const ITarget *t; };
struct tag_wdir : detail::tag_path {};
struct tag_in : detail::tag_io_files {};
struct tag_out : detail::tag_io_files {};
struct tag_stdin : detail::tag_io_file {};
struct tag_stdout : detail::tag_io_file, detail::tag_out_err
{
    using tag_io_file::populate;
    using tag_out_err::populate;
};
struct tag_stderr : detail::tag_io_file, detail::tag_out_err
{
    using tag_io_file::populate;
    using tag_out_err::populate;
};
struct tag_env { String k, v; };
struct tag_end {};

struct tag_dep : detail::tag_targets
{
    std::vector<DependencyPtr> target_ptrs;

    void add(const Target &t)
    {
        targets.push_back((Target*)&t);
    }

    void add(const DependencyPtr &t)
    {
        target_ptrs.push_back(t);
    }

    void add_array()
    {
    }

    template <class T, class ... Args>
    void add_array(const T &f, Args && ... args)
    {
        add(f);
        add_array(std::forward<Args>(args)...);
    }
};

inline tag_prog_dep prog(const DependencyPtr &d)
{
    return { d };
}

inline tag_prog_rule prog(const RuleData &rd)
{
    return { rd };
}

inline tag_prog_prog prog(const path &p)
{
    return { p };
}

inline tag_prog_tgt prog(const ITarget &t)
{
    return { &t };
}

inline tag_wdir wdir(const path &file)
{
    return { file };
}

inline tag_end end()
{
    return {};
}

#define ADD(X)                                                                             \
    inline tag_##X X(const path &file, bool add_to_targets)                                \
    {                                                                                      \
        return {FilesOrdered{file}, add_to_targets};                                       \
    }                                                                                      \
                                                                                           \
    inline tag_##X X(const path &file, const String &prefix, bool add_to_targets)          \
    {                                                                                      \
        return {FilesOrdered{file}, add_to_targets, prefix};                               \
    }                                                                                      \
                                                                                           \
    inline tag_##X X(const FilesOrdered &files, bool add_to_targets)                       \
    {                                                                                      \
        return {files, add_to_targets};                                                    \
    }                                                                                      \
                                                                                           \
    inline tag_##X X(const FilesOrdered &files, const String &prefix, bool add_to_targets) \
    {                                                                                      \
        return {files, add_to_targets, prefix};                                            \
    }                                                                                      \
                                                                                           \
    inline tag_##X X(const Files &files, bool add_to_targets)                              \
    {                                                                                      \
        return {FilesOrdered{files.begin(), files.end()}, add_to_targets};                 \
    }                                                                                      \
                                                                                           \
    inline tag_##X X(const Files &files, const String &prefix, bool add_to_targets)        \
    {                                                                                      \
        return {FilesOrdered{files.begin(), files.end()}, add_to_targets, prefix};         \
    }

ADD(in)
ADD(out)

#define ADD2(X)                                                          \
    template <class... Args>                                             \
    tag_##X X(Args &&... args)                                           \
    {                                                                    \
        return detail::in_out<tag_##X>(#X, std::forward<Args>(args)...); \
    }

ADD2(in)
ADD2(out)

#undef ADD
#undef ADD2

inline
tag_stdin std_in(const path &file, bool add_to_targets)
{
    return { file, add_to_targets };
}

template <class ... Args>
tag_stdin std_in(const path &file)
{
    return { file };
}

inline
tag_stdout std_out(const path &file, bool add_to_targets)
{
    return { file, add_to_targets };
}

/*template <class ... Args>
tag_stdout std_out(const path &file, Args && ... args)
{
    std::vector<Target*> targets;
    (targets.push_back(&args), ...);
    return { file, targets };
}*/

template <class... Args>
tag_stdout std_out(Args &&... args)
{
    tag_stdout t;
    detail::populate(t, std::forward<Args>(args)...);
    return t;
}

inline
tag_stderr std_err(const path &file, bool add_to_targets)
{
    return { file, add_to_targets };
}

/*template <class ... Args>
tag_stderr std_err(const path &file, Args && ... args)
{
    std::vector<Target*> targets;
    (targets.push_back(&args), ...);
    return { file, targets };
}*/

template <class... Args>
tag_stderr std_err(Args &&... args)
{
    tag_stderr t;
    detail::populate(t, std::forward<Args>(args)...);
    return t;
}

template <class ... Args>
tag_dep dep(Args && ... args)
{
    tag_dep d;
    d.add_array(std::forward<Args>(args)...);
    return d;
}

inline tag_env env(const String &k, const String &v)
{
    tag_env d;
    d.k = k;
    d.v = v;
    return d;
}

} // namespace cmd

namespace driver
{

struct SW_DRIVER_CPP_API Command : ::sw::builder::Command
{
    using Base = ::sw::builder::Command;
    using LazyCallback = std::function<String(void)>;
    using LazyAction = std::function<void(void)>;

    using Base::Base;
    virtual ~Command() = default;

    virtual std::shared_ptr<Command> clone() const;
    void prepare() override;

    using Base::setProgram;
    void setProgram(const DependencyPtr &);
    void setProgram(const RuleData &r) { rd = &r; }

    // additional dependencies will be used to set up the command
    void addProgramDependency(const DependencyPtr &);

    using Base::operator|;
    Command &operator|(struct CommandBuilder &);

private:
    bool dependency_set = false;
    std::weak_ptr<Dependency> dependency; // main
    std::vector<std::weak_ptr<Dependency>> dependencies; // others
    const RuleData *rd = nullptr;
};

struct SW_DRIVER_CPP_API CommandBuilder
{
    CommandBuilder(Target &, const std::shared_ptr<::sw::builder::Command> & = {});
    CommandBuilder(const CommandBuilder &) = default;
    CommandBuilder &operator=(const CommandBuilder &) = default;

    const CommandBuilder &operator|(const CommandBuilder &) const;
    const CommandBuilder &operator|(::sw::builder::Command &) const;

    Target &getTarget() const;

    auto &getCommand() { return c; }
    const auto &getCommand() const { return c; }

    ::sw::builder::Command *operator->() { return c.get(); }
    const ::sw::builder::Command *operator->() const { return c.get(); }

    //
#define DECLARE_STREAM_OP(t) \
    CommandBuilder &operator<<(const t &)

    DECLARE_STREAM_OP(::sw::cmd::tag_in);
    DECLARE_STREAM_OP(::sw::cmd::tag_out);
    DECLARE_STREAM_OP(::sw::cmd::tag_stdin);
    DECLARE_STREAM_OP(::sw::cmd::tag_stdout);
    DECLARE_STREAM_OP(::sw::cmd::tag_stderr);
    DECLARE_STREAM_OP(::sw::cmd::tag_wdir);
    DECLARE_STREAM_OP(::sw::cmd::tag_end);
    DECLARE_STREAM_OP(::sw::cmd::tag_dep);
    DECLARE_STREAM_OP(::sw::cmd::tag_env);
    DECLARE_STREAM_OP(::sw::cmd::tag_prog_dep);
    DECLARE_STREAM_OP(::sw::cmd::tag_prog_prog);
    DECLARE_STREAM_OP(::sw::cmd::tag_prog_rule);
    DECLARE_STREAM_OP(::sw::cmd::tag_prog_tgt);
    DECLARE_STREAM_OP(Command::LazyCallback);

    template <class T>
    CommandBuilder &operator<<(const T &t)
    {
        auto add_arg = [this](const auto &s)
        {
            if (stopped)
                return;
            c->arguments.push_back(s);
        };

        if constexpr (std::is_same_v<T, path>)
            add_arg(t);
        else if constexpr (std::is_same_v<T, String>)
            add_arg(t);
        else if constexpr (std::is_arithmetic_v<T>)
            add_arg(std::to_string(t));
        else if constexpr (std::is_invocable_v<T>)
            *this << Command::LazyCallback(t);
        else
            // add static assert?
            add_arg(t);
        return *this;
    }

#undef DECLARE_STREAM_OP

private:
    std::shared_ptr<::sw::builder::Command> c;
    bool stopped = false;
    Target *target;

    void setCommand(const std::shared_ptr<::sw::builder::Command> &);
};

} // namespace driver

Version getVersion(
    const SwManagerContext &swctx, builder::detail::ResolvableCommand &c,
    const String &in_regex = {});

Version getVersion(
    const SwManagerContext &swctx, const path &program,
    const String &arg = "--version", const String &in_regex = {});

std::pair<String, Version> getVersionAndOutput(
    const SwManagerContext &swctx, const path &program,
    const String &arg = "--version", const String &in_regex = {});

} // namespace sw
