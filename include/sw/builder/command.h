// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <primitives/command.h>
#include <primitives/executor.h>

#include <condition_variable>
#include <mutex>

namespace sw
{

struct Program;

template <class T>
struct CommandData
{
    std::unordered_set<std::shared_ptr<T>> dependencies;

    std::atomic_size_t dependencies_left = 0;
    std::unordered_set<std::shared_ptr<T>> dependendent_commands;

    size_t *current_command = nullptr;
    size_t total_commands = 0;

    virtual ~CommandData() = default;

    virtual void execute() = 0;
    virtual void prepare() = 0;
};

namespace builder
{

struct Command;

}

using Commands = std::unordered_set<std::shared_ptr<builder::Command>>;

struct SW_BUILDER_API ResourcePool
{
    int n = -1; // unlimited
    std::condition_variable cv;
    std::mutex m;

    void lock()
    {
        if (n == -1)
            return;
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, [this] { return n > 0; });
        --n;
    }

    void unlock()
    {
        if (n == -1)
            return;
        std::unique_lock<std::mutex> lk(m);
        ++n;
        lk.unlock();
        cv.notify_one();
    }
};

namespace builder
{

#pragma warning(push)
#pragma warning(disable:4275) // warning C4275: non dll-interface struct 'primitives::Command' used as base for dll-interface struct 'sw::builder::Command'

struct SW_BUILDER_API Command : std::enable_shared_from_this<Command>,
    CommandData<Command>, primitives::Command // hide?
{
#pragma warning(pop)
    using Base = primitives::Command;

    String name;
    String name_short;
    Files inputs;
    Files intermediate;
    Files outputs;
    bool use_response_files = true;
    bool remove_outputs_before_execution = true;
    const Program *base = nullptr; // TODO: hide
    //std::shared_ptr<Dependency> dependency; // TODO: hide
    bool silent = false;
    bool always = false;
    bool executed = false;

    virtual ~Command() = default;

    void prepare() override;
    void execute() override { execute1(); }
    void execute(std::error_code &ec) override { execute1(&ec); }
    virtual void postProcess(bool ok = true) {}
    void clean() const;

    String getName(bool short_name = false) const;
    void printLog() const;
    virtual path getProgram() const;
    virtual ResourcePool *getResourcePool() { return nullptr; }

    virtual bool isOutdated() const;
    bool needsResponseFile() const;

    void setProgram(const path &p);
    //void setProgram(const std::shared_ptr<Dependency> &d);
    void setProgram(const Program &p);
    //void setProgram(const NativeTarget &t);
    void addInput(const path &p);
    void addInput(const Files &p);
    void addIntermediate(const path &p);
    void addIntermediate(const Files &p);
    void addOutput(const path &p);
    void addOutput(const Files &p);
    path redirectStdout(const path &p);
    path redirectStderr(const path &p);
    virtual bool isHashable() const { return true; }
    size_t getHash() const;
    size_t calculateFilesHash() const;
    void updateFilesHash() const;

protected:
    bool prepared = false;

    void addInputOutputDeps();

private:
    mutable size_t hash = 0;

    void execute1(std::error_code *ec = nullptr);
};

}

template struct SW_BUILDER_API CommandData<builder::Command>;

/*struct SW_BUILDER_API DummyCommand : sw::builder::Command
{
    void prepare() override {}
    void execute() override {}
};*/

struct ExecuteCommand : builder::Command
{
    using F = std::function<void(void)>;

    F f;
    bool always = false;

    template <class F2>
    ExecuteCommand(F2 &&f) : f(f) {}

    template <class F2>
    ExecuteCommand(bool always, F2 &&f) : f(f), always(true) {}

    template <class F2>
    ExecuteCommand(const path &p, F2 &&f) : f(f)
    {
        addOutput(p);
    }

    template <class F2>
    ExecuteCommand(const Files &fs, F2 &&f) : f(f)
    {
        for (auto &p : fs)
            addOutput(p);
    }

    SW_BUILDER_API
    virtual ~ExecuteCommand() = default;

    SW_BUILDER_API
    bool isOutdated() const override;

    SW_BUILDER_API
    void execute() override;

    SW_BUILDER_API
    void prepare() override;

    SW_BUILDER_API
    bool isHashable() const override { return false; }

    SW_BUILDER_API
    path getProgram() const override { return "ExecuteCommand"; };
};

}

namespace std
{

template<> struct hash<sw::builder::Command>
{
    size_t operator()(const sw::builder::Command &c) const
    {
        return c.getHash();
    }
};

}
