// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin

#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <unordered_set>

namespace sw
{

struct SW_BUILDER_API CommandNode : std::enable_shared_from_this<CommandNode>
{
    using Ptr = CommandNode*;
    using USet = std::unordered_set<Ptr>;

private:
    USet dependencies;

public:
    std::atomic_size_t dependencies_left = 0;
    USet dependent_commands;

    std::atomic_size_t *current_command = nullptr;
    std::atomic_size_t *total_commands = nullptr;

    CommandNode();
    CommandNode(const CommandNode &);
    CommandNode &operator=(const CommandNode &);
    virtual ~CommandNode();

    virtual std::string getName() const = 0;
    virtual size_t getHash() const = 0;
    virtual void execute() = 0;
    virtual void prepare() = 0; // some internal preparations, command may not be executed still
    //virtual void markForExecution() {} // not command can be sure, it will be executed
    virtual bool lessDuringExecution(const CommandNode &) const = 0;

    void addDependency(CommandNode &);
    //void addDependency(const std::shared_ptr<CommandNode> &);
    //USet &getDependencies() { return dependencies; }
    const USet &getDependencies() const { return dependencies; }
    void clearDependencies() { dependencies.clear(); }
};

} // namespace sw
