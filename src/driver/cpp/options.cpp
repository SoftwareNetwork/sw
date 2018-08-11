// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "options.h"

#include <package.h>
#include <target.h>

#include <boost/algorithm/string.hpp>

#include <tuple>

namespace sw
{

IncludeDirectory::IncludeDirectory(const String &s)
{
    i = s;
}

IncludeDirectory::IncludeDirectory(const path &p)
{
    i = p.string();
}

FileRegex::FileRegex(const String &fn, bool recursive)
    : recursive(recursive)
{
    // try to extract dirs from string
    size_t p = 0;

    do
    {
        auto p0 = p;
        p = fn.find_first_of("/*?+", p);
        if (p == -1 || fn[p] != '/')
        {
            r = fn.substr(p0);
            return;
        }

        // scan first part for '\.' pattern that is an exact match for point
        // other patterns? \[ \( \{

        String s = fn.substr(p0, p++ - p0);
        boost::replace_all(s, "\\.", ".");
        boost::replace_all(s, "\\[", "[");
        boost::replace_all(s, "\\]", "]");
        boost::replace_all(s, "\\(", "(");
        boost::replace_all(s, "\\)", ")");
        boost::replace_all(s, "\\{", "{");
        boost::replace_all(s, "\\}", "}");

        if (s.find_first_of("*?+.[](){}") != -1)
        {
            r = fn.substr(p0);
            return;
        }

        dir /= s;
    } while (1);
}

FileRegex::FileRegex(const std::regex &r, bool recursive)
    :r(r), recursive(recursive)
{
}

FileRegex::FileRegex(const path &dir, const std::regex &r, bool recursive)
    : dir(dir), r(r), recursive(recursive)
{
}

template <class C>
void unique_merge_containers(C &c1, const C &c2)
{
    c1.insert(c2.begin(), c2.end());
    /*for (auto &e : c2)
    {
        if (std::find(c1.begin(), c1.end(), e) == c1.end())
            c1.insert(e);
    }*/
}

Dependency::Dependency(const NativeTarget *t)
{
    target = (NativeTarget *)t;
    //package = t->getPackage();
}

Dependency::Dependency(const UnresolvedPackage &p)
{
    package = p;
}

Dependency &Dependency::operator=(const NativeTarget *t)
{
    target = (NativeTarget *)t;
    //package = t->getPackage();
    return *this;
}

/*Dependency &Dependency::operator=(const Package *p)
{
    package = (Package *)p;
    return *this;
}*/

/*NativeTarget *Dependency::get() const
{
    //if (target)
        return target;
    //return package->ResolvedTarget;
}

Dependency::operator NativeTarget*() const
{
    return get();
}

NativeTarget *Dependency::operator->() const
{
    return get();
}*/

UnresolvedPackage Dependency::getPackage() const
{
    if (target)
        return { target->pkg.ppath, target->pkg.version };
    return package;
}

PackageId Dependency::getResolvedPackage() const
{
    if (target)
        return { target->pkg.ppath, target->pkg.version };
    throw std::runtime_error("Package is unresolved: " + getPackage().toString());
}

void NativeCompilerOptionsData::add(const Definition &d)
{
    auto p = d.d.find('=');
    if (p == d.d.npos)
    {
        Definitions[d.d] = 1;
        return;
    }
    auto f = d.d.substr(0, p);
    auto s = d.d.substr(p + 1);
    if (s.empty())
        Definitions[f + "="];
    else
        Definitions[f] = s;
}

void NativeCompilerOptionsData::remove(const Definition &d)
{
    auto p = d.d.find('=');
    if (p == d.d.npos)
    {
        Definitions.erase(d.d);
        return;
    }
    auto f = d.d.substr(0, p);
    auto s = d.d.substr(p + 1);
    if (s.empty())
        Definitions.erase(f + "=");
    else
        Definitions.erase(f);
}

void NativeCompilerOptionsData::add(const DefinitionsType &defs)
{
    Definitions.insert(defs.begin(), defs.end());
}

void NativeCompilerOptionsData::remove(const DefinitionsType &defs)
{
    for (auto &[k, v] : defs)
        Definitions.erase(k);
}

PathOptionsType NativeCompilerOptionsData::gatherIncludeDirectories() const
{
    PathOptionsType d;
    d.insert(PreIncludeDirectories.begin(), PreIncludeDirectories.end());
    d.insert(IncludeDirectories.begin(), IncludeDirectories.end());
    d.insert(PostIncludeDirectories.begin(), PostIncludeDirectories.end());
    return d;
}

bool NativeCompilerOptionsData::IsIncludeDirectoriesEmpty() const
{
    return PreIncludeDirectories.empty() &&
        IncludeDirectories.empty() &&
        PostIncludeDirectories.empty();
}

void NativeCompilerOptionsData::merge(const NativeCompilerOptionsData &o, const GroupSettings &s, bool merge_to_system)
{
    // report conflicts?

    Definitions.insert(o.Definitions.begin(), o.Definitions.end());
    if (s.merge_to_self)
    {
        unique_merge_containers(PreIncludeDirectories, o.PreIncludeDirectories);
        unique_merge_containers(IncludeDirectories, o.IncludeDirectories);
        unique_merge_containers(PostIncludeDirectories, o.PostIncludeDirectories);
    }
    else// if (merge_to_system)
    {
        unique_merge_containers(IncludeDirectories, o.PreIncludeDirectories);
        unique_merge_containers(IncludeDirectories, o.IncludeDirectories);
        unique_merge_containers(IncludeDirectories, o.PostIncludeDirectories);
    }
}

void NativeCompilerOptions::merge(const NativeCompilerOptions &o, const GroupSettings &s)
{
    NativeCompilerOptionsData::merge(o, s);
    System.merge(o.System, s, true);

    /*if (!s.merge_to_self)
    {
        unique_merge_containers(System.IncludeDirectories, o.PreIncludeDirectories);
        unique_merge_containers(System.IncludeDirectories, o.IncludeDirectories);
        unique_merge_containers(System.IncludeDirectories, o.PostIncludeDirectories);
    }*/
}

PathOptionsType NativeLinkerOptionsData::gatherLinkDirectories() const
{
    PathOptionsType d;
    d.insert(PreLinkDirectories.begin(), PreLinkDirectories.end());
    d.insert(LinkDirectories.begin(), LinkDirectories.end());
    d.insert(PostLinkDirectories.begin(), PostLinkDirectories.end());
    return d;
}

PathOptionsType NativeLinkerOptionsData::gatherLinkLibraries() const
{
    PathOptionsType d;
    d.insert(LinkLibraries.begin(), LinkLibraries.end());
    return d;
}

bool NativeLinkerOptionsData::IsLinkDirectoriesEmpty() const
{
    return PreLinkDirectories.empty() &&
        LinkDirectories.empty() &&
        PostLinkDirectories.empty();
}

void NativeLinkerOptionsData::merge(const NativeLinkerOptionsData &o, const GroupSettings &s)
{
    // report conflicts?

    unique_merge_containers(Frameworks, o.Frameworks);
    unique_merge_containers(LinkLibraries, o.LinkLibraries);
    unique_merge_containers(PreLinkDirectories, o.PreLinkDirectories);
    unique_merge_containers(LinkDirectories, o.LinkDirectories);
    unique_merge_containers(PostLinkDirectories, o.PostLinkDirectories);
}

void NativeLinkerOptions::merge(const NativeLinkerOptions &o, const GroupSettings &s)
{
    NativeLinkerOptionsData::merge(o, s);
    System.merge(o.System, s);
}

/*
NativeLinkerOptions &NativeLinkerOptions::operator+=(const NativeTarget &t)
{
    Dependencies.insert((NativeTarget*)&t);
    return *this;
}

NativeLinkerOptions &NativeLinkerOptions::operator=(const DependenciesType &t)
{
    Dependencies = t;
    return *this;
}

NativeLinkerOptions &NativeLinkerOptions::operator+=(const DependenciesType &t)
{
    for (auto &d : t)
        Dependencies.insert(d);
    return *this;
}

NativeLinkerOptions &NativeLinkerOptions::operator+=(const Package &t)
{
    Dependencies.insert(new Package(t));
    return *this;
}
*/

DependencyPtr NativeLinkerOptions::operator+(const NativeTarget &t)
{
    auto d = std::make_shared<Dependency>(&t);
    Dependencies.insert(d);
    return d;
}

DependencyPtr NativeLinkerOptions::operator+(const DependencyPtr &d)
{
    Dependencies.insert(d);
    return d;
}

void NativeLinkerOptions::add(const NativeTarget &t)
{
    Dependencies.insert(std::make_shared<Dependency>((NativeTarget*)&t));
}

void NativeLinkerOptions::remove(const NativeTarget &t)
{
    Dependencies.erase(std::make_shared<Dependency>((NativeTarget*)&t));
}

void NativeLinkerOptions::add(const DependencyPtr &t)
{
    Dependencies.insert(t);
}

void NativeLinkerOptions::remove(const DependencyPtr &t)
{
    Dependencies.erase(t);
}

void NativeOptions::merge(const NativeOptions &o, const GroupSettings &s)
{
    NativeCompilerOptions::merge(o, s);
    NativeLinkerOptions::merge(o, s);
}

}
