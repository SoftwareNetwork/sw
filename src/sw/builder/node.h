// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <memory>

namespace sw
{

namespace builder
{

struct Command;

}

struct SW_BUILDER_API Node
{
    virtual ~Node() = default;

    template <class T>
    T *as()
    {
        return dynamic_cast<T *>(this);
    }

    template <class T>
    const T *as() const
    {
        return dynamic_cast<const T *>(this);
    }

    template <class T>
    T &asRef()
    {
        return dynamic_cast<T &>(*this);
    }

    template <class T>
    const T &asRef() const
    {
        return dynamic_cast<const T &>(*this);
    }
};

namespace detail
{

struct SW_BUILDER_API Executable : virtual Node
{
    virtual ~Executable() = default;

    virtual std::shared_ptr<builder::Command> getCommand() const = 0;
    virtual void execute() const;
};

}

}
