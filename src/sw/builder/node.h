// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin

#pragma once

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

}
