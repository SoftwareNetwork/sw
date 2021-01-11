// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin

#include "command_node.h"

#include <primitives/exceptions.h>

namespace sw
{

void CommandNode::addDependency(CommandNode &c)
{
    if (&c == this)
        throw SW_LOGIC_ERROR("Trying to add self dependency");
    dependencies.insert(&c);
}

/*void CommandNode::addDependency(const std::shared_ptr<CommandNode> &c)
{
    addDependency(*c);
}*/

} // namespace sw
