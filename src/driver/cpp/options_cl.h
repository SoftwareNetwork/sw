// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <sw/builder/command.h>
#include "types.h"

#include <unordered_map>

#define DECLARE_OPTION_SPECIALIZATION(t) \
    template <>                          \
    SW_DRIVER_CPP_API                    \
    Strings CommandLineOption<t>::getCommandLineImpl(builder::Command *c) const

#define DEFINE_OPTION_SPECIALIZATION_DUMMY(t) \
    template <>                               \
    Strings CommandLineOption<t>::getCommandLineImpl(builder::Command *c) const { return {}; }

#define COMMAND_LINE_OPTION(n, t) CommandLineOption<t> n

namespace sw
{

namespace builder
{

struct Command;

}

template <class T>
struct CommandLineOption;

template <class T>
using CommandLineFunctionType = Strings(*)(const CommandLineOption<T> &, builder::Command *);

struct CommandLineOptionBase
{
    virtual ~CommandLineOptionBase() = default;

    virtual Strings getCommandLine(builder::Command *c = nullptr) const = 0;
};

// Command Option tag initializers.
namespace cl
{

/**
* \brief Base tag class.
*/
struct CommandLineOptionBaseValue {};

namespace detail
{

struct StringOption : CommandLineOptionBaseValue, String
{
    StringOption(const String &s)
        : String(s)
    {
    }
};

}

#define STRING_OPTION(n) \
    struct n : detail::StringOption { using detail::StringOption::StringOption; }

/**
* \brief Option name.
*/
STRING_OPTION(Name);

/**
* \brief Command line flag itself.
*/
STRING_OPTION(CommandFlag);

/**
* \brief IDE name of this option.
*/
STRING_OPTION(IDEName);

/**
* \brief Optional comment.
*/
STRING_OPTION(Comment);

/**
* \brief Prefix command option.
*/
STRING_OPTION(Prefix);

/**
 * \brief Changing this variable produces new config.
 */
struct ConfigVariable : CommandLineOptionBaseValue {};

/**
* \brief Prepend command flag before each value in array.
*/
struct CommandFlagBeforeEachValue : CommandLineOptionBaseValue {};

/**
* \brief Adds this file parameter as an input dependency to command.
*/
struct InputDependency : CommandLineOptionBaseValue {};

/**
* \brief File that is created during command but won't be used by anyone.
*/
struct IntermediateFile : CommandLineOptionBaseValue {};

/**
* \brief Adds this file parameter as an output dependency to command.
*/
struct OutputDependency : CommandLineOptionBaseValue {};

/**
* \brief Such options manually handled and skipped during print.
*/
struct ManualHandling : CommandLineOptionBaseValue {};

/**
* \brief Such options will be printed at the end of the command line.
*/
struct PlaceAtTheEnd : CommandLineOptionBaseValue {};

/**
* \brief Skip this option.
*/
struct Skip : CommandLineOptionBaseValue {};

/**
* \brief Separate prefix from argument.
*/
struct SeparatePrefix : CommandLineOptionBaseValue {};

/**
* \brief Adds this file parameter as an output dependency to command.
*/
template <class T>
struct CommandLineFunction : CommandLineOptionBaseValue
{
    CommandLineFunctionType<T> F;

    CommandLineFunction(CommandLineFunctionType<T> F) : F(F) {}
};

// NoCommandFlag

//template <class T> struct Value : CommandLineOptionBaseValue { T v; Value(const T &v) : v(v) {} };

}

// make sure we have same size on all CommandLineOption objects
// to be able to iterate over them, so keep T types under pointers
template <class T>
struct CommandLineOption1 : CommandLineOptionBase
{
    String name;
protected:
    std::unique_ptr<T> value_;
private:
    String cmd_flag;
public:
    String ide_name;
    String comment;
    //String prefix;
private:
    CommandLineFunctionType<T> function = nullptr;
public:
    unsigned config_variable : 1;
    unsigned cmd_flag_before_each_value : 1;
    unsigned input_dependency : 1;
    unsigned intermediate_file : 1;
    unsigned output_dependency : 1;
    unsigned manual_handling : 1;
    unsigned place_at_the_end : 1;
    unsigned skip : 1;

    unsigned separate_prefix : 1;
    unsigned create_directory : 1;
    unsigned : 6;

    CommandLineOption1()
    {
        init_fields();
    }

    template <class U, class ... Args>
    explicit CommandLineOption1(U &&u, Args && ... args)
        : CommandLineOption1(std::forward<Args>(args)...)
    {
        static_assert(std::is_base_of_v<cl::CommandLineOptionBaseValue, U> ||
            std::is_same_v<T, U> || std::is_same_v<CommandLineOption1, U>,
            "Use one of predefined command line option types.");

        init(std::forward<U>(u));
    }

    explicit CommandLineOption1(const T &v)
    {
        init_fields();
        assign_value(v);
    }

    explicit CommandLineOption1(const CommandLineOption1 &v)
    {
        init_fields();
        assign(v);
    }

    /*template <class U>
    CommandLineOption1(U &&v)
    {
        assign(v);
    }*/

    CommandLineOption1 &operator=(const CommandLineOption1 &rhs)
    {
        assign(rhs);
        return *this;
    }

    /*template <class U>
    CommandLineOption1 &operator=(U &&rhs)
    {
        assign_value(rhs);
        return *this;
    }*/

    CommandLineOption1 &operator=(const T &rhs)
    {
        assign_value(rhs);
        return *this;
    }

    virtual Strings getCommandLine(builder::Command *c = nullptr) const
    {
        if (value_)
        {
            if (function)
                return (*function)((const CommandLineOption<T> &)*this, c);
            return getCommandLineImpl(c);
            //return Strings{};
        }
        return Strings{};
    }

    bool empty() const { return !value_; }

    T &value()
    {
        if (!value_)
            init_value();
        return *value_;
    }

    const T &value() const
    {
        if (!value_)
            // if we do this, we might expect bad behavior
            //const_cast<CommandLineOption1*>(this)->init_value();
            throw std::logic_error("Calling const object without allocated value");
        return *value_;
    }

    T &operator()()
    {
        return value();
    }

    const T &operator()() const
    {
        return value();
    }

    operator bool() const
    {
        return !!value_;
    }

    String getCommandLineFlag() const
    {
        if (cmd_flag.empty())
            return "";
        return "-"s + cmd_flag;
    }

    void clear()
    {
        value_.reset();
    }

protected:
    virtual Strings getCommandLineImpl(builder::Command *c = nullptr) const { return {}; }

private:
    //void init(const cl::Value<T> &v) { assign_value(v.v); }
    void init(const cl::Name &v) { name = v; }
    void init(const cl::Comment &v) { comment = v; }
    void init(const cl::IDEName &v) { ide_name = v; }
    //void init(const cl::Prefix &v) { prefix = v; }
    void init(const cl::CommandLineFunction<T> &v) { function = v.F; }
    void init(const cl::CommandFlag &v) { cmd_flag = v; }
    void init(const cl::ConfigVariable &) { config_variable = true; }
    void init(const cl::CommandFlagBeforeEachValue &) { cmd_flag_before_each_value = true; }
    void init(const cl::InputDependency &) { input_dependency = true; }
    void init(const cl::IntermediateFile &) { intermediate_file = true; }
    void init(const cl::OutputDependency &) { output_dependency = true; }
    void init(const cl::ManualHandling &) { manual_handling = true; }
    void init(const cl::PlaceAtTheEnd &) { place_at_the_end = true; }
    void init(const cl::Skip &) { skip = true; }
    void init(const cl::SeparatePrefix &) { separate_prefix = true; }
    void init(const T &v) { assign_value(v); }
    void init(const CommandLineOption1 &v) { assign(v); }

    void init_fields()
    {
        config_variable = false;
        cmd_flag_before_each_value = false;
        input_dependency = false;
        intermediate_file = false;
        output_dependency = false;
        manual_handling = false;
        place_at_the_end = false;
        skip = false;
        separate_prefix = false;
        create_directory = false;
    }

    void assign(const CommandLineOption1 &v)
    {
        assign_value(v.value_);
        cmd_flag = v.cmd_flag;
        ide_name = v.ide_name;
        comment = v.comment;
        name = v.name;
        config_variable = v.config_variable;
        cmd_flag_before_each_value = v.cmd_flag_before_each_value;
        input_dependency = v.input_dependency;
        intermediate_file = v.intermediate_file;
        output_dependency = v.output_dependency;
        manual_handling = v.manual_handling;
        place_at_the_end = v.place_at_the_end;
        skip = v.skip;
        separate_prefix = v.separate_prefix;
        function = v.function;
    }

    void assign_value(const std::unique_ptr<T> &rhs)
    {
        if (rhs)
            assign_value(*rhs);
        else
            value_.reset();
    }

    void assign_value(const T &rhs)
    {
        if (value_)
            *value_ = rhs;
        else
            value_ = std::make_unique<T>(rhs);
    }

    template <class ... Args>
    void init_value(Args && ... args)
    {
        value_ = std::make_unique<T>(std::forward<Args>(args)...);
    }
};

template <class T>
struct CommandLineOption : CommandLineOption1<T>
{
    using CommandLineOption1<T>::CommandLineOption1;
    using CommandLineOption1<T>::operator=;

    template<typename U = T>
    operator typename std::enable_if_t<std::is_same_v<U, bool>, bool>() const
    {
        return this->value_ && *this->value_;
    }

    template<typename U = T>
    operator typename std::enable_if_t<!std::is_same_v<U, bool>, bool>() const
    {
        return !!this->value_;
    }

    template<typename U = T>
    operator typename std::enable_if_t<!std::is_same_v<U, bool>, U>() const
    {
        if (!this->value_)
            throw SW_RUNTIME_ERROR("Option value is not set");
        return *this->value_;
    }

private:
    virtual Strings getCommandLineImpl(builder::Command *c = nullptr) const override;
};

struct CommandLineOptionsBegin
{
    CommandLineOption1<bool> __compiler_options_begin;
};

struct CommandLineOptionsEnd
{
    CommandLineOption1<bool> __compiler_options_end;
};

#pragma warning(disable: 4584)
template <class ... Types>
struct CommandLineOptions : private CommandLineOptionsBegin,
    Types...,
    private CommandLineOptionsEnd
{
#pragma warning(default: 4584)

    // add manual iterator?
    using iterator = CommandLineOption1<bool>*;
    using const_iterator = const CommandLineOption1<bool>*;

    iterator begin() { return (iterator)&this->CommandLineOptionsBegin::__compiler_options_begin + 1; }
    iterator end() { return (iterator)&this->CommandLineOptionsEnd::__compiler_options_end; }

    const_iterator begin() const { return (const_iterator)&this->CommandLineOptionsBegin::__compiler_options_begin + 1; }
    const_iterator end() const { return (const_iterator)&this->CommandLineOptionsEnd::__compiler_options_end; }
};

DECLARE_OPTION_SPECIALIZATION(bool);
DECLARE_OPTION_SPECIALIZATION(String);
DECLARE_OPTION_SPECIALIZATION(StringMap<String>);
DECLARE_OPTION_SPECIALIZATION(path);
DECLARE_OPTION_SPECIALIZATION(FilesOrdered);
DECLARE_OPTION_SPECIALIZATION(Files);
DECLARE_OPTION_SPECIALIZATION(std::set<int>);

}
