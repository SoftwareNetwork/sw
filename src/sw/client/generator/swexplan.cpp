/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2019 Egor Pugin
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

#include "generator.h"

#include <sw/builder/file.h>
#include <sw/builder/execution_plan.h>
#include <sw/core/build.h>
#include <sw/core/sw_context.h>
#include <sw/support/filesystem.h>

#include <boost/serialization/access.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4005) // warning C4005: 'XXX': macro redefinition
#endif

////////////////////////////////////////////////////////////////////////////////

#define SERIALIZATION_BEGIN_LOAD(t) \
    template <class Archive>        \
    void load(Archive &ar, t &v, const unsigned int)

#define SERIALIZATION_BEGIN_SAVE(t) \
    template <class Archive>        \
    void save(Archive &ar, const t &v, const unsigned int)

#define SERIALIZATION_BEGIN_SPLIT                      \
    BOOST_SERIALIZATION_SPLIT_FREE(SERIALIZATION_TYPE) \
    namespace boost::serialization                     \
    {                                                  \
    SERIALIZATION_BEGIN_LOAD(SERIALIZATION_TYPE)       \
    {

#define SERIALIZATION_END }
#define SERIALIZATION_SPLIT_CONTINUE } SERIALIZATION_BEGIN_SAVE(SERIALIZATION_TYPE) {
#define SERIALIZATION_SPLIT_END } SERIALIZATION_END

////////////////////////////////////////////////////////////////////////////////

#define SERIALIZATION_TYPE ::path
SERIALIZATION_BEGIN_SPLIT
    String s;
    ar & s;
    v = fs::u8path(s);
SERIALIZATION_SPLIT_CONTINUE
    ar & v.u8string();
SERIALIZATION_SPLIT_END

#define SERIALIZATION_TYPE StringMap<String>
SERIALIZATION_BEGIN_SPLIT
    size_t sz;
    ar & sz;
    while (sz--)
    {
        String k, v;
        ar & k & v;
        p[k] = v;
    }
SERIALIZATION_SPLIT_CONTINUE
    ar & v.size();
    for (auto &[k, va] : v)
        ar & k & va;
SERIALIZATION_SPLIT_END

namespace boost::serialization
{

template<class Archive>
void serialize(Archive &ar, ::sw::builder::Command::Argument &a, const unsigned int)
{
    ar & a.toString();
}

template<class Archive>
void serialize(Archive &ar, ::sw::builder::Command &c, const unsigned int)
{
    ar & c.working_directory;
    ar & c.environment;
    for (auto &a : c.getArguments())
        ar & *a;
}

} // namespace boost::serialization

#define SERIALIZATION_TYPE ::sw::ExecutionPlan
SERIALIZATION_BEGIN_SPLIT
    SW_UNIMPLEMENTED;
SERIALIZATION_SPLIT_CONTINUE
    for (auto &c : v.getCommands<::sw::builder::Command>())
        ar & *c;
SERIALIZATION_SPLIT_END

void SwExecutionPlan::generate(const sw::SwBuild &b)
{
    const auto d = path(SW_BINARY_DIR) / toPathString(type) / b.getHash();
    fs::create_directories(d);

    // create and open an archive for input
    std::ofstream ofs(d / "1.txt");
    boost::archive::text_oarchive oa(ofs);

    auto ep = b.getExecutionPlan();
    oa & ep;
}
