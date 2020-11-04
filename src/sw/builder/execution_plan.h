/*
 * SW - Build System and Package Manager
 * Copyright (C) 2017-2020 Egor Pugin
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

#pragma once

#include "command.h"

#include <iso646.h> // for #include <boost/graph/transitive_reduction.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/strong_components.hpp>
#include <boost/graph/transitive_reduction.hpp>
#include <boost/graph/graph_utility.hpp> // dumping graphs
#include <boost/graph/graphviz.hpp>      // generating pictures

#include <chrono>

struct Executor;

namespace sw
{

struct SwBuilderContext;

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

    using Clock = std::chrono::steady_clock;

public:
    int64_t skip_errors = 0;
    bool throw_on_errors = true;
    bool build_always = false;
    bool silent = false;
    bool show_output = false;
    bool write_output_to_file = false;

    ExecutionPlan(USet &cmds);
    ExecutionPlan(const ExecutionPlan &rhs) = delete;
    ExecutionPlan(ExecutionPlan &&) = default;
    ~ExecutionPlan();

    //
    void execute(Executor &e) const;

    // external request to stop execution
    // running commands will be finished
    // TODO: break running commands too
    void stop(bool interrupt_running_commands = false);

    // functions for builder::Command's
    static Commands load(const path &, const SwBuilderContext &, int type = 0);
    void save(const path &, int type = 0) const;

    void saveChromeTrace(const path &) const;
    void setTimeLimit(const Clock::duration &);

    const VecT &getCommands() const { return commands; }
    const VecT &getUnprocessedCommands() const { return unprocessed_commands; }
    const USet &getUnprocessedCommandsSet() const { return unprocessed_commands_set; }

    bool isValid() const;

    Graph getGraph() const;
    Graph getGraphUnprocessed() const;
    static Graph getGraph(const VecT &v);
    static std::tuple<size_t, StrongComponents> getStrongComponents(const Graph &g);
    /// returns true if removed something
    static Graph getGraphSkeleton(const Graph &in);
    Graph getGraphSkeleton();
    std::tuple<Graph, size_t, StrongComponents> getStrongComponents();
    void printGraph(path p) const;

    template <class G>
    static void printGraph(const G &g, const path &base, const VecT &names = {}, bool mangle_names = false);

    template <class T>
    static std::unique_ptr<ExecutionPlan> create(const std::unordered_set<std::shared_ptr<T>> &in)
    {
        USet cmds;
        cmds.reserve(in.size());
        for (auto &c : in)
            cmds.insert(c.get());
        prepare(cmds);
        return std::make_unique<ExecutionPlan>(cmds);
    }

    template <class T>
    static std::unique_ptr<ExecutionPlan> create(const std::unordered_set<T> &in)
    {
        USet cmds;
        cmds.reserve(in.size());
        for (auto &c : in)
            cmds.insert(c);
        prepare(cmds);
        return std::make_unique<ExecutionPlan>(cmds);
    }

private:
    using Vertex = typename boost::graph_traits<Graph>::vertex_descriptor;
    using VertexMap = std::unordered_map<Vertex, Vertex>;

    VecT commands;
    VecT unprocessed_commands;
    USet unprocessed_commands_set;
    mutable std::atomic_bool interrupted;

    //
    std::optional<Clock::time_point> stop_time;

    static GraphMapping getGraphMapping(const VecT &v);
    static Graph getGraph(const VecT &v, GraphMapping &gm);
    void transitiveReduction();
    static std::tuple<Graph, VertexMap> transitiveReduction(const Graph &g);
    static void prepare(USet &cmds);
    void init(USet &cmds);
};

extern template SW_BUILDER_API void ExecutionPlan::printGraph(const ExecutionPlan::Graph &, const path &base, const ExecutionPlan::VecT &, bool);

}
