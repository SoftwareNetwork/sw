// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "options.h"

#include <package.h>
#include "target/native.h"

#include <boost/algorithm/string.hpp>

#include <tuple>

namespace sw
{

Definition::Definition(const String &s)
{
    d = s;
}

IncludeDirectory::IncludeDirectory(const String &s)
{
    i = s;
}

IncludeDirectory::IncludeDirectory(const path &p)
{
    i = p.string();
}

LinkLibrary::LinkLibrary(const String &s)
{
    l = s;
}

LinkLibrary::LinkLibrary(const path &p)
{
    l = p.string();
}

SystemLinkLibrary::SystemLinkLibrary(const String &s)
{
    l = s;
}

SystemLinkLibrary::SystemLinkLibrary(const path &p)
{
    l = p.string();
}

FileRegex::FileRegex(const String &fn, bool recursive)
    : recursive(recursive)
{
    // try to extract dirs from string
    size_t p = 0;

    do
    {
        auto p0 = p;
        p = fn.find_first_of("/*?+[.\\", p);
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

FileRegex::FileRegex(const path &d, const String &fn, bool recursive)
    : FileRegex(fn, recursive)
{
    if (dir.empty())
        dir = d;
    else if (!d.empty())
        dir = d / dir;
}

FileRegex::FileRegex(const std::regex &r, bool recursive)
    : r(r), recursive(recursive)
{
}

FileRegex::FileRegex(const path &d, const std::regex &r, bool recursive)
    : dir(d), r(r), recursive(recursive)
{
}

template <class C>
void unique_merge_containers(C &to, const C &from)
{
    to.insert(from.begin(), from.end());
    /*for (auto &e : c2)
    {
        if (std::find(c1.begin(), c1.end(), e) == c1.end())
            c1.insert(e);
    }*/
}

Dependency::Dependency(const Target &t)
{
    operator=(t);
}

Dependency::Dependency(const UnresolvedPackage &p)
{
    package = p;
}

Dependency &Dependency::operator=(const Target &t)
{
    setTarget(t);
    return *this;
}

bool Dependency::operator==(const Dependency &t) const
{
    auto t1 = target;
    auto t2 = t.target;
    return std::tie(package, t1) == std::tie(t.package, t2);
}

bool Dependency::operator<(const Dependency &t) const
{
    auto t1 = target;
    auto t2 = t.target;
    return std::tie(package, t1) < std::tie(t.package, t2);
}

UnresolvedPackage Dependency::getPackage() const
{
    auto t = target;
    if (t)
        return { t->pkg.ppath, t->pkg.version };
    return package;
}

PackageId Dependency::getResolvedPackage() const
{
    auto t = target;
    if (t)
        return { t->pkg.ppath, t->pkg.version };
    throw SW_RUNTIME_ERROR("Package is unresolved: " + getPackage().toString());
}

void Dependency::setTarget(const Target &t)
{
    target = (Target*)&t;
    propagateTargetToChain();
}

void Dependency::propagateTargetToChain()
{
    for (auto &c : chain)
    {
        if (c.get() != this)
            c->setTarget(*target);
    }
}

void NativeCompilerOptionsData::add(const Definition &d)
{
    auto p = d.d.find('=');
    if (p == d.d.npos)
    {
        Definitions[d.d];// = 1;
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
    CompileOptions.insert(CompileOptions.end(), o.CompileOptions.begin(), o.CompileOptions.end());
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

void NativeCompilerOptions::addDefinitionsAndIncludeDirectories(builder::Command &c) const
{
    auto print_def = [&c](auto &a)
    {
        for (auto &d : a)
        {
            using namespace sw;

            if (d.second.empty())
                c.args.push_back("-D" + d.first);
            else
                c.args.push_back("-D" + d.first + "=" + d.second);
        }
    };

    print_def(System.Definitions);
    print_def(Definitions);

    auto print_idir = [&c](const auto &a, auto &flag)
    {
        for (auto &d : a)
            c.args.push_back(flag + normalize_path(d));
    };

    print_idir(gatherIncludeDirectories(), "-I");
    print_idir(System.gatherIncludeDirectories(), "-I");
}

void NativeCompilerOptions::addEverything(builder::Command &c) const
{
    addDefinitionsAndIncludeDirectories(c);

    auto print_idir = [&c](const auto &a, auto &flag)
    {
        for (auto &d : a)
            c.args.push_back(flag + normalize_path(d));
    };

    print_idir(System.CompileOptions, "");
    print_idir(CompileOptions, "");
}

PathOptionsType NativeCompilerOptions::gatherIncludeDirectories() const
{
    PathOptionsType idirs;
    auto i = NativeCompilerOptionsData::gatherIncludeDirectories();
    idirs.insert(i.begin(), i.end());
    i = System.gatherIncludeDirectories();
    idirs.insert(i.begin(), i.end());
    return idirs;
}

void NativeLinkerOptionsData::add(const LinkLibrary &l)
{
     LinkLibraries.push_back(l.l);
}

void NativeLinkerOptionsData::remove(const LinkLibrary &l)
{
    LinkLibraries.erase(l.l);
}

PathOptionsType NativeLinkerOptionsData::gatherLinkDirectories() const
{
    PathOptionsType d;
    d.insert(PreLinkDirectories.begin(), PreLinkDirectories.end());
    d.insert(LinkDirectories.begin(), LinkDirectories.end());
    d.insert(PostLinkDirectories.begin(), PostLinkDirectories.end());
    return d;
}

LinkLibrariesType NativeLinkerOptionsData::gatherLinkLibraries() const
{
    LinkLibrariesType d;
    d.insert(d.end(), LinkLibraries.begin(), LinkLibraries.end());
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
    LinkLibraries.insert(LinkLibraries.end(), o.LinkLibraries.begin(), o.LinkLibraries.end());
    LinkOptions.insert(LinkOptions.end(), o.LinkOptions.begin(), o.LinkOptions.end());
    unique_merge_containers(PreLinkDirectories, o.PreLinkDirectories);
    unique_merge_containers(LinkDirectories, o.LinkDirectories);
    unique_merge_containers(PostLinkDirectories, o.PostLinkDirectories);
}

void NativeLinkerOptions::add(const SystemLinkLibrary &l)
{
    System.LinkLibraries.push_back(l.l);
}

void NativeLinkerOptions::remove(const SystemLinkLibrary &l)
{
    System.LinkLibraries.erase(l.l);
}

void NativeLinkerOptions::merge(const NativeLinkerOptions &o, const GroupSettings &s)
{
    // deps are handled separately
    //FileDependencies.insert(o.FileDependencies.begin(), o.FileDependencies.end());
    NativeLinkerOptionsData::merge(o, s);
    System.merge(o.System, s);
}

void NativeLinkerOptions::addEverything(builder::Command &c) const
{
    auto print_idir = [&c](const auto &a, auto &flag)
    {
        for (auto &d : a)
            c.args.push_back(flag + normalize_path(d));
    };

    print_idir(System.LinkOptions, "");
    print_idir(LinkOptions, "");
}

FilesOrdered NativeLinkerOptions::gatherLinkLibraries() const
{
    FilesOrdered llib;
    auto i = NativeLinkerOptionsData::gatherLinkLibraries();
    llib.insert(llib.end(), i.begin(), i.end());
    //i = System.gatherLinkLibraries();
    //llib.insert(llib.end(), i.begin(), i.end());
    return llib;
}

DependencyPtr NativeLinkerOptions::operator+(const Target &t)
{
    auto d = std::make_shared<Dependency>(t);
    add(d);
    return d;
}

DependencyPtr NativeLinkerOptions::operator+(const DependencyPtr &d)
{
    add(d);
    return d;
}

DependencyPtr NativeLinkerOptions::operator+(const PackageId &pkg)
{
    auto d = std::make_shared<Dependency>(pkg);
    add(d);
    return d;
}

void NativeLinkerOptions::add(const Target &t)
{
    add(std::make_shared<Dependency>(t));
}

void NativeLinkerOptions::remove(const Target &t)
{
    remove(std::make_shared<Dependency>(t));
}

void NativeLinkerOptions::add(const DependencyPtr &t)
{
    auto i = std::find_if(Dependencies.begin(), Dependencies.end(), [t](const auto &d)
    {
        return d->getPackage() == t->getPackage();
    });
    if (i == Dependencies.end())
    {
        t->Disabled = false;
        Dependencies.insert(t);
    }
    else
    {
        (*i)->Disabled = false;
        (*i)->chain.push_back(t);
        auto d = (*i)->target;
        if (d)
            t->setTarget(*d);
    }
}

void NativeLinkerOptions::remove(const DependencyPtr &t)
{
    auto i = std::find_if(Dependencies.begin(), Dependencies.end(), [t](const auto &d)
    {
        return d->getPackage() == t->getPackage();
    });
    if (i == Dependencies.end())
    {
        t->Disabled = true;
        Dependencies.insert(t);
    }
    else
    {
        (*i)->Disabled = true;
        (*i)->chain.push_back(t);
        auto d = (*i)->target;
        if (d)
            t->setTarget(*d);
    }
}

void NativeLinkerOptions::add(const UnresolvedPackage &t)
{
    add(std::make_shared<Dependency>(t));
}

void NativeLinkerOptions::remove(const UnresolvedPackage &t)
{
    remove(std::make_shared<Dependency>(t));
}

void NativeLinkerOptions::add(const UnresolvedPackages &t)
{
    for (auto &d : t)
        add(d);
}

void NativeLinkerOptions::remove(const UnresolvedPackages &t)
{
    for (auto &d : t)
        remove(d);
}

void NativeLinkerOptions::add(const PackageId &p)
{
    add(std::make_shared<Dependency>(p));
}

void NativeLinkerOptions::remove(const PackageId &p)
{
    remove(std::make_shared<Dependency>(p));
}

void NativeOptions::merge(const NativeOptions &o, const GroupSettings &s)
{
    NativeCompilerOptions::merge(o, s);
    NativeLinkerOptions::merge(o, s);
}

}
