// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin

#include "execution_plan.h"

#include <sw/support/exceptions.h>

#include <nlohmann/json.hpp>
#include <primitives/exceptions.h>
#include <primitives/executor.h>

namespace sw
{

ExecutionPlan::ExecutionPlan(USet &cmds)
{
    init(cmds);
}

ExecutionPlan::~ExecutionPlan()
{
    auto break_commands = [](auto &a)
    {
        // FIXME: clear() may destroy our next pointer in 'a' variable,
        // so we make a copy of everything :(
        /*std::vector<std::shared_ptr<T>> copy;
        copy.reserve(a.size());
        for (auto &c : a)
            copy.emplace_back(c->shared_from_this());
        for (auto &c : a)
            c->clear();*/
    };
    break_commands(commands);
    break_commands(unprocessed_commands);
    break_commands(unprocessed_commands_set);
}

void ExecutionPlan::stop(bool interrupt_running_commands)
{
    if (interrupt_running_commands)
        SW_UNIMPLEMENTED;
    interrupted = true;
}

void ExecutionPlan::execute(Executor &e) const
{
    if (!isValid())
        throw SW_RUNTIME_ERROR("Invalid execution plan");
    if (commands.empty())
        return;

    std::mutex m;
    std::vector<Future<void>> fs;
    std::vector<Future<void>> all;
    std::atomic_bool stopped = false;
    interrupted = false;
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
        //c->markForExecution();
    }

    std::function<void(PtrT)> run;
    run = [this, &askip_errors, &e, &run, &fs, &all, &m, &running, &stopped](T *c)
    {
        if (stopped || interrupted)
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
                fs.push_back(e.push([&run, d] {run(d); }));
                all.push_back(fs.back());
            }
        }

        if (stop_time && Clock::now() > *stop_time)
            stopped = true;
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
            if (!c->getDependencies().empty())
                //continue;
                break;
            fs.push_back(e.push([&run, c] {run(c); }));
            all.push_back(fs.back());
        }
    }

    if (fs.empty())
        throw SW_RUNTIME_ERROR("No commands without deps were added");

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
        if (stopped || fs2.empty() || interrupted)
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
        throw support::ExceptionVector(eptrs);

    if (i != sz)
    {
        if (stop_time && Clock::now() > *stop_time && stopped)
            throw SW_RUNTIME_ERROR("Time limit exceeded");
        if (interrupted)
            throw SW_RUNTIME_ERROR("Interrupted");
        throw SW_RUNTIME_ERROR("Executor did not perform all steps (" + std::to_string(i) + "/" + std::to_string(sz) + ")");
    }
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
        auto c2 = static_cast<builder::Command *>(c);
        if (!c2)
            continue;
        if (c2->t_begin.time_since_epoch().count() == 0)
            continue;

        nlohmann::json b;
        b["name"] = c->getName();
        b["cat"] = "BUILD";
        b["pid"] = 1;
        b["tid"] = tid_to_ll(c2->tid);
        b["ts"] = std::chrono::duration_cast<std::chrono::microseconds>(c2->t_begin - min).count();
        b["ph"] = "B";
        events.push_back(b);

        nlohmann::json e;
        e["name"] = c->getName();
        e["cat"] = "BUILD";
        e["pid"] = 1;
        e["tid"] = tid_to_ll(c2->tid);
        e["ts"] = std::chrono::duration_cast<std::chrono::microseconds>(c2->t_end - min).count();
        e["ph"] = "E";
        for (auto &a : c2->getArguments())
            e["args"]["command_line"].push_back(a->toString());
        for (auto &[k, v] : c2->environment)
            e["args"]["environment"][k] = v;
        events.push_back(e);
    }
    trace["traceEvents"] = events;
    write_file(p, trace.dump(2));
}

bool ExecutionPlan::isValid() const
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

std::tuple<size_t, ExecutionPlan::StrongComponents> ExecutionPlan::getStrongComponents(const Graph &g)
{
    StrongComponents components(num_vertices(g));
    auto num = boost::strong_components(g, boost::make_iterator_property_map(components.begin(), get(boost::vertex_index, g)));
    return std::tuple{ num, components };
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

std::tuple<ExecutionPlan::Graph, size_t, ExecutionPlan::StrongComponents> ExecutionPlan::getStrongComponents()
{
    auto g = getGraphUnprocessed();
    auto [num, components] = getStrongComponents(g);
    return std::tuple{ g, num, components };
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
                o << " [" << "label=" << names[v]->getName() << "]";
        });
        if (mangle_names)
        {
            auto p = base;
            p += ".txt";
            std::ofstream o(p);
            int i = 0;
            for (auto &n : names)
                o << i++ << " = " << n->getName() << "\n";
        }
    }
}

template void ExecutionPlan::printGraph(const ExecutionPlan::Graph &, const path &base, const ExecutionPlan::VecT &, bool);

void ExecutionPlan::printGraph(path p) const
{
    printGraph(getGraph(), p);
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
        for (auto &d : c->getDependencies())
            boost::add_edge(gm[c], gm[d], g);
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

    // make new edges (getDependencies())
    for (auto &[from, to] : vm)
    {
        auto c = commands[from];
        c->clearDependencies();
        for (auto &e : tr.m_vertices[to].m_out_edges)
            c->addDependency(*tr.m_vertices[e.m_target].m_property);
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
    // prepare commands
    for (auto &c : cmds)
        c->prepare();

    // 1. check that we have all of deps too
    // 2. check that we do not have duplicates by hash
    std::unordered_set<size_t> hashes;
    for (auto &c : cmds)
    {
        if (!hashes.emplace(c->getHash()).second)
            throw SW_RUNTIME_ERROR("Duplicate command passed: " + c->getName());

        for (auto &d : c->getDependencies())
        {
            if (!cmds.contains(d))
                throw SW_RUNTIME_ERROR("You did not pass command that is in dependency: " + d->getName());
        }
    }

    // some commands get its i/o deps in wrong order,
    // so we explicitly call this once more
    // do not remove!
    std::unordered_map<path, CommandNode *> generators;
    std::unordered_map<path, std::unordered_set<CommandNode *>> simultaneous_generators;
    generators.reserve(cmds.size());
    for (auto &c : cmds)
    {
        auto c1 = dynamic_cast<builder::Command *>(c);
        if (!c1)
            continue;
        for (auto &o : c1->outputs)
        {
            if (!generators.emplace(o, c1).second)
                throw SW_RUNTIME_ERROR("Output file is generated with more than one command: " + to_printable_string(o));
        }
        for (auto &o : c1->simultaneous_outputs)
            simultaneous_generators[o].insert(c1);
    }
    for (auto &c : cmds)
    {
        auto c1 = dynamic_cast<builder::Command *>(c);
        if (!c1)
            continue;
        auto f = [&generators, &simultaneous_generators, &c1](auto &inputs)
        {
            for (auto &i : inputs)
            {
                if (auto it = generators.find(i); it != generators.end())
                    c1->addDependency(*it->second);
                if (auto it = simultaneous_generators.find(i); it != simultaneous_generators.end())
                {
                    for (auto &&c : it->second)
                        c1->addDependency(*c);
                }
            }
        };
        f(c1->inputs);
        f(c1->inputs_without_timestamps);
    }
}

void ExecutionPlan::init(USet &cmds)
{
    while (!cmds.empty())
    {
        bool added = false;
        for (auto it = cmds.begin(); it != cmds.end();)
        {
            // count number of deps
            auto n = std::count_if((*it)->getDependencies().begin(), (*it)->getDependencies().end(),
                [this, &cmds](auto &d) { return cmds.find(d) != cmds.end(); });
            if (n)
            {
                it++;
                continue;
            }
            added = true;
            commands.push_back(*it);
            it = cmds.erase(it);
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

    // setup

    // potentially *should* speedup later execution
    // TODO: measure and decide
    // it reduses some memory usage,
    // but influence on performance on execution stages is not very clear
    //transitiveReduction();

    // set number of deps and dependent commands
    for (auto &c : commands)
    {
        c->dependencies_left = c->getDependencies().size();
        for (auto &d : c->getDependencies())
            d->dependent_commands.insert(c);
    }

    std::sort(commands.begin(), commands.end(), [](const auto &c1, const auto &c2)
    {
        return c1->lessDuringExecution(*c2);
    });
}

void ExecutionPlan::setTimeLimit(const Clock::duration &d)
{
    stop_time = Clock::now() + d;
}

}
