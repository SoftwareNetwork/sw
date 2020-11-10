// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>

#pragma once

#include <primitives/filesystem.h>

#include <tuple>

namespace sw
{

namespace builder { struct Command; }

struct RuleFile
{
    //using AdditionalArguments = std::map<String, primitives::command::Arguments>;
    using AdditionalArguments = std::map<String, Strings>;

    RuleFile(const path &fn) : file(fn) {}

    AdditionalArguments &getAdditionalArguments() { return additional_arguments; }
    const AdditionalArguments &getAdditionalArguments() const { return additional_arguments; }

    bool operator<(const RuleFile &rhs) const { return std::tie(file, additional_arguments) < std::tie(rhs.file, rhs.additional_arguments); }
    bool operator==(const RuleFile &rhs) const { return std::tie(file, additional_arguments) == std::tie(rhs.file, rhs.additional_arguments); }
    //auto operator<=>(const RuleFile &rhs) const = default;

    const path &getFile() const { return file; }

    /*size_t getHash() const
    {
        size_t h = 0;
        hash_combine(h, std::hash<decltype(file)>()(file));
        hash_combine(h, std::hash<decltype(additional_arguments)>()(additional_arguments));
        return h;
    }*/

private:
    path file;
    AdditionalArguments additional_arguments;
public:
    std::shared_ptr<builder::Command> command;
    std::unordered_set<builder::Command*> dependencies;
};

} // namespace sw

/*namespace std
{

template<> struct hash<::sw::RuleFile>
{
    size_t operator()(const ::sw::RuleFile &f) const
    {
        return f.getHash();
    }
};

}*/

namespace sw
{

using RuleFiles = std::unordered_map<path, RuleFile>;

} // namespace sw
