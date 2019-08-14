// Copyright (C) 2017-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "execution_plan.h"

#include <sw/support/exceptions.h>

#include <nlohmann/json.hpp>
#include <primitives/exceptions.h>

namespace sw
{

ExecutionPlan::~ExecutionPlan()
{
    auto break_commands = [](auto &a)
    {
        // FIXME: clear() may destroy our next pointer in 'a' variable,
        // so we make a copy of everything :(
        std::vector<std::shared_ptr<T>> copy;
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

void ExecutionPlan::execute(Executor &e) const
{
    if (commands.empty())
        return;

    std::mutex m;
    std::vector<Future<void>> fs;
    std::vector<Future<void>> all;
    std::atomic_bool stopped = false;
    std::atomic_int running = 0;
    std::atomic_int64_t askip_errors = skip_errors;

    bool build_commands = dynamic_cast<builder::Command *>(*commands.begin());

    // set numbers
    std::atomic_size_t current_command = 1;
    std::atomic_size_t total_commands = commands.size();
    for (auto &c : commands)
    {
        c->total_commands = &total_commands;
        c->current_command = &current_command;
        if (build_commands)
        {
            static_cast<builder::Command*>(c)->silent |= silent;
            static_cast<builder::Command*>(c)->show_output |= show_output;
            static_cast<builder::Command*>(c)->write_output_to_file |= write_output_to_file;
            static_cast<builder::Command*>(c)->always |= build_always;
        }
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
                fs.push_back(e.push([&run, d] {run((T *)d.get()); }));
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

void ExecutionPlan::saveChromeTrace(const path &p) const
{
    // calculate minimal time
    auto min = decltype (builder::Command::t_begin)::clock::now();
    for (auto &c : commands)
    {
        if (static_cast<builder::Command*>(c)->t_begin.time_since_epoch().count() == 0)
            continue;
        min = std::min(static_cast<builder::Command*>(c)->t_begin, min);
    }

    auto tid_to_ll = [](auto &id)
    {
        std::ostringstream ss;
        ss << id;
        return ss.str();
    };

    nlohmann::json trace;
    nlohmann::json events;
    for (auto &c : commands)
    {
        if (static_cast<builder::Command*>(c)->t_begin.time_since_epoch().count() == 0)
            continue;

        nlohmann::json b;
        b["name"] = c->getName();
        b["cat"] = "BUILD";
        b["pid"] = 1;
        b["tid"] = tid_to_ll(static_cast<builder::Command*>(c)->tid);
        b["ts"] = std::chrono::duration_cast<std::chrono::microseconds>(static_cast<builder::Command*>(c)->t_begin - min).count();
        b["ph"] = "B";
        events.push_back(b);

        nlohmann::json e;
        e["name"] = c->getName();
        e["cat"] = "BUILD";
        e["pid"] = 1;
        e["tid"] = tid_to_ll(static_cast<builder::Command*>(c)->tid);
        e["ts"] = std::chrono::duration_cast<std::chrono::microseconds>(static_cast<builder::Command*>(c)->t_end - min).count();
        e["ph"] = "E";
        events.push_back(e);
    }
    trace["traceEvents"] = events;
    write_file(p, trace.dump(2));
}

ExecutionPlan::operator bool() const
{
    return unprocessed_commands.empty();
}

ExecutionPlan::Graph ExecutionPlan::getGraph() const
{
    return getGraph(commands);
}

ExecutionPlan::Graph ExecutionPlan::getGraphUnprocessed() const
{
    return getGraph(unprocessed_commands);
}

ExecutionPlan::Graph ExecutionPlan::getGraph(const VecT &v)
{
    auto gm = getGraphMapping(v);
    return getGraph(v, gm);
}

auto ExecutionPlan::getStrongComponents(const Graph &g)
{
    StrongComponents components(num_vertices(g));
    auto num = boost::strong_components(g, boost::make_iterator_property_map(components.begin(), get(boost::vertex_index, g)));
    return std::tuple{ g, num, components };
}

/// returns true if removed something
ExecutionPlan::Graph ExecutionPlan::getGraphSkeleton(const Graph &in)
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

ExecutionPlan::Graph ExecutionPlan::getGraphSkeleton()
{
    return getGraphSkeleton(getGraph());
}

auto ExecutionPlan::getStrongComponents()
{
    auto g = getGraphUnprocessed();
    return getStrongComponents(g);
}

template <class G>
void ExecutionPlan::printGraph(const G &g, const path &base, const VecT &names, bool mangle_names)
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

void ExecutionPlan::printGraph(path p) const
{
    printGraph(getGraph(), p);
}

void ExecutionPlan::setup()
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

ExecutionPlan::GraphMapping ExecutionPlan::getGraphMapping(const VecT &v)
{
    GraphMapping gm;
    VertexNode i = 0;
    for (auto &c : v)
        gm[c] = i++;
    return gm;
}

ExecutionPlan::Graph ExecutionPlan::getGraph(const VecT &v, GraphMapping &gm)
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

void ExecutionPlan::transitiveReduction()
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
            c->dependencies.insert(tr.m_vertices[e.m_target].m_property->shared_from_this());
    }
}

std::tuple<ExecutionPlan::Graph, ExecutionPlan::VertexMap> ExecutionPlan::transitiveReduction(const Graph &g)
{
    Graph tr;
    VertexMap vm;

    std::vector<size_t> id_map(boost::num_vertices(g));
    std::iota(id_map.begin(), id_map.end(), 0u);

    boost::transitive_reduction(g, tr, boost::make_assoc_property_map(vm), id_map.data());
    return { tr, vm };
}

void ExecutionPlan::prepare(USet &cmds)
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
        // do not remove!
        for (auto &c : cmds)
        {
            if (auto c1 = dynamic_cast<builder::Command*>(c))
                c1->addInputOutputDeps();
        }

        // separate loop for additional deps tracking (programs, inputs, outputs etc.)
        auto cmds2 = cmds;
        reserve(cmds2);
        for (auto &c : cmds)
        {
            for (auto &d : c->dependencies)
            {
                cmds2.insert((T *)d.get());
                for (auto &d2 : d->dependencies)
                    cmds2.insert((T *)d2.get());
            }
            // also take explicit dependent commands
            for (auto &d : c->dependent_commands)
                cmds2.insert((T *)d.get());
        }
        cmds = std::move(cmds2);

        if (cmds.size() == sz)
            break;
    }
}

void ExecutionPlan::init(USet &cmds)
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
                [this, &cmds](auto &d) { return cmds.find((T *)d.get()) == cmds.end(); }))
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

ExecutionPlan ExecutionPlan::create(USet &cmds)
{
    ExecutionPlan ep;
    ep.init(cmds);
    ep.setup();
    return ep;
}

}
