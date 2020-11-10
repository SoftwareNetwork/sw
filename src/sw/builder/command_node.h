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
