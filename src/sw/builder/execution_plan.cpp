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

#include "execution_plan.h"

#include <sw/support/exceptions.h>

// clang(win)+linux workaround
#if defined(__linux__) && defined(__clang__)
#define BOOST_ASIO_HAS_CO_AWAIT
#define BOOST_ASIO_HAS_STD_COROUTINE
#endif
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <primitives/exceptions.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "explan");

namespace sw
{

uint16_t get_module_mapper_port() {
    static const auto port = []() {
        using tcp = boost::asio::ip::tcp;
        boost::asio::io_context ctx;
        tcp::acceptor acceptor(ctx, tcp::endpoint(tcp::v6(), 0));
        return acceptor.local_endpoint().port();
    }();
    return port;
}

struct gcc_modules_server {
    const ExecutionPlan &ep;
    boost::asio::io_context ctx_main;
    boost::asio::io_context ctx_headers;
    std::thread t_main;
    std::thread t_headers;

    ~gcc_modules_server() {
        ctx_main.stop();
        ctx_headers.stop();
        if (t_headers.joinable())
            t_headers.join();
        if (t_main.joinable())
            t_main.join();
    }
    void run() {
        // compile handling
        boost::asio::co_spawn(ctx_main, accept(ctx_main, get_module_mapper_port()), boost::asio::detached);
        // scan handling
        boost::asio::co_spawn(ctx_main, accept(ctx_main, get_module_mapper_port() + 1, true), boost::asio::detached);
        // import header handling
        boost::asio::co_spawn(ctx_headers, accept(ctx_headers, get_module_mapper_port() + 2, true), boost::asio::detached);
        t_main = std::thread{[this](){
            try {
                ctx_main.run();
            } catch (...) {}
        }};
        t_headers = std::thread{[this](){
            try {
                ctx_headers.run();
            } catch (...) {}
        }};
    }
    boost::asio::awaitable<void> accept(auto &&ctx, auto port, bool scan = false) {
        using tcp = boost::asio::ip::tcp;
        tcp::acceptor acceptor(ctx, tcp::endpoint(tcp::v6(), port));
        while (1) {
            auto sock = co_await acceptor.async_accept(boost::asio::use_awaitable);
            boost::asio::co_spawn(ctx, scan ? process_scan(std::move(sock)) : process2(std::move(sock)), boost::asio::detached);
        }
    }
    boost::asio::awaitable<void> process2(auto socket) {
        using namespace boost::asio;
        auto reply = [&socket](auto &&line, std::string s) {
            if (line.ends_with(';'))
                s += " ;";
            s += "\n";
            return async_write(socket, buffer(s), use_awaitable);
        };
        struct data {
            path out;
            String source;
            String export_module;
            std::unordered_map<String, path> import_modules;
            std::unordered_map<String, path> header_units;

            ~data() {
                if (!source.empty())
                    write_file_module_mapper();
            }
            void write_file_module_mapper() {
                std::ostringstream ss;
                ss << "$root ." << "\n";
                if (!export_module.empty())
                    ss << export_module << " " << export_module << "\n";
                auto print = [&ss](auto &&what) {
                    for (auto &&[k,v] : what)
                        ss << k << " " << v.string() << "\n";
                };
                print(import_modules);
                print(header_units);
                write_file(out.parent_path() / (out.stem().stem() += ".map"), ss.str());
            }
            void write() {
                // follow msvc here
                nlohmann::json j;
                j["Version"] = "1.1";
                auto &jd = j["Data"];
                jd["Source"] = source;
                jd["ProvidedModule"] = export_module;
                for (auto &&[n,p] : import_modules) {
                    nlohmann::json m;
                    m["Name"] = n;
                    m["BMI"] = p.u8string();
                    jd["ImportedModules"].push_back(m);
                }
                for (auto &&[n,p] : header_units) {
                    nlohmann::json m;
                    m["Name"] = n;
                    m["BMI"] = p.u8string();
                    jd["ImportedHeaderUnits"].push_back(m);
                }
                write_file(out, j.dump());
            }
        } d;
        while (1) {
            std::string buf;
            co_await async_read_until(socket, dynamic_buffer(buf, 1024), "\n", use_awaitable);
            auto lines = split_lines(buf);
            for (auto &&line : lines) {
                if (line.starts_with("HELLO")) {
                    auto parts = split_string(line, " '"); // hello,ver,prog,ident,;
                    auto parts2 = split_string(parts[3], ":");
                    d.source = parts2[0];
                    d.out = parts2[1];
                    co_await reply(line, "HELLO 1 sw");
                } else if (line.starts_with("MODULE-REPO")) {
                    co_await reply(line, "PATHNAME .");
                } else if (line.starts_with("MODULE-EXPORT")) {
                    auto module = split_string(line, " ")[1]; // module,name
                    d.export_module = module;
                    co_await reply(line, "PATHNAME " + module + ".cmi");
                } else if (line.starts_with("MODULE-IMPORT")) {
                    auto module = split_string(line, " ")[1]; // module,name
                    bool header = path{module}.is_absolute();
                    if (header) {
                        d.header_units[module] = d.out.parent_path() / "gcm.cache" / ("." + module + ".gcm");
                    } else
                        d.import_modules[module] = d.out.parent_path() / (module + ".cmi");
                    d.write();
                    if (header)
                        co_await reply(line, "PATHNAME gcm.cache/." + module + ".gcm");
                    else
                        co_await reply(line, "PATHNAME " + module + ".cmi");
                } else if (line.starts_with("MODULE-COMPILED")) {
                    d.write();
                    co_await reply(line, "OK"); // could be any string actually
                } else {
                    co_await reply(line, "ERROR 'Unknown command: " + line.substr(0, line.find(' ')) + "'");
                    co_return;
                }
                /*
                "INCLUDE-TRANSLATE"
                "INVOKE"
                */
            }
        }
    }
    boost::asio::awaitable<void> process_scan(auto socket) {
        using namespace boost::asio;
        auto reply = [&socket](auto &&line, std::string s) {
            if (line.ends_with(';'))
                s += " ;";
            LOG_TRACE(logger, "socket " << socket.native_handle() << "< " << s);
            s += "\n";
            return async_write(socket, buffer(s), use_awaitable);
        };
        builder::Command *this_command{nullptr};
        builder::Command::msvc_modules_scan_data d;
        std::string module;
        while (1) {
            std::string buf;
            co_await async_read_until(socket, dynamic_buffer(buf, 1024), "\n", use_awaitable);
            auto lines = split_lines(buf);
            std::string error;
            try {
                for (auto &&line : lines) {
                    LOG_TRACE(logger, "socket " << socket.native_handle() << "> " << line);
                    if (line.starts_with("HELLO")) {
                        auto parts = split_string(line, " '"); // hello,ver,prog,ident,;
                        auto parts2 = split_string(parts[3], ":");
                        d.source = parts2[0];
                        d.out = parts2[1];
                        co_await reply(line, "HELLO 1 sw");
                    } else if (line.starts_with("MODULE-REPO")) {
                        co_await reply(line, "PATHNAME .");
                    } else if (line.starts_with("MODULE-EXPORT")) {
                        module = split_string(line, " ")[1]; // module,name
                        bool header = path{module}.is_absolute();
                        if (header) {
                            d.export_module = module;
                            d.write();
                            co_await reply(line, "PATHNAME " + (d.out.parent_path() / d.out.stem().stem()).string() + ".gcm");
                        }
                        else {
                            d.export_module = module;
                            d.write();
                            co_await reply(line, "PATHNAME " + module + ".cmi");
                        }
                    } else if (line.starts_with("MODULE-IMPORT")) {
                        auto module = split_string(line, " ")[1]; // module,name
                        bool header = path{module}.is_absolute();
                        if (header) {
                            d.header_units.push_back(module);
                            // we create import header immediately
                            auto fn = d.out.parent_path() / ("gcm.cache/." + module + ".gcm");
                            if (!fs::exists(fn)) {
                                if (!this_command) {
                                    auto &cmds = ep.getCommands();
                                    auto it = std::find_if(cmds.begin(), cmds.end(), [&d](auto &&v){ return static_cast<builder::Command*>(v)->outputs.contains(d.out); });
                                    if (it == cmds.end()) {
                                        co_await reply(line, "ERROR 'Cannot find according command for import header: " + module + "'");
                                        co_return;
                                    }
                                    this_command = static_cast<builder::Command*>(*it);
                                }
                                auto &from = *this_command;
                                auto cmd = std::make_shared<builder::Command>(from.getContext());
                                auto &c = *cmd;
                                c.working_directory = from.working_directory;
                                c.command_storage = from.command_storage;
                                c.environment = from.environment;
                                c.addOutput(fn);
                                c.setProgram(from.arguments[0]->toString());
                                for (int i = 0; auto &&a : from.arguments) {
                                    if (i++) {
                                        if (0
                                            || a->toString().starts_with("-o")
                                            )
                                            continue;
                                        if (a->toString().starts_with("-fmodule-mapper")) {
                                            c.arguments.push_back(std::make_unique<primitives::command::SimpleArgument>(
                                                "-fmodule-mapper=:::" + std::to_string(get_module_mapper_port() + 2) + "?"s + module + ":"
                                                + d.out.parent_path().string() + "/gcm.cache" + module + ".ifc.json"));
                                            continue;
                                        }
                                        if (a->toString().starts_with("-E")) {
                                            c.arguments.push_back(std::make_unique<primitives::command::SimpleArgument>("-c"s));
                                            c.arguments.push_back(std::make_unique<primitives::command::SimpleArgument>("-xc++-header"s));
                                            continue;
                                        }
                                        if (a->toString().starts_with("/"))
                                            c.arguments.push_back(std::make_unique<primitives::command::SimpleArgument>(module));
                                        else
                                            c.arguments.push_back(std::make_unique<primitives::command::SimpleArgument>(a->toString()));
                                        continue;
                                    }
                                }
                                LOG_INFO(logger, "building import header: " << fn);
                                LOG_TRACE(logger, "import header command: " << fn << "\n" << c.print());
                                c.execute();
                            }
                        }
                        else
                            d.import_modules.push_back(module);
                        d.write();
                        if (header)
                            co_await reply(line, "PATHNAME gcm.cache/." + module + ".gcm");
                        else
                            co_await reply(line, "PATHNAME " + module + ".cmi");
                    } else if (line.starts_with("MODULE-COMPILED")) {
                        co_await reply(line, "OK"); // could be any string actually
                    } else if (line.starts_with("INCLUDE-TRANSLATE")) {
                        co_await reply(line, "BOOL TRUE");
                    } else {
                        co_await reply(line, "ERROR 'Unknown command: " + line.substr(0, line.find(' ')) + "'");
                        co_return;
                    }
                    /*
                    ""
                    "INVOKE"
                    */
                }
            } catch (std::exception &e) {
                LOG_ERROR(logger, "ERROR: " << e.what());
                error = e.what();
            }
            if (!error.empty()) {
                co_await reply(""s, "ERROR '"s + error + "'");
                co_return;
            }
        }
    }
};

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
    gcc_modules_server s{*this};
    s.run();

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
                fs.push_back(e.push([&run, d] {run((T *)d.get()); }));
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
    // 1. prepare all commands
    // 2. extract all deps commands
    // 3. remove duplicates?

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

    // remove self deps
    for (auto &c : cmds)
        c->dependencies.erase(c->shared_from_this());

    // 3. remove duplicates
    {
        // gather hashes
        std::unordered_map<size_t, T *> cmds3;
        cmds3.reserve(cmds.size());
        for (auto &c : cmds)
        {
            //if (cmds3[c->getHash()])
                //LOG_INFO(logger, "Duplicate commands!\n " << ((builder::Command*)cmds3[c->getHash()])->print());
                //throw SW_RUNTIME_ERROR("Duplicate commands!");
            cmds3[c->getHash()] = c;
        }

        auto replace = [&cmds3](const auto &d)
        {
            auto i = cmds3.find(d->getHash());
            SW_CHECK(i != cmds3.end());
            //if (i->second != d.get())
                return i->second->shared_from_this();
        };

        auto replace2 = [&replace](auto &a)
        {
            auto copy = a;
            a.clear();
            for (auto &d : copy)
                a.insert(replace(d));
        };

        // replace commands and set them back
        cmds.clear();
        cmds.reserve(cmds3.size());
        for (auto &[_,c] : cmds3)
        {
            replace2(c->dependencies);
            replace2(c->dependent_commands);
            cmds.insert(c);
        }
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
            auto n = std::count_if((*it)->dependencies.begin(), (*it)->dependencies.end(),
                [this, &cmds](auto &d) { return cmds.find(d.get()) != cmds.end(); });
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
        c->dependencies_left = c->dependencies.size();
        for (auto &d : c->dependencies)
            d->dependent_commands.insert(c->shared_from_this());
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
