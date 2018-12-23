// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <command.h>
#include <exceptions.h>

#include <primitives/debug.h>

#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/strong_components.hpp>
#include <boost/graph/transitive_reduction.hpp>
#include <boost/graph/graph_utility.hpp> // dumping graphs
#include <boost/graph/graphviz.hpp>      // generating pictures

// DAG
template <class T>
struct ExecutionPlan
{
    using PtrT = std::shared_ptr<T>;
    using USet = std::unordered_set<PtrT>;
    using Vec = std::vector<PtrT>;

    using VertexNode = size_t;
    using Graph = boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS,
        PtrT, boost::property<boost::edge_index_t, int>>;
    using GraphMapping = std::unordered_map<T*, VertexNode>;
    using StrongComponents = std::vector<size_t>;

    Vec commands;
    Vec unprocessed_commands;
    USet unprocessed_commands_set;

    ExecutionPlan() = default;
    ExecutionPlan(const ExecutionPlan &rhs) = delete;
    ExecutionPlan(ExecutionPlan &&) = default;
    ~ExecutionPlan()
    {
        auto break_commands = [](auto &a)
        {
            for (auto &c : a)
                c->clear();
        };
        break_commands(commands);
        break_commands(unprocessed_commands);
        break_commands(unprocessed_commands_set);
    }

    void execute(Executor &e, std::atomic_int64_t skip_errors = 0) const
    {
        std::mutex m;
        std::vector<Future<void>> fs;
        std::vector<Future<void>> all;
        std::atomic_bool stopped = false;

        std::function<void(T*)> run;
        run = [&skip_errors, &e, &run, &fs, &all, &m, &stopped](T *c)
        {
            if (stopped)
                return;
            try
            {
                c->execute();
            }
            catch (...)
            {
                if (--skip_errors < 1)
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
            throw SW_RUNTIME_EXCEPTION("Executor did not perform all steps");
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

    static ExecutionPlan createExecutionPlan(const USet &in)
    {
        auto cmds = in;
        prepare(cmds);

        // detect and eliminate duplicate commands
        if constexpr (std::is_same_v<T, sw::builder::Command>)
        {
            std::unordered_map<size_t, Vec> dups;
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
        // it reduses some memory usage, but influence on performance on execution stages
        // is not very clear
        transitiveReduction();

        // set number of deps and dependent commands
        for (auto &c : commands)
        {
            c->dependencies_left = c->dependencies.size();
            for (auto &d : c->dependencies)
                d->dependendent_commands.insert(c);
        }

        // improve sorting! it's too stupid
        // simple "0 0 0 0 1 2 3 6 7 8 9 11" is not enough
        std::sort(commands.begin(), commands.end(), [](const auto &c1, const auto &c2)
        {
            if (c1->dependencies.size() != c2->dependencies.size())
                return c1->dependencies.size() < c2->dependencies.size();
            return c1->dependendent_commands.size() > c2->dependendent_commands.size();
        });
    }

    static GraphMapping getGraphMapping(const Vec &v)
    {
        GraphMapping gm;
        VertexNode i = 0;
        for (auto &c : v)
            gm[c.get()] = i++;
        return gm;
    }

    static Graph getGraph(const Vec &v, GraphMapping &gm)
    {
        Graph g(v.size());
        for (auto &c : v)
        {
            g.m_vertices[gm[c.get()]].m_property = c;
            for (auto &d : c->dependencies)
                boost::add_edge(gm[c.get()], gm[d.get()], g);
        }
        return g;
    }

    void transitiveReduction()
    {
        auto gm = getGraphMapping(commands);
        auto g = getGraph(commands, gm);

        auto [tr, vm] = transitiveReduction(g);

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

        size_t sz = cmds.size();
        while (1)
        {
            // initial prepare
            for (auto &c : cmds)
                c->prepare();

            // some commands get its i/o deps in wrong order,
            // so we explicitly call this once more
            for (auto &c : cmds)
                c->addInputOutputDeps();

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

    void init(USet &cmds)
    {
        // remove self deps
        for (auto &c : cmds)
            c->dependencies.erase(c);

        while (!cmds.empty())
        {
            bool added = false;
            for (auto it = cmds.begin(); it != cmds.end();)
            {
                if (std::all_of((*it)->dependencies.begin(), (*it)->dependencies.end(),
                    [this, &cmds](auto &d) { return cmds.find(d) == cmds.end(); }))
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
