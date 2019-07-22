// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "build.h"

#include "driver.h"
#include "sw_context.h"

#include <nlohmann/json.hpp>
#include <primitives/executor.h>
#include <primitives/sw/cl.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "build");

//cl::opt<bool> dry_run("n", cl::desc("Dry run"));

cl::opt<bool> build_always("B", cl::desc("Build always"));
cl::opt<int> skip_errors("k", cl::desc("Skip errors"));
static cl::opt<bool> time_trace("time-trace", cl::desc("Record chrome time trace events"));

//static cl::opt<bool> hide_output("hide-output");
static cl::opt<bool> cl_show_output("show-output");
static cl::opt<bool> print_graph("print-graph", cl::desc("Print file with build graph"));

namespace sw
{

SwBuild::SwBuild(const SwContext &swctx)
    : swctx(swctx)
{
}

void SwBuild::load()
{
    load(inputs);
}

void SwBuild::build()
{
    // this is all in one call

    // load provided inputs
    load();

    //
    setTargetsToBuild();

    //
    resolvePackages();

    //
    loadPackages();

    // prepare targets
    prepare();

    // create ex. plan and execute it
    execute();
}

void SwBuild::execute()
{
    auto p = getExecutionPlan();
    execute(p);
}

void SwBuild::resolvePackages()
{
    // gather
    UnresolvedPackages upkgs;
    for (const auto &[pkg, tgts] : getTargets())
    {
        for (const auto &tgt : tgts)
        {
            auto deps = tgt->getDependencies();
            for (auto &d : deps)
            {
                // filter out existing targets as they come from same module
                if (auto id = d->getUnresolvedPackage().toPackageId(); id && getTargets().find(*id) != getTargets().end())
                    continue;
                // filter out predefined targets
                if (swctx.getPredefinedTargets().find(d->getUnresolvedPackage().ppath) != swctx.getPredefinedTargets().end(d->getUnresolvedPackage().ppath))
                    continue;
                upkgs.insert(d->getUnresolvedPackage());
            }
            break; // take first as all deps are equal
        }
    }

    // install
    auto m = swctx.install(upkgs);

    // now we know all drivers
    ProcessedInputs inputs;
    for (auto &[u, p] : m)
        inputs.emplace(p, swctx);
    load(inputs);
}

void SwBuild::loadPackages()
{
    loadPackages(getTargets());
}

void SwBuild::loadPackages(TargetMap &targets) const
{
    loadPackages(targets, swctx.getPredefinedTargets());
}

void SwBuild::loadPackages(TargetMap &targets, const TargetMap &predefined)
{
    // load
    while (1)
    {
        std::map<TargetSettings, std::pair<PackageId, TargetData *>> load;
        auto &chld = targets; // take a ref, because it won't be changed in this loop
        for (const auto &[pkg, tgts] : chld)
        {
            for (const auto &tgt : tgts)
            {
                auto deps = tgt->getDependencies();
                for (auto &d : deps)
                {
                    if (d->isResolved())
                        continue;

                    auto i = chld.find(d->getUnresolvedPackage());
                    if (i == chld.end())
                        throw SW_RUNTIME_ERROR("No target loaded: " + d->getUnresolvedPackage().toString());

                    auto k = i->second.find(d->getSettings());
                    if (k != i->second.end())
                    {
                        d->setTarget(**k);
                        continue;
                    }

                    if (predefined.find(d->getUnresolvedPackage().ppath) != predefined.end(d->getUnresolvedPackage().ppath))
                    {
                        throw SW_LOGIC_ERROR(tgt->getPackage().toString() + ": predefined target is not resolved: " + d->getUnresolvedPackage().toString());
                    }

                    load.insert({ d->getSettings(), { i->first, &i->second } });
                }
            }
        }
        if (load.empty())
            break;
        for (auto &[s, d] : load)
        {
            // empty settings mean we want dependency only to be present
            if (s.empty())
                continue;

            d.second->loadPackages(s, {}/* { d.first }*/ );
            auto k = d.second->find(s);
            if (k == d.second->end())
            {
                //throw SW_RUNTIME_ERROR("cannot load package with current settings:\n" + s.toString());
                throw SW_RUNTIME_ERROR("cannot load package " + d.first.toString() + " with current settings\n" + s.toString());
            }
        }
    }
}


bool SwBuild::prepareStep()
{
    std::atomic_bool next_pass = false;

    auto &e = getExecutor();
    Futures<void> fs;
    for (const auto &[pkg, tgts] : getTargets())
    {
        for (const auto &tgt : tgts)
        {
            fs.push_back(e.push([tgt, &next_pass]
            {
                if (tgt->prepare())
                    next_pass = true;
            }));
        }
    }
    waitAndGet(fs);

    return next_pass;
}

void SwBuild::setTargetsToBuild()
{
    // mark existing targets as targets to build
    // only in case if not present?
    if (targets_to_build.empty())
        targets_to_build = getTargets();
    for (auto &[pkg, d] : swctx.getPredefinedTargets())
        targets_to_build.erase(pkg.ppath);
}

void SwBuild::prepare()
{
    while (prepareStep())
        ;
}

void SwBuild::execute(CommandExecutionPlan &p) const
{
    auto print_graph = [](const auto &ep, const path &p, bool short_names = false)
    {
        String s;
        s += "digraph G {\n";
        for (auto &c : ep.commands)
        {
            {
                s += c->getName(short_names) + ";\n";
                for (auto &d : c->dependencies)
                    s += c->getName(short_names) + " -> " + d->getName(short_names) + ";\n";
            }
            /*s += "{";
            s += "rank = same;";
            for (auto &c : level)
            s += c->getName(short_names) + ";\n";
            s += "};";*/
        }

        /*if (ep.Root)
        {
        const auto root_name = "all"s;
        s += root_name + ";\n";
        for (auto &d : ep.Root->dependencies)
        s += root_name + " -> " + d->getName(short_names) + ";\n";
        }*/

        s += "}";
        write_file(p, s);
    };

    /*for (auto &c : p.commands)
    {
        c->silent = silent;
        c->show_output = show_output;
    }*/

    // execute early to prevent commands expansion into response files
    // print misc
    /*if (::print_graph && !silent) // && !b console mode
    {
        auto d = getServiceDir();

        //message_box(d.string());

        // new graphs
        //p.printGraph(p.getGraphSkeleton(), d / "build_skeleton");
        p.printGraph(p.getGraph(), d / "build");

        // old graphs
        print_graph(p, d / "build_old.dot");

        if (auto b = this->template as<Build*>())
        {
            SW_UNIMPLEMENTED;
            //for (const auto &[i, s] : enumerate(b->solutions))
            //s.printGraph(d / ("solution." + std::to_string(i + 1) + ".dot"));
        }
    }

    if (dry_run)
        return;*/

    if (build_always)
    {
        for (auto &c : p.commands)
            c->always = true;
    }

    ScopedTime t;
    std::unique_ptr<Executor> ex;
    //if (execute_jobs > 0)
        //ex = std::make_unique<Executor>(execute_jobs);
    auto &e = /*execute_jobs > 0 ? *ex : */getExecutor();

    // prevent memory leaks (high mem usage)
    /*updateConcurrentContext();
    for (int i = 0; i < 1000; i++)
    e.push([] {updateConcurrentContext(); });*/

    p.skip_errors = skip_errors.getValue();
    p.execute(e);
    /*auto t2 = t.getTimeFloat();
    if (!silent && t2 > 0.15)
        LOG_INFO(logger, "Build time: " << t2 << " s.");*/

    // produce chrome tracing log
    if (time_trace)
    {
        // calculate minimal time
        auto min = decltype (builder::Command::t_begin)::clock::now();
        for (auto &c : p.commands)
        {
            if (c->t_begin.time_since_epoch().count() == 0)
                continue;
            min = std::min(c->t_begin, min);
        }

        auto tid_to_ll = [](auto &id)
        {
            std::ostringstream ss;
            ss << id;
            return ss.str();
        };

        nlohmann::json trace;
        nlohmann::json events;
        for (auto &c : p.commands)
        {
            if (c->t_begin.time_since_epoch().count() == 0)
                continue;

            nlohmann::json b;
            b["name"] = c->getName();
            b["cat"] = "BUILD";
            b["pid"] = 1;
            b["tid"] = tid_to_ll(c->tid);
            b["ts"] = std::chrono::duration_cast<std::chrono::microseconds>(c->t_begin - min).count();
            b["ph"] = "B";
            events.push_back(b);

            nlohmann::json e;
            e["name"] = c->getName();
            e["cat"] = "BUILD";
            e["pid"] = 1;
            e["tid"] = tid_to_ll(c->tid);
            e["ts"] = std::chrono::duration_cast<std::chrono::microseconds>(c->t_end - min).count();
            e["ph"] = "E";
            events.push_back(e);
        }
        trace["traceEvents"] = events;
        write_file(swctx.source_dir / SW_BINARY_DIR / "misc" / "time_trace.json", trace.dump(2));
    }

    // prevent memory leaks (high mem usage)
    /*updateConcurrentContext();
    for (int i = 0; i < 1000; i++)
    e.push([] {updateConcurrentContext(); });*/
}

Commands SwBuild::getCommands() const
{
    // calling this for all targets in any case to set proper command dependencies
    for (const auto &[pkg, tgts] : getTargets())
    {
        for (auto &tgt : tgts)
        {
            for (auto &c : tgt->getCommands())
                c->maybe_unused = builder::Command::MU_TRUE; // why?
        }
    }

    if (targets_to_build.empty())
        throw SW_RUNTIME_ERROR("no targets were selected for building");

    Commands cmds;
    for (auto &[p, tgts] : targets_to_build)
    {
        for (auto &tgt : tgts)
        {
            auto c = tgt->getCommands();
            for (auto &c2 : c)
                c2->maybe_unused &= ~builder::Command::MU_TRUE;
            cmds.insert(c.begin(), c.end());

            // copy output dlls

            /*auto nt = t->as<NativeCompiledTarget*>();
            if (!nt)
                continue;
            if (*nt->HeaderOnly)
                continue;
            if (nt->getSelectedTool() == nt->Librarian.get())
                continue;

            // copy
            if (nt->isLocal() && //getSettings().Native.CopySharedLibraries &&
                nt->Scope == TargetScope::Build && nt->OutputDir.empty() && !nt->createWindowsRpath())
            {
                for (auto &l : nt->gatherAllRelatedDependencies())
                {
                    auto dt = l->as<NativeCompiledTarget*>();
                    if (!dt)
                        continue;
                    if (dt->isLocal())
                        continue;
                    if (dt->HeaderOnly.value())
                        continue;
                    if (dt->getSettings().Native.LibrariesType != LibraryType::Shared && !dt->isSharedOnly())
                        continue;
                    if (dt->getSelectedTool() == dt->Librarian.get())
                        continue;
                    auto in = dt->getOutputFile();
                    auto o = nt->getOutputDir() / dt->OutputDir;
                    o /= in.filename();
                    if (in == o)
                        continue;

                    SW_MAKE_EXECUTE_BUILTIN_COMMAND(copy_cmd, *nt, "sw_copy_file", nullptr);
                    copy_cmd->arguments.push_back(in.u8string());
                    copy_cmd->arguments.push_back(o.u8string());
                    copy_cmd->addInput(dt->getOutputFile());
                    copy_cmd->addOutput(o);
                    copy_cmd->dependencies.insert(nt->getCommand());
                    copy_cmd->name = "copy: " + normalize_path(o);
                    copy_cmd->maybe_unused = builder::Command::MU_ALWAYS;
                    copy_cmd->command_storage = builder::Command::CS_LOCAL;
                    cmds.insert(copy_cmd);
                }
            }*/
        }
    }

    return cmds;
}

SwBuild::CommandExecutionPlan SwBuild::getExecutionPlan() const
{
    return getExecutionPlan(getCommands());
}

SwBuild::CommandExecutionPlan SwBuild::getExecutionPlan(const Commands &cmds) const
{
    auto ep = CommandExecutionPlan::createExecutionPlan(cmds);
    if (ep)
        return ep;

    // error!

    /*auto d = getServiceDir();

    auto [g, n, sc] = ep.getStrongComponents();

    using Subgraph = boost::subgraph<CommandExecutionPlan::Graph>;

    // fill copy of g
    Subgraph root(g.m_vertices.size());
    for (auto &e : g.m_edges)
        boost::add_edge(e.m_source, e.m_target, root);

    std::vector<Subgraph*> subs(n);
    for (decltype(n) i = 0; i < n; i++)
        subs[i] = &root.create_subgraph();
    for (int i = 0; i < sc.size(); i++)
        boost::add_vertex(i, *subs[sc[i]]);

    auto cyclic_path = d / "cyclic";
    fs::create_directories(cyclic_path);
    for (decltype(n) i = 0; i < n; i++)
    {
        if (subs[i]->m_graph.m_vertices.size() > 1)
            CommandExecutionPlan::printGraph(subs[i]->m_graph, cyclic_path / std::to_string(i));
    }

    ep.printGraph(ep.getGraph(), cyclic_path / "processed", ep.commands, true);
    ep.printGraph(ep.getGraphUnprocessed(), cyclic_path / "unprocessed", ep.unprocessed_commands, true);*/

    //String error = "Cannot create execution plan because of cyclic dependencies: strong components = " + std::to_string(n);
    String error = "Cannot create execution plan because of cyclic dependencies";

    throw SW_RUNTIME_ERROR(error);
}

String SwBuild::getSpecification() const
{
    if (inputs.empty())
        throw SW_RUNTIME_ERROR("Empty inputs");
    if (inputs.size() > 1)
        throw SW_RUNTIME_ERROR("More than 1 input");
    return inputs.begin()->getDriver().getSpecification();
}

String SwBuild::getHash() const
{
    String s;
    for (auto &i : inputs)
        s += i.getHash();
    return shorten_hash(blake2b_512(s), 6);
}

void SwBuild::load(ProcessedInputs &inputs)
{
    std::map<IDriver *, ProcessedInputs> active_drivers;
    for (auto &i : inputs)
        active_drivers[&i.getDriver()].insert(i);
    for (auto &[d, g] : active_drivers)
        d->load(*this, g);
}

Input &SwBuild::addInput(const String &i)
{
    path p(i);
    if (fs::exists(p))
        return addInput(p);
    else
        return addInput(PackageId(i));
}

Input &SwBuild::addInput(const path &i)
{
    auto p = inputs.emplace(i, swctx);
    return (Input &)*p.first;
}

Input &SwBuild::addInput(const PackageId &i)
{
    auto p = inputs.emplace(i, swctx);
    return (Input &)*p.first;
}

}

