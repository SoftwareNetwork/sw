#include "command.h"

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

namespace sw
{

template <class A>
Commands loadCommands(A &);

extern template Commands loadCommands<boost::archive::binary_iarchive>(boost::archive::binary_iarchive &);
extern template Commands loadCommands<boost::archive::text_iarchive>(boost::archive::text_iarchive &);

using SimpleCommands = std::vector<builder::Command *>;

template <class A>
void saveCommands(A &ar, const SimpleCommands &);

extern template void saveCommands<boost::archive::binary_oarchive>(boost::archive::binary_oarchive &, const SimpleCommands &);
extern template void saveCommands<boost::archive::text_oarchive>(boost::archive::text_oarchive &, const SimpleCommands &);

template <class A, class T>
void saveCommands(A &ar, const T &cmds)
{
    SimpleCommands commands;
    commands.reserve(cmds.size());
    for (auto &c : cmds)
        commands.push_back(c.get());
    saveCommands(ar, commands);
}

}
