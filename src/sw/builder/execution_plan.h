// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "command.h"

#include <sw/support/exceptions.h>

#include <primitives/debug.h>

#include <iso646.h> // for #include <boost/graph/transitive_reduction.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/strong_components.hpp>
#include <boost/graph/transitive_reduction.hpp>
#include <boost/graph/graph_utility.hpp> // dumping graphs
#include <boost/graph/graphviz.hpp>      // generating pictures

namespace sw {

// DAG
template <class T>
struct ExecutionPlan
{
    using PtrT = T*;
    using SPtrT = std::shared_ptr<T>;
    using USet = std::unordered_set<PtrT>;
    using USetSPtrT = std::unordered_set<SPtrT>;
    using Vec = std::vector<PtrT>;

    using VertexNode = size_t;
    using Graph = boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS,
        PtrT, boost::property<boost::edge_index_t, int>>;
    using GraphMapping = std::unordered_map<PtrT, VertexNode>;
    using StrongComponents = std::vector<size_t>;

    Vec commands;
    Vec unprocessed_commands;
    USet unprocessed_commands_set;
    int64_t skip_errors = 0;
    bool throw_on_errors = true;

    ExecutionPlan() = default;
    ExecutionPlan(const ExecutionPlan &rhs) = delete;
    ExecutionPlan(ExecutionPlan &&) = default;
    ~ExecutionPlan()
    {
        auto break_commands = [](auto &a)
        {
            // FIXME: clear() may destroy our next pointer in a,
            // so we make a copy of everything :(
            std::vector<SPtrT> copy;
            copy.reserve(a.size());
            for (auto &c : a)
                copy.emplace_back(c->shared_from_this());
            for (auto &c : a)
                c->clear();
        };
        break_commands(commands);
        break_commands(unprocessed_commands);
        break_commands(unprocessed_commands_set);
    }

    void execute(Executor &e) const
    {
        std::mutex m;
        std::vector<Future<void>> fs;
        std::vector<Future<void>> all;
        std::atomic_bool stopped = false;
        std::atomic_int running = 0;
        std::atomic_int64_t askip_errors = skip_errors;

        // set numbers
        std::atomic_size_t current_command = 1;
        std::atomic_size_t total_commands = commands.size();
        for (auto &c : commands)
        {
            c->total_commands = &total_commands;
            c->current_command = &current_command;
        }

        std::function<void(PtrT)> run;
        run = [this, &askip_errors, &e, &run, &fs, &all, &m, &stopped, &running](T *c)
        {
            if (stopped)
                return;
            try
            {
                running++;
                c->execute();
                running--;
            }
            catch (...)
            {
                running--;
                if (--askip_errors < 1)
                    stopped = true;
                if (throw_on_errors)
                    throw; // don't go futher on DAG by default
            }
            for (auto &d : c->dependent_commands)
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
                    //continue;
                    break;
                fs.push_back(e.push([&run, c] {run(c); }));
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

        // wait other jobs to finish or it will crash
        while (running)
            std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // gather exceptions
        for (auto &f : all)
        {
            if (f.state->eptr)
                eptrs.push_back(f.state->eptr);
        }

        // ... or it will crash here in throw
        if (!eptrs.empty() && throw_on_errors)
            throw ExceptionVector(eptrs);

        if (i != sz/* && !stopped*/)
            throw SW_RUNTIME_ERROR("Executor did not perform all steps");
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

    operator bool() const
    {
        return unprocessed_commands.empty();
    }

    Graph getGraph()
    {
        return getGraph(commands);
    }

    Graph getGraphUnprocessed()
    {
        return getGraph(unprocessed_commands);
    }

    static Graph getGraph(const Vec &v)
    {
        auto gm = getGraphMapping(v);
        return getGraph(v, gm);
    }

    static auto getStrongComponents(const Graph &g)
    {
        StrongComponents components(num_vertices(g));
        auto num = boost::strong_components(g, boost::make_iterator_property_map(components.begin(), get(boost::vertex_index, g)));
        return std::tuple{ g, num, components };
    }

    /// returns true if removed something
    static Graph getGraphSkeleton(const Graph &in)
    {
        //
        throw SW_RUNTIME_ERROR("not implemented");

        // make an algorithm here

        auto g = in;
        while (1)
        {
            bool removed = false;

            typename boost::graph_traits<Graph>::vertex_iterator vi, vi_end, next;
            boost::tie(vi, vi_end) = boost::vertices(g);
            for (next = vi; vi != vi_end; vi = next)
            {
                ++next;
                if (g.m_vertices[(*vi)].m_in_edges.size() == 1 && g.m_vertices[(*vi)].m_out_edges.empty())
                {
                    boost::remove_vertex(*vi, g);
                    removed = true;
                    break; // iterators were invalidated, as we use vecS and not listS
                }
            }
        }
        return g;
    }

    Graph getGraphSkeleton()
    {
        return getGraphSkeleton(getGraph());
    }

    auto getStrongComponents()
    {
        auto g = getGraphUnprocessed();
        return getStrongComponents(g);
    }

    void printGraph(path p) const
    {
        printGraph(getGraph(), p);
    }

    template <class G>
    static void printGraph(const G &g, const path &base, const Vec &names = {}, bool mangle_names = false)
    {
        auto p = base;
        p += ".dot";
        std::ofstream o(p);
        if (names.empty())
            boost::write_graphviz(o, g);
        else
        {
            write_graphviz(o, g, [&names, mangle_names](auto &o, auto v)
            {
                if (mangle_names)
                    o << " [" << "label=\"" << std::to_string(v) << "\"]";
                else
                    o << " [" << "label=" << names[v]->getName(true) << "]";
            });
            if (mangle_names)
            {
                auto p = base;
                p += ".txt";
                std::ofstream o(p);
                int i = 0;
                for (auto &n : names)
                    o << i++ << " = " << n->getName(true) << "\n";
            }
        }
    }

    static ExecutionPlan createExecutionPlan(const USetSPtrT &in)
    {
        USet cmds;
        cmds.reserve(in.size());
        for (auto &c : in)
            cmds.insert(c.get());

        prepare(cmds);

        // detect and eliminate duplicate commands
        if constexpr (std::is_same_v<T, sw::builder::Command>)
        {
            std::unordered_map<size_t, Vec> dups;
            for (const auto &c : cmds)
            {
                auto k = std::hash<T>()(*c);
                dups[k].push_back(c);
            }

            // create replacements
            std::unordered_map<PtrT /*dup*/, PtrT /*repl*/> repls;
            for (auto &[h, v] : dups)
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
                        auto i = repls.find(d.get());
                        if (i == repls.end())
                            continue;
                        to_rm.insert(d.get());
                        to_add.insert(i->second);
                    }
                    for (auto &rm : to_rm)
                        c->dependencies.erase(rm->shared_from_this());
                    for (auto &add : to_add)
                        c->dependencies.insert(add->shared_from_this());
                }
            }

            // we cannot remove outdated before execution
            // because outdated property is changed while executing other commands
        }

        return create(cmds);
    }

private:
    using Vertex = typename boost::graph_traits<Graph>::vertex_descriptor;
    using VertexMap = std::unordered_map<Vertex, Vertex>;

    void setup()
    {
        // potentially *should* speedup later execution
        // TODO: measure and decide
        // it reduses some memory usage,
        // but influence on performance on execution stages is not very clear
        //transitiveReduction();

        // set number of deps and dependent commands
        for (auto &c : commands)
        {
            c->dependencies_left = c->dependencies.size();
            for (auto &d : c->dependencies)
                d->dependent_commands.insert(c->shared_from_this());
        }

        std::sort(commands.begin(), commands.end(), [](const auto &c1, const auto &c2)
        {
            return c1->lessDuringExecution(*c2);
        });
    }

    static GraphMapping getGraphMapping(const Vec &v)
    {
        GraphMapping gm;
        VertexNode i = 0;
        for (auto &c : v)
            gm[c] = i++;
        return gm;
    }

    static Graph getGraph(const Vec &v, GraphMapping &gm)
    {
        Graph g(v.size());
        for (auto &c : v)
        {
            g.m_vertices[gm[c]].m_property = c;
            for (auto &d : c->dependencies)
                boost::add_edge(gm[c], gm[d.get()], g);
        }
        return g;
    }

    void transitiveReduction()
    {
        auto gm = getGraphMapping(commands);
        auto g = getGraph(commands, gm);

        auto[tr, vm] = transitiveReduction(g);

        // copy props
        for (auto &[from, to] : vm)
            tr.m_vertices[to].m_property = g.m_vertices[from].m_property;

        // make new edges (dependencies)
        for (auto &[from, to] : vm)
        {
            auto c = commands[from];
            c->dependencies.clear();
            for (auto &e : tr.m_vertices[to].m_out_edges)
                c->dependencies.insert(tr.m_vertices[e.m_target].m_property);
        }
    }

    static std::tuple<Graph, VertexMap> transitiveReduction(const Graph &g)
    {
        Graph tr;
        VertexMap vm;

        std::vector<size_t> id_map(boost::num_vertices(g));
        std::iota(id_map.begin(), id_map.end(), 0u);

        boost::transitive_reduction(g, tr, boost::make_assoc_property_map(vm), id_map.data());
        return { tr, vm };
    }

    static void prepare(USet &cmds)
    {
        // prepare all commands
        // extract all deps commands

        // try to lower number of rehashes
        auto reserve = [](auto &a)
        {
            a.reserve((a.size() / 10000 + 1) * 20000);
        };
        reserve(cmds);

        while (1)
        {
            size_t sz = cmds.size();

            // initial prepare
            for (auto &c : cmds)
                c->prepare();

            // some commands get its i/o deps in wrong order,
            // so we explicitly call this once more
            for (auto &c : cmds)
                c->addInputOutputDeps();

            // separate loop for additional deps tracking (programs, inputs, outputs etc.)
            auto cmds2 = cmds;
            reserve(cmds2);
            for (auto &c : cmds)
            {
                for (auto &d : c->dependencies)
                {
                    cmds2.insert(d.get());
                    for (auto &d2 : d->dependencies)
                        cmds2.insert(d2.get());
                }
                // also take explicit dependent commands
                for (auto &d : c->dependent_commands)
                    cmds2.insert(d.get());
            }
            cmds = std::move(cmds2);

            if (cmds.size() == sz)
                break;
        }
    }

    void init(USet &cmds)
    {
        // remove self deps
        for (auto &c : cmds)
            c->dependencies.erase(c->shared_from_this());

        while (!cmds.empty())
        {
            bool added = false;
            for (auto it = cmds.begin(); it != cmds.end();)
            {
                if (std::all_of((*it)->dependencies.begin(), (*it)->dependencies.end(),
                    [this, &cmds](auto &d) { return cmds.find(d.get()) == cmds.end(); }))
                {
                    added = true;
                    commands.push_back(*it);
                    it = cmds.erase(it);
                }
                else
                    it++;
            }
            if (!added)
            {
                // We failed with stupid algorithm, now we must perform smarter.
                // Shall we?
                unprocessed_commands.insert(unprocessed_commands.end(), cmds.begin(), cmds.end());
                unprocessed_commands_set = cmds;
                return;
            }
        }
    }

    static ExecutionPlan<T> create(USet &cmds)
    {
        ExecutionPlan<T> ep;
        ep.init(cmds);
        ep.setup();
        return ep;
    }
};

}
