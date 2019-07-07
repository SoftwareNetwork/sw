// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "sw_context.h"

#include <nlohmann/json.hpp>
#include <primitives/executor.h>
#include <primitives/pack.h>

#include <regex>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "context");

namespace sw
{

void detectCompilers(SwCoreContext &s);

IDriver::~IDriver() = default;

SwCoreContext::SwCoreContext(const path &local_storage_root_dir)
    : SwBuilderContext(local_storage_root_dir)
{
    source_dir = fs::canonical(fs::current_path());

    detectCompilers(*this);
    predefined_targets = targets; // save

    host_settings = toTargetSettings(getHostOs());

    auto &ts = host_settings;
    ts["native"]["configuration"] = "release";
    ts["native"]["library"] = "shared";

    if (getHostOs().is(OSType::Windows))
    {
        ts["native"]["lib"]["c"] = "com.Microsoft.Windows.SDK.ucrt";
        ts["native"]["lib"]["c++"] = "com.Microsoft.VisualStudio.VC.libcpp";

        ts["native"]["rc"] = "com.Microsoft.Windows.rc";

        //if (!getTargets()["com.Microsoft.VisualStudio.VC.cl"].empty_releases())
        if (!getTargets()["com.Microsoft.VisualStudio.VC.cl"].empty())
        {
            ts["native"]["cl"] = "com.Microsoft.VisualStudio.VC.cl";
            ts["native"]["ml"] = "com.Microsoft.VisualStudio.VC.ml";
            ts["native"]["lib"] = "com.Microsoft.VisualStudio.VC.lib";
            ts["native"]["link"] = "com.Microsoft.VisualStudio.VC.link";
        }

        //ts["native"]["cl.c"] = "com.Microsoft.VisualStudio.VC.cl";
        //ts["native"]["extensions"s][".c"] = "com.Microsoft.VisualStudio.VC.cl";
        //ts["native"]["cl.cpp"] = "com.Microsoft.VisualStudio.VC.cl";
    }
}

SwCoreContext::~SwCoreContext()
{
}

const TargetSettings &SwCoreContext::getHostSettings() const
{
    return host_settings;
}

SwContext::SwContext(const path &local_storage_root_dir)
    : SwCoreContext(local_storage_root_dir)
{
}

SwContext::~SwContext()
{
}

void SwContext::load()
{
    load(inputs);
}

void SwContext::build()
{
    load(inputs);
    execute();
}

void SwContext::execute()
{
    configure();
    auto p = getExecutionPlan();
    execute(p);
}

void SwContext::resolvePackages()
{
    // gather
    UnresolvedPackages upkgs;
    for (const auto &[pkg, tgts] : getTargets())
    {
        for (const auto &tgt : tgts)
        {
            auto deps = tgt->getDependencies();
            // we filter out existing targets as they come from same module
            for (auto &d : deps)
            {
                if (auto id = d->getUnresolvedPackage().toPackageId(); id && getTargets().find(*id) != getTargets().end())
                    continue;
                upkgs.insert(d->getUnresolvedPackage());
            }
            break; // take first as all deps are equal
        }
    }

    // install
    auto m = install(upkgs);

    // now we know all drivers
    ProcessedInputs inputs;
    for (auto &[u, p] : m)
        inputs.emplace(p, *this);
    load(inputs);

    // load
    while (1)
    {
        std::map<TargetSettings, TargetData *> load;
        auto &chld = getTargets();
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
                    if (i != chld.end())
                    {
                        auto k = i->second.find(d->getSettings());
                        if (k != i->second.end())
                        {
                            d->setTarget(**k);
                            continue;
                        }
                        load[d->getSettings()] = &i->second;
                    }
                }
            }
        }
        if (load.empty())
            break;
        for (auto &[s, d] : load)
        {
            d->loadPackages(s);
            auto k = d->find(s);
            if (k == d->end())
            {
                throw SW_RUNTIME_ERROR("cannot load package with current settings:\n" + s.toString());
                //throw SW_RUNTIME_ERROR(pkg.toString() + ": cannot load package " + d->getUnresolvedPackage().toString() +
                    //" with current settings\n" + d->getSettings().toString());
            }
        }
    }
}

bool SwContext::prepareStep()
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

void SwContext::configure()
{
    // mark existing targets as targets to build
    targets_to_build = getTargets();

    resolvePackages();
    while (prepareStep())
        ;
}

void SwContext::execute(CommandExecutionPlan &p) const
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

    ScopedTime t;
    std::unique_ptr<Executor> ex;
    //if (execute_jobs > 0)
        //ex = std::make_unique<Executor>(execute_jobs);
    auto &e = /*execute_jobs > 0 ? *ex : */getExecutor();

    // prevent memory leaks (high mem usage)
    /*updateConcurrentContext();
    for (int i = 0; i < 1000; i++)
    e.push([] {updateConcurrentContext(); });*/

    //p.skip_errors = skip_errors.getValue();
    p.execute(e);
    /*auto t2 = t.getTimeFloat();
    if (!silent && t2 > 0.15)
        LOG_INFO(logger, "Build time: " << t2 << " s.");*/

    // produce chrome tracing log
    /*if (time_trace)
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
        write_file(getServiceDir() / "time_trace.json", trace.dump(2));
    }*/

    // prevent memory leaks (high mem usage)
    /*updateConcurrentContext();
    for (int i = 0; i < 1000; i++)
    e.push([] {updateConcurrentContext(); });*/
}

Commands SwContext::getCommands() const
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

SwContext::CommandExecutionPlan SwContext::getExecutionPlan() const
{
    return getExecutionPlan(getCommands());
}

SwContext::CommandExecutionPlan SwContext::getExecutionPlan(const Commands &cmds) const
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

PackageDescriptionMap SwContext::getPackages() const
{
    PackageDescriptionMap m;
    for (auto &[pkg, tgts] : getTargets())
    {
        // deps
        if (pkg.ppath.isAbsolute())
            continue;

        auto &t = *tgts.begin();
        if (!t->isReal())
            continue;

        nlohmann::json j;

        // source, version, path
        t->getSource().save(j["source"]);
        j["version"] = pkg.getVersion().toString();
        j["path"] = pkg.ppath.toString();

        auto rd = source_dir;
        /*if (!fetch_info.sources.empty())
        {
            auto src = t->getSource().clone(); // copy
            src->applyVersion(pkg.version);
            auto si = fetch_info.sources.find(src->getHash());
            if (si == fetch_info.sources.end())
                throw SW_RUNTIME_ERROR("no such source");
            rd = si->second;
        }*/
        j["root_dir"] = normalize_path(rd);

        // double check files (normalize them)
        Files files;
        for (auto &f : t->getSourceFiles())
            files.insert(f.lexically_normal());

        // we put files under SW_SDIR_NAME to keep space near it
        // e.g. for patch dir or other dirs (server provided files)
        // we might unpack to other dir, but server could push service files in neighbor dirs like gpg keys etc
        nlohmann::json jm;
        auto files_map1 = primitives::pack::prepare_files(files, rd.lexically_normal());
        for (const auto &[f1, f2] : files_map1)
        {
            nlohmann::json jf;
            jf["from"] = normalize_path(f1);
            jf["to"] = normalize_path(f2);
            j["files"].push_back(jf);
        }

        // deps
        for (auto &d : t->getDependencies())
        {
            nlohmann::json jd;
            jd["path"] = d->getUnresolvedPackage().ppath.toString();
            jd["range"] = d->getUnresolvedPackage().range.toString();
            j["dependencies"].push_back(jd);
        }

        auto s = j.dump();
        m[pkg] = std::make_unique<JsonPackageDescription>(s);
    }
    return m;
}

String SwContext::getBuildHash() const
{
    String s;
    for (auto &i : inputs)
        s += i.getHash();
    return shorten_hash(blake2b_512(s), 6);
}

void SwContext::load(ProcessedInputs &inputs)
{
    std::map<IDriver *, ProcessedInputs> active_drivers;
    for (auto &i : inputs)
        active_drivers[&i.getDriver()].insert(i);
    for (auto &[d, g] : active_drivers)
        d->load(g);
}

void SwContext::registerDriver(std::unique_ptr<IDriver> driver)
{
    drivers.insert_or_assign(driver->getPackageId(), std::move(driver));
}

Input &SwContext::addInput(const String &i)
{
    path p(i);
    if (fs::exists(p))
        return addInput(p);
    else
        return addInput(PackageId(i));
}

Input &SwContext::addInput(const path &i)
{
    auto p = inputs.emplace(i, *this);
    return (Input &)*p.first;
}

Input &SwContext::addInput(const PackageId &i)
{
    auto p = inputs.emplace(i, *this);
    return (Input &)*p.first;
}

Input::Input(const path &p, const SwContext &swctx)
{
    init(p, swctx);
}

Input::Input(const PackageId &p, const SwContext &swctx)
{
    init(p, swctx);
}

const std::set<TargetSettings> &Input::getSettings() const
{
    if (settings.empty())
        throw SW_RUNTIME_ERROR("No input settings provided");
    return settings;
}

void Input::addSettings(const TargetSettings &s)
{
    settings.insert(s);
}

String Input::getHash() const
{
    String s;
    switch (getType())
    {
    case InputType::InstalledPackage:
        s = getPackageId().toString();
        break;
    default:
        s = normalize_path(getPath());
        break;
    }
    for (auto &ss : settings)
        s += ss.getHash();
    return s;
}

void Input::init(const path &in, const SwContext &swctx)
{
    auto find_driver = [this, &swctx](auto t)
    {
        type = t;
        for (auto &[_, d] : swctx.getDrivers())
        {
            if (d->canLoad(*this))
            {
                driver = d.get();
                return true;
            }
        }
        return false;
    };

    path p = in;
    if (p.empty())
        p = swctx.source_dir;
    if (!p.is_absolute())
        p = fs::absolute(p);

    auto status = fs::status(p);
    data = path(normalize_path(p));

    // spec or regular file
    if (status.type() == fs::file_type::regular)
    {
        if (find_driver(InputType::SpecificationFile) ||
            find_driver(InputType::InlineSpecification))
            return;

        // find in file first: 'sw driver package-id', call that driver on whole file
        auto f = read_file(p);

        static const std::regex r("sw\\s+driver\\s+(\\S+)");
        std::smatch m;
        if (std::regex_search(f, m, r))
        {
            SW_UNIMPLEMENTED;

            /*
            - install driver
            - load & register it
            - re-run this ctor
            */

            auto driver_pkg = swctx.install({ m[1].str() }).find(m[1].str());
            return;
        }
    }
    else if (status.type() == fs::file_type::directory)
    {
        if (find_driver(InputType::DirectorySpecificationFile) ||
            find_driver(InputType::Directory))
            return;
    }
    else
        throw SW_RUNTIME_ERROR("Bad file type: " + normalize_path(p));

    throw SW_RUNTIME_ERROR("Cannot select driver for " + normalize_path(p));
}

void Input::init(const PackageId &p, const SwContext &swctx)
{
    data = p;
    type = InputType::InstalledPackage;
    driver = swctx.getDrivers().begin()->second.get();
}

path Input::getPath() const
{
    return std::get<path>(data);
}

PackageId Input::getPackageId() const
{
    return std::get<PackageId>(data);
}

bool Input::operator<(const Input &rhs) const
{
    return data < rhs.data;
}

bool Input::isChanged() const
{
    return true;

    switch (getType())
    {
    //case InputType::SpecificationFile:
        //return true;
    default:
        SW_UNIMPLEMENTED;
    }
}

}

