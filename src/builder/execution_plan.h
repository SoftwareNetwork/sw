// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <command.h>
#include <exceptions.h>

#include <primitives/debug.h>

template <class T>
struct ExecutionPlan
{
    using PtrT = std::shared_ptr<T>;
    using USet = std::unordered_set<PtrT>;

    std::vector<PtrT> commands;

    ExecutionPlan() = default;
    ExecutionPlan(const ExecutionPlan &) = delete;
    ExecutionPlan(ExecutionPlan &&) = default;
    ~ExecutionPlan()
    {
        // break commands
        for (auto &c : commands)
        {
            c->dependendent_commands.clear();
            c->dependencies.clear();
        }
    }

    void execute(Executor &e) const
    {
        std::mutex m;
        std::vector<Future<void>> fs;
        std::vector<Future<void>> all;
        std::atomic_bool stopped = false;

        std::function<void(T*)> run;
        run = [&e, &run, &fs, &all, &m, &stopped](T *c)
        {
            if (stopped)
                return;
            try
            {
                c->execute();
            }
            catch (...)
            {
                stopped = true;
                throw;
            }
            for (auto &d : c->dependendent_commands)
            {
                if (--d->dependencies_left == 0)
                {
                    std::unique_lock<std::mutex> lk(m);
                    fs.push_back(e.push([&run, d] {run(d.get()); }));
                    all.push_back(fs.back());
                }
            }
        };

        // we cannot know exact number of commands to be executed,
        // because some of them might use write_file_if_different idiom,
        // so actual number is known only at runtime
        // we can just skip some amount of numbers

        // TODO: check non-outdated commands and lower total_commands
        // total_commands -= non outdated;

        // run commands without deps
        {
            std::unique_lock<std::mutex> lk(m);
            for (auto &c : commands)
            {
                if (!c->dependencies.empty())
                    break;
                fs.push_back(e.push([&run, c] {run(c.get()); }));
                all.push_back(fs.back());
            }
        }

        // wait for all commands until exception
        int i = 0;
        auto sz = commands.size();
        std::vector<std::exception_ptr> eptrs;
        while (i != sz)
        {
            std::vector<Future<void>> fs2;
            {
                std::unique_lock<std::mutex> lk(m);
                fs2 = fs;
                fs.clear();
            }
            for (auto &f : fs2)
            {
                i++;
                f.wait();
            }
            if (stopped || fs2.empty())
                break;
        }

        // gather exceptions
        for (auto &f : all)
        {
            if (f.state->eptr)
                eptrs.push_back(f.state->eptr);
        }

        if (!eptrs.empty())
            throw ExceptionVector(eptrs);

        if (i != sz)
            throw std::runtime_error("Executor did not perform all steps");
    }

    StringHashMap<int> gatherStrings() const
    {
        StringHashMap<int> strings;
        int i = 1;

        auto insert = [&strings, &i](const auto &s)
        {
            auto &v = strings[s];
            if (v)
                return;
            v = i++;
        };

        for (auto &c : commands)
        {
            insert(c->getName());
            insert(c->program.u8string());
            insert(c->working_directory.u8string());
            for (auto &a : c->args)
                insert(a);
            insert(c->in.file.u8string());
            insert(c->out.file.u8string());
            insert(c->err.file.u8string());
            for (auto &[k, v] : c->environment)
            {
                insert(k);
                insert(v);
            }
            for (auto &f : c->inputs)
                insert(f.u8string());
            for (auto &f : c->intermediate)
                insert(f.u8string());
            for (auto &f : c->outputs)
                insert(f.u8string());
        }

        return strings;
    }

    static ExecutionPlan createExecutionPlan(USet &cmds)
    {
        prepare(cmds);

        // detect and eliminate duplicate commands
        if constexpr (std::is_same_v<T, sw::builder::Command>)
        {
            std::unordered_map<size_t, std::vector<PtrT>> dups;
            for (const auto &c : cmds)
            {
                if (!c->isHashable())
                    continue;
                auto k = std::hash<T>()(*c);
                dups[k].push_back(c);
            }

            // create replacements
            std::unordered_map<PtrT /*dup*/, PtrT /*repl*/> repls;
            for (auto &[h,v] : dups)
            {
                if (v.size() < 2)
                    continue;
                // we take back command as its easier to take
                auto repl = v.back();
                v.pop_back();
                for (auto &c : v)
                {
                    repls[c] = repl;
                    // remove dups from cmds
                    cmds.erase(c);
                }
            }

            if (!repls.empty())
            {
                // go through all command and its deps and do replacements
                for (auto &c : cmds)
                {
                    USet to_rm, to_add;
                    for (auto &d : c->dependencies)
                    {
                        auto i = repls.find(d);
                        if (i == repls.end())
                            continue;
                        to_rm.insert(d);
                        to_add.insert(i->second);
                    }
                    for (auto &rm : to_rm)
                        c->dependencies.erase(rm);
                    for (auto &add : to_add)
                        c->dependencies.insert(add);
                }
            }

            // remove not outdated before execution
            /*while (1)
            {
                USet erased;
                for (auto &c : cmds)
                {
                    if (c->dependencies.empty() && !c->isOutdated())
                        erased.insert(c);
                }

                // remove erased deps from the rest of commands deps
                for (auto &c : erased)
                    cmds.erase(c);
                for (auto &c : cmds)
                {
                    for (auto &e : erased)
                        c->dependencies.erase(e);
                }

                if (erased.empty())
                    break;
            }*/
        }

        /*{
            auto ep = create(cmds);
            if (!cmds.empty())
                return ep;

            // here we are sure that DAG is possible

            // push all cmds back
            cmds.insert(ep.commands.begin(), ep.commands.end());

            // prevent destruction
            ep.commands.clear();
        }

        // now we set dependencies between commands with same outputs
        // to execute them sequentially, because they will break each other on producing same files
        if constexpr (std::is_same_v<T, sw::builder::Command>)
        {
            std::unordered_map<path, std::vector<PtrT>> outputs;

            // gather outputs
            for (const auto &c : cmds)
            {
                for (auto &o : c->outputs)
                    outputs[o].push_back(c);
            }

            std::multimap<size_t, path> mm;
            for (auto &[f, v] : outputs)
            {
                if (v.size() < 2)
                    continue;
                mm.emplace(v.size(), f);
                // sort, more deps commands go first
                std::sort(v.begin(), v.end(),
                    [](const auto &e1, const auto &e2)
                {
                    // also check existing relations, we respect them
                    if (e1->dependencies.find(e2) != e1->dependencies.end())
                        return false;
                    if (e2->dependencies.find(e1) != e2->dependencies.end())
                        return true;
                    return e1->dependencies.size() > e2->dependencies.size();
                });
                // we set a chain of commands 1..n
                // Cn -> Cn-1 -> ... -> C2 -> C1
                for (auto i = std::next(v.begin()); i != v.end(); i++)
                    (*i)->dependencies.insert(*(i - 1));
            }
            for (auto &[n, f] : mm)
                std::cout << n << ": " << f.string() << std::endl;
        }*/

        // create again
        auto ep = create(cmds);

        // set number of deps and dependent commands
        for (auto &c : ep.commands)
        {
            c->dependencies_left = c->dependencies.size();
            for (auto &d : c->dependencies)
                d->dependendent_commands.insert(c);
        }

        // improve sorting! it's too stupid
        // simple "0 0 0 0 1 2 3 6 7 8 9 11" is not enough
        std::sort(ep.commands.begin(), ep.commands.end(), [](const auto &c1, const auto &c2)
        {
            if (c1->dependencies.size() != c2->dependencies.size())
                return c1->dependencies.size() < c2->dependencies.size();
            //if (c1->dependendent_commands.size() != c2->dependendent_commands.size())
                return c1->dependendent_commands.size() > c2->dependendent_commands.size();
            //return c1->getName() < c2->getName();
        });

        return ep;// std::move(ep);
    }

private:
    static void prepare(USet &cmds)
    {
        // prepare all commands
        // extract all deps commands

        size_t sz = cmds.size();
        while (1)
        {
            for (auto &c : cmds)
                c->prepare();
            // separate loop for additional deps tracking (programs, inputs, outputs etc.)
            auto cmds2 = cmds;
            for (auto &c : cmds)
            {
                for (auto &d : c->dependencies)
                {
                    cmds2.insert(d);
                    for (auto &d2 : d->dependencies)
                        cmds2.insert(d2);
                }
            }
            cmds = cmds2;

            if (cmds.size() == sz)
                break;
            sz = cmds.size();
        }
    }

    static ExecutionPlan<T> create(USet &cmds)
    {
        ExecutionPlan<T> ep;
        while (!cmds.empty())
        {
            bool added = false;
            for (auto it = cmds.begin(); it != cmds.end();)
            {
                if (std::all_of((*it)->dependencies.begin(), (*it)->dependencies.end(),
                    [&cmds](auto &d) { return cmds.find(d) == cmds.end(); }))
                {
                    added = true;
                    ep.commands.push_back(*it);
                    it = cmds.erase(it);
                }
                else
                    it++;
            }
            if (!added)
            {
                // We failed with stupid algorithm, now we must perform smarter.
                // Shall we?
                return ep;
            }
        }
        return ep;
    }
};
