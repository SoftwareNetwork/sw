/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
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

#include "command_node.h"
#include "node.h"

#include <primitives/command.h>

#include <condition_variable>
#include <mutex>

namespace sw
{

struct Program;
struct SwBuilderContext;
struct CommandStorage;

struct SW_BUILDER_API ResourcePool
{
    ResourcePool(int n_resources)
    {
        if (n_resources < 1)
            return;
        n = n_resources;
    }

    void lock()
    {
        if (n == -1)
            return;
        std::unique_lock lk(m);
        cv.wait(lk, [this] { return n > 0; });
        --n;
    }

    void unlock()
    {
        if (n == -1)
            return;
        std::unique_lock lk(m);
        ++n;
        lk.unlock();
        cv.notify_one();
    }

private:
    int n = -1; // unlimited
    std::condition_variable cv;
    std::mutex m;
};

namespace builder
{

using ::primitives::command::QuoteType;

namespace detail
{

#pragma warning(push)
#pragma warning(disable:4275) // warning C4275: non dll-interface struct 'primitives::Command' used as base for dll-interface struct 'sw::builder::Command'

struct SW_BUILDER_API ResolvableCommand : ::primitives::Command
{
#pragma warning(pop)

    using ::primitives::Command::push_back;
    path resolveProgram(const path &p) const override;
};

}

struct SW_BUILDER_API Command : ICastable, CommandNode, detail::ResolvableCommand // hide?
{
    using Base = detail::ResolvableCommand;
    using Clock = std::chrono::high_resolution_clock;
    using ImplicitDependenciesProcessor = std::function<Files(Command &)>;

    enum class DepsProcessor
    {
        Undefined,
        Gnu,
        Msvc,
        Custom,
    };

public:
    String name;

    Files inputs;
    // byproducts
    // used only to clean files and pre-create dirs
    //Files intermediate;
    // if some commands accept pairs of args, and specific outputs depend on specific inputs
    // C I1 O1 I2 O2
    // then split that command!
    Files outputs;
    // programs may use (and write to) same file during execution, e.g. pdb files
    // then these files are inputs to other programs, so those programs must wait for all
    // commands that write to such files
    Files simultaneous_outputs;
    //
    Files implicit_inputs;

    // additional create dirs
    Files output_dirs;

    fs::file_time_type mtime = fs::file_time_type::min();
    std::optional<bool> use_response_files;
    int first_response_file_argument = 0;
    bool remove_outputs_before_execution = false; // was true
    bool protect_args_with_quotes = true;
    bool always = false;
    bool do_not_save_command = false;
    bool silent = false; // no log record
    bool show_output = false; // no command output
    bool write_output_to_file = false;
    int strict_order = 0; // used to execute this before other commands
    std::shared_ptr<ResourcePool> pool;

    std::thread::id tid;
    Clock::time_point t_begin;
    Clock::time_point t_end;

    // cs
    path command_storage_root; // used during deserialization to restore command_storage
    CommandStorage *command_storage = nullptr;

    // deps
    DepsProcessor deps_processor = DepsProcessor::Undefined;
    path deps_module; // custom processor
    String deps_function; // custom processor
    path deps_file; // gnu
    String msvc_prefix; // msvc
    //ImplicitDependenciesProcessor implicit_dependencies_processor;

public:
    Command() = default;
    Command(const SwBuilderContext &swctx);
    virtual ~Command();

    void prepare() override;
    //void markForExecution() override;
    void execute() override;
    void execute(std::error_code &ec) override;
    void clean() const;
    bool isExecuted() const { return pid != -1 || executed_.v; }

    String getName() const override;
    size_t getHash() const override;

    virtual bool isOutdated() const;
    bool needsResponseFile() const;
    bool needsResponseFile(size_t sz) const;

    using Base::push_back;
    using Base::setProgram;
    //void setProgram(std::shared_ptr<Program> p);
    void addInput(const path &p);
    void addInput(const Files &p);
    void addImplicitInput(const path &p);
    void addImplicitInput(const Files &p);
    void addOutput(const path &p);
    void addOutput(const Files &p);
    path redirectStdin(const path &p);
    path redirectStdout(const path &p, bool append = false);
    path redirectStderr(const path &p, bool append = false);
    Files getGeneratedDirs() const; // used by generators
    path writeCommand(const path &basename, bool print_name = true) const;

    bool lessDuringExecution(const CommandNode &rhs) const override;

    void onBeforeRun() noexcept override;
    void onEnd() noexcept override;

    path getResponseFilename() const;
    virtual String getResponseFileContents(bool showIncludes = false) const;
    int getFirstResponseFileArgument() const;

    Arguments &getArguments() override;
    const Arguments &getArguments() const override;

    Command &operator|(Command &);
    Command &operator|=(Command &);

    const SwBuilderContext &getContext() const;
    void setContext(const SwBuilderContext &);

protected:
    bool prepared = false;
    template <class T> struct simple_atomic
    {
        T v;

        template <class U>
        simple_atomic(U &&v) : v(v) {}
        simple_atomic(const simple_atomic &rhs) { operator=(rhs); }
        simple_atomic &operator=(const simple_atomic &rhs) { v = rhs.v.load(); return *this; }
        //operator T() const { return v; }
        //operator const T &() const { return v; }
    };
    simple_atomic<std::atomic_bool> executed_{ false };

    virtual bool check_if_file_newer(const path &, const String &what, bool throw_on_missing) const;

private:
    const SwBuilderContext *swctx = nullptr;
    mutable size_t hash = 0;
    Arguments rsp_args;
    mutable String log_string;

    void execute0(std::error_code *ec);
    virtual void execute1(std::error_code *ec = nullptr);
    virtual size_t getHash1() const;

    void postProcess(bool ok = true);
    bool beforeCommand();
    void afterCommand();
    bool isTimeChanged() const;
    void printLog() const;
    size_t getHashAndSave() const;
    String makeErrorString();
    String makeErrorString(const String &e);
    String saveCommand() const;
    void printOutputs();
};

struct SW_BUILDER_API CommandSequence : Command
{
    using Command::Command;

    void addCommand(const std::shared_ptr<Command> &c);

    template <class C = Command, class ... Args>
    std::shared_ptr<C> addCommand(Args && ... args)
    {
        auto c = std::make_shared<C>(getContext(), std::forward<Args>(args)...);
        commands.push_back(c);
        return c;
    }

    const std::vector<std::shared_ptr<Command>> &getCommands() { return commands; }

private:
    std::vector<std::shared_ptr<Command>> commands;

    void execute1(std::error_code *ec = nullptr) override;
    size_t getHash1() const override;
    void prepare() override;
};

// remove? probably no, just don't use it much
// we always can create executable commands that is not builtin into modules
struct SW_BUILDER_API BuiltinCommand : Command
{
    BuiltinCommand();
    BuiltinCommand(const SwBuilderContext &swctx);
    // 3rd parameter is a symbol in module in which our function resides
    BuiltinCommand(const SwBuilderContext &swctx, const String &cmd_name, void *symbol, int version = 0);
    virtual ~BuiltinCommand() = default;

    using Command::push_back;
    void push_back(const Strings &strings);
    void push_back(const Files &files);
    void push_back(const FilesOrdered &files);

private:
    void execute1(std::error_code *ec = nullptr) override;
    size_t getHash1() const override;
    void prepare() override {}

#ifdef BOOST_SERIALIZATION_ACCESS_HPP
    friend class boost::serialization::access;
    template <class Ar>
    void serialize(Ar &ar, unsigned)
    {
        ar & boost::serialization::base_object<Command>(*this);
    }
#endif
};

SW_BUILDER_API
String getInternalCallBuiltinFunctionName();

} // namespace bulder

using builder::BuiltinCommand;
using Commands = std::unordered_set<std::shared_ptr<builder::Command>>;

/// return input when file not found
SW_BUILDER_API
path resolveExecutable(const path &p);

SW_BUILDER_API
path resolveExecutable(const FilesOrdered &paths);

// serialization

// remember to set context and command storage after loading
SW_BUILDER_API
Commands loadCommands(const path &archive_fn, int type = 0);

SW_BUILDER_API
void saveCommands(const path &archive_fn, const Commands &, int type = 0);

} // namespace sw

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

#ifdef BOOST_SERIALIZATION_ACCESS_HPP
// change when you update serialization
//BOOST_CLASS_VERSION(::sw::builder::Command, 4)
#endif
