// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <memory>
#include <type_traits>

namespace sw
{

/// provides castable interface (as() methods)
struct SW_BUILDER_API ICastable
{
    virtual ~ICastable() = 0;

    template <class T, typename = std::enable_if_t<std::is_pointer_v<T>>>
    std::decay_t<std::remove_pointer_t<T>> *as()
    {
        return dynamic_cast<std::decay_t<std::remove_pointer_t<T>> *>(this);
    }

    template <class T, typename = std::enable_if_t<std::is_pointer_v<T>>>
    const std::decay_t<std::remove_pointer_t<T>> *as() const
    {
        return dynamic_cast<const std::decay_t<std::remove_pointer_t<T>> *>(this);
    }

    template <class T, typename = std::enable_if_t<!std::is_pointer_v<T>>>
    std::decay_t<T> &as()
    {
        return dynamic_cast<std::decay_t<T> &>(*this);
    }

    template <class T, typename = std::enable_if_t<!std::is_pointer_v<T>>>
    const std::decay_t<T> &as() const
    {
        return dynamic_cast<const std::decay_t<T> &>(*this);
    }
};

namespace builder { struct Command; }

namespace detail
{

struct SW_BUILDER_API Executable
{
    virtual ~Executable() = default;

    virtual std::shared_ptr<builder::Command> getCommand() const = 0;
    virtual void execute() const;
};

}

}
