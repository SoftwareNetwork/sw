// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "command.h"

#include <iso646.h> // for #include <boost/graph/transitive_reduction.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/strong_components.hpp>
#include <boost/graph/transitive_reduction.hpp>
#include <boost/graph/graph_utility.hpp> // dumping graphs
#include <boost/graph/graphviz.hpp>      // generating pictures

namespace sw
{

// DAG
struct SW_BUILDER_API ExecutionPlan
{
    using T = CommandNode;

    template <class T>
    using Vec = std::vector<T>;

    using PtrT = T*;
    using USet = std::unordered_set<PtrT>;
    using VecT = Vec<PtrT>;

    using VertexNode = size_t;
    using Graph = boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS,
        PtrT, boost::property<boost::edge_index_t, int>>;
    using GraphMapping = std::unordered_map<PtrT, VertexNode>;
    using StrongComponents = std::vector<size_t>;

    USet unprocessed_commands_set;
    int64_t skip_errors = 0;
    bool throw_on_errors = true;

    ExecutionPlan() = default;
    ExecutionPlan(const ExecutionPlan &rhs) = delete;
    ExecutionPlan(ExecutionPlan &&) = default;
    ~ExecutionPlan();

    void execute(Executor &e) const;

    template <class U = T>
    Vec<U*> &getCommands()
    {
        return (Vec<U*> &)commands;
    }

    template <class U = T>
    const Vec<U*> &getCommands() const
    {
        return (Vec<U*> &)commands;
    }

    /*StringHashMap<int> gatherStrings() const
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
            insert(c->working_directory.u8string());
            for (auto &a : c->arguments)
                insert(a->toString());
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
            //for (auto &f : c->intermediate)
                //insert(f.u8string());
            for (auto &f : c->outputs)
                insert(f.u8string());
        }

        return strings;
    }*/

    explicit operator bool() const;

    Graph getGraph() const;
    Graph getGraphUnprocessed() const;
    static Graph getGraph(const VecT &v);
    static auto getStrongComponents(const Graph &g);
    /// returns true if removed something
    static Graph getGraphSkeleton(const Graph &in);
    Graph getGraphSkeleton();
    auto getStrongComponents();
    void printGraph(path p) const;

    template <class G>
    static void printGraph(const G &g, const path &base, const VecT &names = {}, bool mangle_names = false);

    template <class T>
    static ExecutionPlan createExecutionPlan(const std::unordered_set<T> &in)
    {
        USet cmds;
        cmds.reserve(in.size());
        for (auto &c : in)
            cmds.insert(c.get());

        prepare(cmds);
        return create(cmds);
    }

private:
    using Vertex = typename boost::graph_traits<Graph>::vertex_descriptor;
    using VertexMap = std::unordered_map<Vertex, Vertex>;

    VecT commands;
    VecT unprocessed_commands;

    void setup();
    static GraphMapping getGraphMapping(const VecT &v);
    static Graph getGraph(const VecT &v, GraphMapping &gm);
    void transitiveReduction();
    static std::tuple<Graph, VertexMap> transitiveReduction(const Graph &g);
    static void prepare(USet &cmds);
    void init(USet &cmds);
    static ExecutionPlan create(USet &cmds);
};

}
