// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "build.h"

#include "driver.h"
#include "input.h"
#include "sw_context.h"

#include <sw/builder/execution_plan.h>
#include <sw/builder/jumppad.h>
#include <sw/manager/storage.h>

#include <boost/current_function.hpp>
#include <magic_enum.hpp>
#include <nlohmann/json.hpp>
#include <primitives/date_time.h>
#include <primitives/executor.h>
#include <pugixml.hpp>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "build");

#define CHECK_STATE(from)                                                                 \
    if (state != from)                                                                    \
    throw SW_RUNTIME_ERROR("Unexpected build state = " + std::to_string(toIndex(state)) + \
                           ", expected = " + std::to_string(toIndex(from)))

#define CHECK_STATE_AND_CHANGE_RAW(from, to, scope_exit)          \
    if (stopped)                                                  \
        throw SW_RUNTIME_ERROR("Interrupted");                    \
    auto swctx_old_op = swctx.registerOperation((SwBuild *)this); \
    CHECK_STATE(from);                                            \
    scope_exit                                                    \
    {                                                             \
        swctx.registerOperation(swctx_old_op);                    \
        if (std::uncaught_exceptions() == 0)                      \
            state = to;                                           \
    };                                                            \
    LOG_TRACE(logger, "build id " << this << " performing " << BOOST_CURRENT_FUNCTION)

#define CHECK_STATE_AND_CHANGE(from, to) CHECK_STATE_AND_CHANGE_RAW(from, to, SCOPE_EXIT)

#define SW_CURRENT_LOCK_FILE_VERSION 1

namespace sw
{

static auto get_base_settings_version()
{
    // move this later to target settings?
    return 59;
}

static auto get_base_settings_name()
{
    return "settings." + std::to_string(get_base_settings_version());
}

static auto use_json()
{
    return true;
}

static auto get_settings_fn()
{
    return get_base_settings_name() + (use_json() ? ".json" : ".bin");
}

static auto create_target(const path &sfn, const LocalPackage &pkg, const TargetSettings &s)
{
    LOG_TRACE(logger, "loading " << pkg.toString() << ": " << s.getHash() << " from settings file");

    auto tgt = std::make_shared<PredefinedTarget>(pkg, s);
    if (!use_json())
        tgt->public_ts = loadSettings(sfn);
    else
    {
        TargetSettings its;
        its.mergeFromString(read_file(sfn));
        tgt->public_ts = its;
    }

    return tgt;
}

static std::shared_ptr<PredefinedTarget> create_target(const LocalPackage &p, const TargetSettings &s)
{
    auto cfg = s.getHash();
    auto base = p.getDirObj(cfg);
    auto sfn = base / get_settings_fn();
    if (fs::exists(sfn))
    {
        auto tgt = create_target(sfn, p, s);
        return tgt;
    }
    return {};
}

static auto can_use_saved_configs(const SwBuild &b)
{
    auto &s = b.getSettings();
    return true
        && s["use_saved_configs"] == "true"
        // allow only in the main build for now
        && s["master_build"] == "true"
        ;
}

static std::unordered_map<UnresolvedPackage, PackageId> loadLockFile(const path &fn/*, SwContext &swctx*/)
{
    auto j = nlohmann::json::parse(read_file(fn));
    if (j["schema"]["version"].is_null())
    {
        throw SW_RUNTIME_ERROR("Cannot use this lock file: unknown version, expected " + std::to_string(SW_CURRENT_LOCK_FILE_VERSION));
    }
    if (j["schema"]["version"] != SW_CURRENT_LOCK_FILE_VERSION)
    {
        throw SW_RUNTIME_ERROR("Cannot use this lock file: bad version " + std::to_string((int)j["schema"]["version"]) +
            ", expected " + std::to_string(SW_CURRENT_LOCK_FILE_VERSION));
    }

    std::unordered_map<UnresolvedPackage, PackageId> m;

    /*for (auto &v : j["packages"])
    {
        DownloadDependency d;
        (PackageId&)d = extractFromStringPackageId(v["package"].get<std::string>());
        d.createNames();
        d.prefix = v["prefix"];
        d.hash = v["hash"];
        d.group_number_from_lock_file = d.group_number = v["group_number"];
        auto i = overridden.find(d);
        d.local_override = i != overridden.end(d);
        if (d.local_override)
            d.group_number = i->second.getGroupNumber();
        d.from_lock_file = true;
        for (auto &v2 : v["dependencies"])
        {
            auto p = extractFromStringPackageId(v2.get<std::string>());
            DownloadDependency1 d2{ p };
            d.db_dependencies[p.ppath.toString()] = d2;
        }
        download_dependencies_.insert(d);
    }*/

    for (auto &v : j["resolved_packages"].items())
    {
        auto u = extractFromString(v.key());
        auto id = extractPackageIdFromString(v.value()["package"].get<std::string>());
        //LocalPackage d(swctx.getLocalStorage(), id);
        /*auto i = download_dependencies_.find(d);
        if (i == download_dependencies_.end())
            throw SW_RUNTIME_EXCEPTION("bad lock file");
        d = *i;
        if (v.value().find("installed") != v.value().end())
            d.installed = v.value()["installed"];*/
        m.emplace(u, id);
    }
    return m;
}

static void saveLockFile(const path &fn, const std::unordered_map<UnresolvedPackage, LocalPackage> &pkgs)
{
    nlohmann::json j;
    j["schema"]["version"] = SW_CURRENT_LOCK_FILE_VERSION;

    /*auto &jpkgs = j["packages"];
    for (auto &r : std::set<DownloadDependency>(download_dependencies_.begin(), download_dependencies_.end()))
    {
        nlohmann::json jp;
        jp["package"] = r.toString();
        jp["prefix"] = r.prefix;
        jp["hash"] = r.hash;
        if (r.group_number > 0)
            jp["group_number"] = r.group_number;
        else
            jp["group_number"] = r.group_number_from_lock_file;
        for (auto &[_, d] : std::map<String, DownloadDependency1>(r.db_dependencies.begin(), r.db_dependencies.end()))
            jp["dependencies"].push_back(d.toString());
        jpkgs.push_back(jp);
    }*/

    auto &jp = j["resolved_packages"];
    // sort
    for (auto &[u, r] : std::map<UnresolvedPackage, LocalPackage>(pkgs.begin(), pkgs.end()))
    {
        jp[u.toString()]["package"] = r.toString();
        //if (r.installed)
            //jp[u.toString()]["installed"] = true;
    }

    write_file_if_different(fn, j.dump(2));
}

static ExecutionPlan::Clock::duration parseTimeLimit(String tl)
{
    enum duration_type
    {
        none,
        day,
        hour,
        minute,
        second,
    };

    ExecutionPlan::Clock::duration d;

    size_t idx = 0, n;
    int t = none;
    while (1)
    {
        n = std::stoi(tl, &idx);
        if (tl[idx] == 0)
            break;
        int t0 = t;
        switch (tl[idx])
        {
        case 'd':
            d += std::chrono::hours(24 * n);
            t = day;
            break;
        case 'h':
            d += std::chrono::hours(n);
            t = hour;
            break;
        case 'm':
            d += std::chrono::minutes(n);
            t = minute;
            break;
        case 's':
            d += std::chrono::seconds(n);
            t = second;
            break;
        default:
            throw SW_RUNTIME_ERROR("Unknown duration specifier: '"s + tl[idx] + "'");
        }
        if (t < t0)
            throw SW_RUNTIME_ERROR("Bad duration specifier order");
        tl = tl.substr(idx + 1);
        if (tl.empty())
            break;
    }

    return d;
}

SwBuild::SwBuild(SwContext &swctx, const path &build_dir)
    : swctx(swctx)
    , build_dir(build_dir)
{
}

SwBuild::~SwBuild()
{
}

path SwBuild::getBuildDirectory() const
{
    return build_dir;
}

void SwBuild::stop()
{
    stopped = true;
    if (current_explan)
        current_explan->stop();
}

void SwBuild::build()
{
    /*
        General build process:
        1) Load provided inputs.
        2) Set all targets to build from input.
        3) Resolve dependencies.
        4) Load dependencies (inputs).
        5) Prepare build.
        6) Run build.

        ---

        Each package has exactly one entry point.
        Entry point may include several packages.
    */

    ScopedTime t;

    // this is all in one call
    while (step())
        ;

    if (build_settings["measure"] == "true")
        LOG_DEBUG(logger, BOOST_CURRENT_FUNCTION << " time: " << t.getTimeFloat() << " s.");
}

bool SwBuild::step()
{
    ScopedTime t;

    switch (state)
    {
    case BuildState::NotStarted:
        // load provided inputs
        loadInputs();
        break;
    case BuildState::InputsLoaded:
        setTargetsToBuild();
        break;
    case BuildState::TargetsToBuildSet:
        resolvePackages();
        break;
    case BuildState::PackagesResolved:
        loadPackages();
        break;
    case BuildState::PackagesLoaded:
        // prepare targets
        prepare();
        break;
    case BuildState::Prepared:
        // create ex. plan and execute it
        execute();
        break;
    default:
        return false;
    }

    if (build_settings["measure"] == "true")
        // not working atm: magic_enum bug
        //LOG_DEBUG(logger, "build step " << magic_enum::enum_name(state) << " time: " << t.getTimeFloat() << " s.");
        LOG_DEBUG(logger, "build step " << toIndex(state) << " time: " << t.getTimeFloat() << " s.");

    return true;
}

void SwBuild::overrideBuildState(BuildState s) const
{
    LOG_TRACE(logger, "build id " << this << " overriding state from " << toIndex(state) << " to " << toIndex(s));

    state = s;
}

void SwBuild::loadInputs()
{
    CHECK_STATE_AND_CHANGE(BuildState::NotStarted, BuildState::InputsLoaded);

    std::set<Input *> iv;
    for (auto &i : inputs)
        iv.insert(&i.getInput().getInput());
    swctx.loadEntryPointsBatch(iv);

    // and load packages
    for (auto &i : inputs)
    {
        auto tgts = i.loadTargets(*this);
        for (auto &tgt : tgts)
        {
            getTargets()[tgt->getPackage()].push_back(tgt);
            targets[tgt->getPackage()].setInput(i.getInput());
        }
    }
}

void SwBuild::setTargetsToBuild()
{
    CHECK_STATE_AND_CHANGE(BuildState::InputsLoaded, BuildState::TargetsToBuildSet);

    // mark existing targets as targets to build
    // only in case if not present?
    if (!targets_to_build.empty())
        return;
    targets_to_build = getTargets();
    for (auto &[pkg, d] : swctx.getPredefinedTargets())
        targets_to_build.erase(pkg.getPath());
}

void SwBuild::resolvePackages()
{
    CHECK_STATE_AND_CHANGE_RAW(BuildState::TargetsToBuildSet, BuildState::PackagesResolved, auto se = SCOPE_EXIT_NAMED);

    // gather
    std::vector<IDependency*> upkgs;
    for (const auto &[pkg, tgts] : getTargets())
    {
        for (const auto &tgt : tgts)
        {
            auto deps = tgt->getDependencies();
            for (auto &d : deps)
            {
                auto u = d->getUnresolvedPackage();

                // filter out existing targets as they come from same module
                // reconsider?
                if (auto id = u.toPackageId(); id && getTargets().find(*id) != getTargets().end())
                    continue;
                // filter out predefined targets
                if (swctx.getPredefinedTargets().find(u.getPath()) != swctx.getPredefinedTargets().end(u.getPath()))
                    continue;
                // filter out local targets
                if (u.getPath().isRelative() || u.getPath().is_loc())
                    continue;

                upkgs.push_back(d);
            }
            break; // take first as all deps are equal
        }
    }

    se.~ScopeGuard();
    resolvePackages(upkgs);
}

void SwBuild::resolvePackages(const std::vector<IDependency*> &udeps)
{
    CHECK_STATE_AND_CHANGE(BuildState::PackagesResolved, BuildState::PackagesResolved);

    // this is simple lock file: u->p
    //
    // more complex lock file will be
    // when we able to set dependency per each target with its settings
    bool must_update_lock_file = true;
    if (1
        && build_settings["update_lock_file"] != "true" // update flag
        && build_settings["lock_file"].isValue()
        && fs::exists(build_settings["lock_file"].getValue())
        )
    {
        must_update_lock_file = false; // no need to update, we are loading

        auto m = loadLockFile(build_settings["lock_file"].getValue());
        if (build_settings["update_lock_file_packages"])
        {
            for (auto &[u, p] : build_settings["update_lock_file_packages"].getMap())
            {
                m.erase(u);
                must_update_lock_file = true; // must update lock file here
            }
        }
        getContext().setCachedPackages(m);
        UnresolvedPackages upkgs;
        for (auto &[u, p] : m)
            upkgs.insert(p); // add exactly p, not u!
        swctx.install(upkgs, false);
    }

    UnresolvedPackages upkgs;
    for (auto &d : udeps)
        upkgs.insert(d->getUnresolvedPackage());

    // install
    std::unordered_map<UnresolvedPackage, LocalPackage> m;
    m.merge(swctx.install(upkgs));
    // mark packages as known right after resolve
    for (auto &[u, p] : m)
        targets[p];

    if (can_use_saved_configs(*this))
    {
        std::function<bool(const std::vector<IDependency*> &)> load_targets;
        load_targets = [this, &load_targets](const std::vector<IDependency*> &udeps)
        {
            bool everything_resolved = true;
            for (auto d : udeps)
            {
                auto i = swctx.getPredefinedTargets().find(d->getUnresolvedPackage());
                if (i != swctx.getPredefinedTargets().end())
                    continue;
                const auto &pi = targets.find(d->getUnresolvedPackage());
                if (pi == targets.end())
                {
                    everything_resolved = false;
                    continue;
                }
                auto p = LocalPackage(getContext().getLocalStorage(), pi->first);
                if (getTargets().find(p, d->getSettings()))
                    continue;
                auto tgt = create_target(p, d->getSettings());
                if (tgt)
                {
                    getTargets()[tgt->getPackage()].push_back(tgt);
                    everything_resolved &= load_targets(tgt->getDependencies());
                    continue;
                }
                everything_resolved = false;
            }
            return everything_resolved;
        };

        if (load_targets(udeps))
            return;
    }

    if (build_settings["lock_file"].isValue() && must_update_lock_file)
    {
        // TODO: update for deps changes
        SW_UNIMPLEMENTED;

        // show fancy diffs during update lock file
        if (build_settings["update_lock_file"] == "true")
        try
        {
            // may throw
            auto mold = loadLockFile(build_settings["lock_file"].getValue());
            for (auto &[u, p] : mold)
            {
                auto i = m.find(u);
                if (i == m.end())
                    LOG_INFO(logger, "Deleting dependency  : " + u.toString() + " (" + p.toString() + ")");
                else if (i->second != p)
                    LOG_INFO(logger, "Updating dependency  : " + u.toString() + " (" + p.toString() + " -> " + i->second.toString() + ")");
            }
            for (auto &[u, p] : m)
            {
                auto i = mold.find(u);
                if (i == mold.end())
                    LOG_INFO(logger, "Adding new dependency: " + u.toString() + " -> " + p.toString());
            }
        }
        catch (std::exception &)
        {
        }

        saveLockFile(build_settings["lock_file"].getValue(), m);
    }

    // now we know all drivers
    std::set<Input *> iv;
    for (auto &[u,p] : m)
    {
        // use addInput to prevent doubling already existing and loaded inputs
        // like when we loading dependency that is already loaded from the input
        // test: sw build org.sw.demo.gnome.pango.pangocairo-1.44
        auto i = addInput(p);
        iv.insert(&i.getInput());
        // this also marks package as known
        targets[p].setInput(i);
    }

    {
        ScopedTime t;
        swctx.loadEntryPointsBatch(iv);
        if (build_settings["measure"] == "true")
            LOG_DEBUG(logger, "load entry points time: " << t.getTimeFloat() << " s.");
    }
}

void SwBuild::loadPackages()
{
    CHECK_STATE_AND_CHANGE(BuildState::PackagesResolved, BuildState::PackagesLoaded);

    loadPackages(swctx.getPredefinedTargets());
}

void SwBuild::loadPackages(const TargetMap &predefined)
{
    // load
    auto usc = can_use_saved_configs(*this);
    //               input hash
    std::unordered_map<size_t, TargetMap> cache;
    int r = 1;
    while (!stopped)
    {
        LOG_TRACE(logger, "build id " << this << " " << BOOST_CURRENT_FUNCTION << " round " << r++);

        std::map<TargetSettings, std::pair<PackageId, TargetContainer *>> load;
        for (const auto &[pkg, tgts] : getTargets())
        {
            for (const auto &tgt : tgts)
            {
                auto deps = tgt->getDependencies();
                for (auto &d : deps)
                {
                    if (d->isResolved())
                        continue;

                    auto i = getTargets().find(d->getUnresolvedPackage());
                    if (i == getTargets().end())
                    {
                        auto i = predefined.find(d->getUnresolvedPackage());
                        if (i != predefined.end())
                        {
                            auto k = i->second.findSuitable(d->getSettings());
                            if (k != i->second.end())
                            {
                                d->setTarget(**k);
                                continue;
                            }

                            // package was not resolved
                            throw SW_RUNTIME_ERROR(tgt->getPackage().toString() + ": " + tgt->getSettings().toString() + ": predefined target is not resolved: " + d->getUnresolvedPackage().toString());
                        }

                        // package was not resolved
                        throw SW_RUNTIME_ERROR(tgt->getPackage().toString() + ": " + tgt->getSettings().toString() + ": No target resolved: " + d->getUnresolvedPackage().toString());
                    }

                    auto k = i->second.findSuitable(d->getSettings());
                    if (k != i->second.end())
                    {
                        d->setTarget(**k);
                        continue;
                    }

                    if (predefined.find(d->getUnresolvedPackage().ppath) != predefined.end(d->getUnresolvedPackage().ppath))
                    {
                        throw SW_LOGIC_ERROR(tgt->getPackage().toString() + ": " + tgt->getSettings().toString() + ": predefined target is not resolved: " + d->getUnresolvedPackage().toString());
                    }

                    load.insert({ d->getSettings(), { i->first, &i->second } });
                }
            }
        }
        if (load.empty())
            break;
        bool loaded = false;
        for (auto &[s, d] : load)
        {
            // empty settings mean we want dependency only to be present
            if (s.empty())
                continue;

            if (usc)
            {
                LocalPackage p(getContext().getLocalStorage(), d.first);
                auto tgt = create_target(p, s);
                if (tgt)
                {
                    getTargets()[tgt->getPackage()].push_back(tgt);
                    loaded = true;
                    continue;
                }
            }

            LOG_TRACE(logger, "build id " << this << " " << BOOST_CURRENT_FUNCTION << " loading " << d.first.toString());

            loaded = true;

            // from cache
            {
                // only if inputs the same
                // (we might change something in one of the inputs, do not take wrong targets from cache)
                auto h = getTargets()[d.first].getInput().getInput().getHash();
                auto i = cache[h].find(d.first);
                if (i != cache[h].end())
                {
                    auto k = i->second.findSuitable(s);
                    if (k != i->second.end())
                    {
                        auto &t = *k;
                        getTargets()[t->getPackage()].push_back(t);
                        continue;
                    }
                }
            }

            auto tgts = getTargets()[d.first].loadPackages(*this, s, getTargets().getPackagesSet());
            for (auto &tgt : tgts)
            {
                if (tgt->getPackage() == d.first)
                    getTargets()[tgt->getPackage()].push_back(tgt);
                else
                {
                    auto h = getTargets()[d.first].getInput().getInput().getHash();
                    cache[h][tgt->getPackage()].push_back(tgt);
                }
            }

            auto k = d.second->findSuitable(s);
            if (k == d.second->end())
            {
                String e;
                e += d.first.toString() + " with current settings\n" + s.toString();
                e += "\navailable targets:\n";
                for (auto &tgt : tgts)
                    e += tgt->getSettings().toString() + "\n";
                e.resize(e.size() - 1);
                throw SW_RUNTIME_ERROR("cannot load package " + e);
            }
        }
        if (!loaded)
            break;
    }
}

bool SwBuild::prepareStep()
{
    std::atomic_bool next_pass = false;

    auto &e = getPrepareExecutor();
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

void SwBuild::prepare()
{
    CHECK_STATE_AND_CHANGE(BuildState::PackagesLoaded, BuildState::Prepared);

    while (prepareStep() && !stopped)
        ;
    if (stopped)
        return;

    if (build_settings["master_build"] != "true")
        return;

    // save after prepare
    for (const auto &[pkg, tgts] : targets)
    {
        if (!pkg.getPath().isAbsolute())
            continue;
        for (auto &tgt : tgts)
        {
            LocalPackage p(getContext().getLocalStorage(), tgt->getPackage());
            if (p.isOverridden())
                continue;
            // skip predefs - they are already readed from disk or created in sw
            if (tgt->as<const PredefinedTarget *>())
                continue;
            auto cfg = tgt->getSettings().getHash();
            auto base = p.getDirObj(cfg);
            auto sfn = base / get_settings_fn();
            auto sfncfg = base / get_base_settings_name() += ".cfg";
            auto sptrfn = base / "settings.hash";

            if (!fs::exists(sfn) || !fs::exists(sptrfn) || read_file(sptrfn) != tgt->getInterfaceSettings().getHash())
            {
                if (!use_json())
                    saveSettings(sfn, tgt->getInterfaceSettings());
                else
                {
                    write_file(sfn, nlohmann::json::parse(tgt->getInterfaceSettings().toString()).dump(2));
                    write_file(sfncfg, nlohmann::json::parse(tgt->getSettings().toString()).dump(2));
                }
                write_file(sptrfn, tgt->getInterfaceSettings().getHash());
            }
        }
    }
}

void SwBuild::execute() const
{
    auto p = getExecutionPlan();
    execute(*p);
}

void SwBuild::execute(ExecutionPlan &p) const
{
    CHECK_STATE_AND_CHANGE(BuildState::Prepared, BuildState::Executed);

    SwapAndRestore sr(current_explan, &p);

    p.build_always |= build_settings["build_always"] == "true";
    p.write_output_to_file |= build_settings["write_output_to_file"] == "true";
    if (build_settings["skip_errors"].isValue())
        p.skip_errors = std::stoll(build_settings["skip_errors"].getValue());
    if (build_settings["time_limit"].isValue())
        p.setTimeLimit(parseTimeLimit(build_settings["time_limit"].getValue()));

    ScopedTime t;
    p.execute(getBuildExecutor());
    if (build_settings["measure"] == "true")
        LOG_DEBUG(logger, BOOST_CURRENT_FUNCTION << " time: " << t.getTimeFloat() << " s.");

    if (build_settings["time_trace"] == "true")
        p.saveChromeTrace(getBuildDirectory() / "misc" / "time_trace.json");

    path ide_fast_path = build_settings["build_ide_fast_path"].isValue() ? build_settings["build_ide_fast_path"].getValue() : "";
    if (!ide_fast_path.empty())
    {
        String s;
        for (auto &f : fast_path_files)
            s += to_string(normalize_path(f)) + "\n";
        write_file(ide_fast_path, s);

        uint64_t mtime = 0;
        for (auto &f : fast_path_files)
        {
            auto lwt = fs::last_write_time(f);
            mtime ^= file_time_type2time_t(lwt);
        }
        path fmtime = ide_fast_path;
        fmtime += ".t";
        write_file(fmtime, std::to_string(mtime));
    }
}

Commands SwBuild::getCommands() const
{
    // calling this for all targets in any case to set proper command dependencies
    for (const auto &[pkg, tgts] : getTargets())
    {
        for (auto &tgt : tgts)
            tgt->getCommands();
    }

    if (targets_to_build.empty())
        throw SW_RUNTIME_ERROR("no targets were selected for building");

    StringSet in_ttb;
    StringSet in_ttb_exclude;
    for (auto &t : build_settings["target-to-build"].getArray())
        in_ttb.insert(t.getValue());
    for (auto &t : build_settings["target-to-exclude"].getArray())
    {
        if (in_ttb.find(t.getValue()) != in_ttb.end())
            throw SW_RUNTIME_ERROR("Target " + t.getValue() + " specified both in include and exclude lists");
        in_ttb_exclude.insert(t.getValue());
    }
    bool in_ttb_used = !in_ttb.empty();

    decltype(targets_to_build) ttb;

    // detect all targets to build
    // some static builds won't build deps, because there's no dependent link files
    // (e.g. build static png, zlib won't be built)
    for (auto &[p, tgts] : targets_to_build)
    {
        if (in_ttb_used)
        {
            auto i = in_ttb.find(p.toString());
            if (i == in_ttb.end())
                continue;
            in_ttb.erase(i);
        }
        if (in_ttb_exclude.find(p.toString()) != in_ttb_exclude.end())
        {
            continue;
        }

        ttb.emplace(p, tgts);

        for (auto &tgt : tgts)
        {
            // gather targets to build
            const auto &s = tgt->getInterfaceSettings();

            // skip prebuilt
            //if (auto t = tgt->as<const PredefinedTarget *>())
                //continue;

            std::function<void(const TargetSettings &)> gather_ttb;
            gather_ttb = [this, &gather_ttb, &ttb](const auto &s) mutable
            {
                if (s["header_only"] == "true")
                    return;

                if (!(s["type"] == "native_shared_library" || s["type"] == "native_static_library" || s["type"] == "native_executable"))
                    return;

                std::function<void(const TargetSettings &)> process_deps;
                process_deps = [this, &gather_ttb, &process_deps, &ttb](const auto &s) mutable
                {
                    auto get_deps = [this, &gather_ttb, &process_deps, &ttb](const auto &in)
                    {
                        for (auto &[k, v] : in)
                        {
                            if (swctx.getPredefinedTargets().find(PackageId(k)) != swctx.getPredefinedTargets().end())
                                continue;
                            auto i = getTargets().find(PackageId(k));
                            if (i == getTargets().end())
                                throw SW_RUNTIME_ERROR("dep not found: " + k);
                            auto j = i->second.findSuitable(v.getMap());
                            if (j == i->second.end())
                            {
                                LOG_TRACE(logger, "dep+settings not found: " + k + ": " + v.getMap().toString());
                                continue; // probably was loaded config
                                //throw SW_RUNTIME_ERROR("dep+settings not found: " + k + ": " + v.getMap().toString());
                            }

                            auto m = ttb[PackageId(k)].findEqual((*j)->getSettings());
                            if (m != ttb[PackageId(k)].end())
                                continue;
                            ttb[PackageId(k)].push_back(*j);

                            const auto &s = (*j)->getInterfaceSettings();
                            gather_ttb(s);
                            process_deps(s);
                        }
                    };

                    get_deps(s["dependencies"]["link"].getMap());
                    get_deps(s["dependencies"]["dummy"].getMap());
                };

                process_deps(s);
            };

            gather_ttb(s);
        }
    }

    if (!in_ttb.empty())
    {
        String s;
        for (auto &t : in_ttb)
            s += t + ", ";
        s.resize(s.size() - 2);
        throw SW_RUNTIME_ERROR("Cannot make targets: " + s + ": no such targets");
    }

    // update public ttb
    // reconsider? remove?
    targets_to_build = ttb;

    //
    auto cl_show_output = build_settings["show_output"] == "true";
    auto cl_write_output_to_file = build_settings["write_output_to_file"] == "true";

    // gather commands
    Commands cmds;
    for (auto &[p, tgts] : ttb)
    {
        for (auto &tgt : tgts)
        {
            auto c = tgt->getCommands();
            for (auto &c2 : c)
            {
                c2->show_output = cl_show_output || cl_write_output_to_file; // only for selected targets
            }
            cmds.insert(c.begin(), c.end());
        }
    }

    // copy output files
    path copy_dir = build_settings["build_ide_copy_to_dir"].isValue() ? build_settings["build_ide_copy_to_dir"].getValue() : "";
    {
        std::unordered_map<path, path> copy_files;
        for (auto &[p, tgts] : ttb)
        {
            for (auto &tgt : tgts)
            {
                const auto &s = tgt->getInterfaceSettings();

                // do not copy such deps
                // example: when processing qt plugins, without the condition we'll copy
                // main dlls to plugins dir which is not desirable
                if (s["output_dir"].isValue())
                    continue;

                bool copy_ok = 0
                    || tgt->getSettings()["os"]["kernel"] == "com.Microsoft.Windows.NT"
                    || tgt->getSettings()["os"]["kernel"] == "org.cygwin"
                    || tgt->getSettings()["os"]["kernel"] == "org.mingw"
                    ;
                if (!copy_ok)
                    continue;

                auto copy_dir_current = copy_dir;
                // copy only for local targets
                if (copy_dir_current.empty())
                {
                    if (p.getPath().isAbsolute())
                        continue;
                    if (s["header_only"] == "true")
                        continue;
                    if (!(s["type"] == "native_shared_library" || s["type"] == "native_static_library" || s["type"] == "native_executable"))
                        continue;
                    copy_dir_current = s["output_file"].getPathValue(getContext().getLocalStorage()).parent_path();
                }

                PackageIdSet visited_pkgs;
                std::function<void(const TargetSettings &)> copy_file;
                copy_file = [this, &copy_dir_current, &copy_files, &copy_file, &visited_pkgs](const auto &s)
                {
                    if (s["header_only"] == "true")
                        return;

                    if (!(s["type"] == "native_shared_library" || s["type"] == "native_static_library" || s["type"] == "native_executable"))
                        return;

                    auto in = s["output_file"].getPathValue(getContext().getLocalStorage());
                    fast_path_files.insert(in);

                    if (s["import_library"].isValue())
                    {
                        path il = s["import_library"].getPathValue(getContext().getLocalStorage());
                        fast_path_files.insert(il);
                    }

                    if (s["type"] == "native_shared_library"
                        // copy only for wintgt?
                        //&& PackagePath(s["os"]["kernel"].getValue()) == PackagePath("com.Microsoft.Windows.NT")
                        )
                    {
                        auto o = copy_dir_current;
                        if (s["output_dir"].isValue())
                            o /= s["output_dir"].getValue();
                        o /= in.filename();
                        if (in == o)
                            return;
                        if (copy_files.find(in) != copy_files.end())
                            return;
                        copy_files[in] = o;
                        fast_path_files.insert(o);
                    }

                    std::function<void(const TargetSettings &)> process_deps;
                    process_deps = [this, &copy_file, &process_deps, &visited_pkgs](const auto &s)
                    {
                        for (auto &[k, v] : s["dependencies"]["link"].getMap())
                        {
                            PackageId p(k);
                            if (visited_pkgs.find(p) != visited_pkgs.end())
                                continue;
                            visited_pkgs.insert(p);
                            auto i = getTargets().find(p);
                            if (i == getTargets().end())
                                throw SW_RUNTIME_ERROR("dep not found");
                            auto j = i->second.findSuitable(v.getMap());
                            if (j == i->second.end())
                                throw SW_RUNTIME_ERROR("dep+settings not found");

                            const auto &s = (*j)->getInterfaceSettings();
                            copy_file(s);
                            process_deps(s);
                        }
                    };

                    process_deps(s);
                };

                copy_file(s);
            }
        }

        for (auto &[f, t] : copy_files)
        {
            auto copy_cmd = std::make_shared<::sw::builder::BuiltinCommand>(*this, SW_VISIBLE_BUILTIN_FUNCTION(copy_file));
            copy_cmd->arguments.push_back(f);
            copy_cmd->arguments.push_back(t);
            copy_cmd->addInput(f);
            copy_cmd->addOutput(t);
            //copy_cmd->dependencies.insert(nt->getCommand());
            copy_cmd->name = "copy: " + to_string(normalize_path(t));
            copy_cmd->command_storage = &getCommandStorage(getBuildDirectory() / "cs");
            cmds.insert(copy_cmd);
            commands_storage.insert(copy_cmd); // prevents early destruction
        }
    }

    return cmds;
}

std::unique_ptr<ExecutionPlan> SwBuild::getExecutionPlan() const
{
    return getExecutionPlan(getCommands());
}

std::unique_ptr<ExecutionPlan> SwBuild::getExecutionPlan(const Commands &cmds) const
{
    auto ep = ExecutionPlan::create(cmds);
    if (ep->isValid())
        return std::move(ep);

    // error!

    auto d = getBuildDirectory() / "misc";

    auto [g, n, sc] = ep->getStrongComponents();

    using Subgraph = boost::subgraph<ExecutionPlan::Graph>;

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
            ExecutionPlan::printGraph(subs[i]->m_graph, cyclic_path / ("cycle_" + std::to_string(i)));
    }

    ep->printGraph(ep->getGraph(), cyclic_path / "processed", ep->getCommands(), true);
    ep->printGraph(ep->getGraphUnprocessed(), cyclic_path / "unprocessed", ep->getUnprocessedCommands(), true);

    String error = "Cannot create execution plan because of cyclic dependencies";
    //String error = "Cannot create execution plan because of cyclic dependencies: strong components = " + std::to_string(n);

    throw SW_RUNTIME_ERROR(error);
}

String SwBuild::getHash() const
{
    String s;
    for (auto &i : inputs)
        s += i.getHash();
    return shorten_hash(blake2b_512(s), 8);
}

void SwBuild::setName(const String &n)
{
    if (!name.empty())
        throw SW_RUNTIME_ERROR("Cannot set build name twice");
    name = n;
}

String SwBuild::getName() const
{
    if (!name.empty())
        return name;
    return getHash();
}

void SwBuild::addInput(const InputWithSettings &i)
{
    inputs.push_back(i);
}

std::vector<BuildInput> SwBuild::addInput(const String &i)
{
    path p(i);
    if (fs::exists(p))
        return addInput(p);
    else
    {
        try
        {
            auto p = extractFromString(i);
            auto bi = addInput(getContext().resolve(p));
            std::vector<BuildInput> v;
            v.push_back(bi);
            return v;
        }
        catch (std::exception &e)
        {
            throw SW_RUNTIME_ERROR("No such file, directory or suitable package: " + i + ": " + e.what());
        }
    }
}

BuildInput SwBuild::addInput(const LocalPackage &p)
{
    LOG_TRACE(logger, "Loading input: " + p.toString());

    auto v = addInput(p.getDirSrc2());
    //SW_CHECK(v.size() == 1); // allow multiple inputs for now, take only first
    v[0].addPackage(p);
    return v[0];
}

std::vector<BuildInput> SwBuild::addInput(const path &p)
{
    auto v = getContext().addInputInternal(p);
    std::vector<BuildInput> inputs;
    for (auto i : v)
        inputs.emplace_back(*i);
    return inputs;
}

path SwBuild::getExecutionPlanPath() const
{
    const auto ext = ".swb"; // sw build
    return getBuildDirectory() / "ep" / getName() += ext;
}

void SwBuild::saveExecutionPlan() const
{
    saveExecutionPlan(getExecutionPlanPath());
}

void SwBuild::runSavedExecutionPlan() const
{
    CHECK_STATE(BuildState::InputsLoaded);

    runSavedExecutionPlan(getExecutionPlanPath());
}

void SwBuild::saveExecutionPlan(const path &in) const
{
    CHECK_STATE(BuildState::Prepared);

    auto p = getExecutionPlan();
    p->save(in);
}

void SwBuild::runSavedExecutionPlan(const path &in) const
{
    auto cmds = ExecutionPlan::load(in, *this);
    auto p = ExecutionPlan::create(cmds);

    // change state
    overrideBuildState(BuildState::Prepared);
    SCOPE_EXIT
    {
        // fallback
        overrideBuildState(BuildState::InputsLoaded);
    };

    execute(*p);
}

const std::vector<InputWithSettings> &SwBuild::getInputs() const
{
    return inputs;
}

void SwBuild::setSettings(const TargetSettings &bs)
{
    build_settings = bs;

    if (build_settings["build-jobs"])
        build_executor = std::make_unique<Executor>(std::stoi(build_settings["build-jobs"].getValue()));
    if (build_settings["prepare-jobs"])
        prepare_executor = std::make_unique<Executor>(std::stoi(build_settings["prepare-jobs"].getValue()));
}

Executor &SwBuild::getBuildExecutor() const
{
    if (build_executor)
        return *build_executor;
    return ::getExecutor();
}

Executor &SwBuild::getPrepareExecutor() const
{
    if (prepare_executor)
        return *prepare_executor;
    return ::getExecutor();
}

const TargetSettings &SwBuild::getExternalVariables() const
{
    return getSettings()["D"].getMap();
}

path SwBuild::getTestDir() const
{
    return getBuildDirectory() / "test";
}

void SwBuild::test()
{
    build();

    auto dir = getTestDir();

    // remove only test dirs for active configs
    Files tdirs;
    for (const auto &[pkg, tgts] : getTargetsToBuild())
    {
        for (auto &tgt : tgts)
        {
            auto test_dir = dir / tgt->getSettings().getHash();
            tdirs.insert(test_dir);
        }
    }
    for (auto &d : tdirs)
        fs::remove_all(d);

    // prepare
    struct data
    {
        path dir;
        String suite;
        String config;
        String name;
    };
    std::unordered_map<builder::Command *, data> test_data;
    for (const auto &[pkg, tgts] : getTargetsToBuild())
    {
        for (auto &tgt : tgts)
        {
            for (auto &c : tgt->getTests())
            {
                auto test_dir_name = c->getName();
                // don't go deeper for now?
                boost::replace_all(test_dir_name, "/", ".");
                boost::replace_all(test_dir_name, "\\", ".");
                auto test_dir = dir / tgt->getSettings().getHash() / tgt->getPackage().toString() / test_dir_name;
                test_data[c.get()].dir = test_dir;
                test_data[c.get()].config = tgt->getSettings().getHash();
                test_data[c.get()].suite = tgt->getPackage().toString();
                test_data[c.get()].name = c->name;
                auto wdir = test_dir / "wdir";
                fs::create_directories(wdir);

                //
                c->name = "test: [" + tgt->getPackage().toString() + "]/[" + tgt->getSettings().getHash() + "]/[" + c->name + "]";
                c->always = true;
                c->working_directory = wdir;
                //c.addPathDirectory(BinaryDir / getSettings().getConfig());
                c->out.file = test_dir / "stdout.txt";
                c->err.file = test_dir / "stderr.txt";
            }
        }
    }

    // gather commands
    Commands cmds;
    for (const auto &[pkg, tgts] : getTargetsToBuild())
    {
        for (auto &tgt : tgts)
        {
            auto c = tgt->getTests();
            cmds.insert(c.begin(), c.end());
        }
    }

    auto ep = getExecutionPlan(cmds);
    ep->throw_on_errors = false;
    ep->skip_errors = cmds.size();
    ep->execute(getBuildExecutor());

    // record time
    for (const auto &[c, d] : test_data)
    {
        if (!fs::exists(d.dir))
            continue;
        std::ofstream ofile(d.dir / "time.txt");
        ofile.precision(10);
        ofile << std::chrono::duration_cast<std::chrono::duration<double>>(c->t_end - c->t_begin).count();

        if (c->exit_code)
            write_file(d.dir / "exit_code.txt", std::to_string(*c->exit_code));
    }

    // count
    int skipped = 0;
    int errors = 0;
    for (auto &c : cmds)
    {
        if (c->skip)
            skipped++;
        else if (!c->exit_code || c->exit_code != 0)
            errors++;
    }

    // print
    LOG_INFO(logger, "");
    LOG_INFO(logger, "Test results:");
    LOG_INFO(logger, "TOTAL:   " << cmds.size());
    LOG_INFO(logger, "PASSED:  " << cmds.size() - errors - skipped);
    LOG_INFO(logger, "FAILED:  " << errors);
    LOG_INFO(logger, "SKIPPED: " << skipped);
/*
    // from gnu testsuite?
    # TOTAL: 611
    # PASS: 144
    # SKIP: 86
    # XFAIL: 0
    # FAIL: 342
    # XPASS: 0
    # ERROR: 39
*/
    if (errors)
    {
        LOG_INFO(logger, "");
        LOG_INFO(logger, "List of failed tests:");
        for (auto &c : cmds)
        {
            if (c->skip)
                continue;
            if (c->exit_code && c->exit_code == 0)
                continue;
            LOG_INFO(logger, c->name);
        }
    }
    if (skipped)
    {
        LOG_INFO(logger, "");
        LOG_INFO(logger, "List of skipped tests:");
        for (auto &c : cmds)
        {
            if (c->skip)
                LOG_INFO(logger, c->name);
        }
    }

    //
    enum TestReportFormat
    {
        Sw,
        // currently default
        // for format see https://llg.cubic.org/docs/junit/
        JUnit,
    };
    auto format = TestReportFormat::JUnit;

    pugi::xml_document doc;
    auto suites = doc.append_child("testsuites");
    struct suite_data
    {
        pugi::xml_node node;
        int nall = 0;
        int nok = 0;
        int nskipped = 0;
        int nfailed = 0;
        int nerrors = 0;
        double time = 0;
    };
    std::unordered_map<String, suite_data> suitesmap;
    for (auto &c : cmds)
    {
        auto &d = test_data[c.get()];
        if (suitesmap.find(d.suite) == suitesmap.end())
        {
            auto suite = suitesmap[d.suite.c_str()].node = suites.append_child("testsuite");
            suite.append_attribute("name").set_value(d.suite.c_str());
            suite.append_attribute("package").set_value(d.suite.c_str());
            suite.append_attribute("config").set_value(d.config.c_str());
        }
        auto &suite = suitesmap[d.suite.c_str()];
        auto testcase = suite.node.append_child("testcase");
        testcase.append_attribute("name").set_value(d.name.c_str());
        testcase.append_attribute("config").set_value(d.config.c_str());
        suite.nall++;
        if (c->skip)
        {
            suite.nskipped++;
            testcase.append_child("skipped");
            continue;
        }
        auto t = std::chrono::duration_cast<std::chrono::duration<double>>(c->t_end - c->t_begin).count();
        suite.time += t;
        testcase.append_attribute("time").set_value(read_file(d.dir / "time.txt").c_str()); // read from file to keep precision
        if (c->exit_code && c->exit_code == 0)
        {
            suite.nok++;
            continue;
        }
        testcase.append_child("system-out")
            //.append_child(pugi::node_cdata)
            .text().set(read_file(d.dir / "stdout.txt").c_str());
        testcase.append_child("system-err")
            //.append_child(pugi::node_cdata)
            .text().set(read_file(d.dir / "stderr.txt").c_str());
        auto e = testcase.append_child("failure");
        e.append_attribute("message").set_value(("error code = "s + std::to_string(*c->exit_code)).c_str());
        suite.nfailed++;
    }
    for (auto &[_, s] : suitesmap)
    {
        s.node.append_attribute("time").set_value(std::to_string(s.time).c_str());
        s.node.append_attribute("tests").set_value(std::to_string(s.nall).c_str());
        s.node.append_attribute("skipped").set_value(std::to_string(s.nskipped).c_str());
        s.node.append_attribute("errors").set_value(std::to_string(s.nerrors).c_str());
        s.node.append_attribute("failures").set_value(std::to_string(s.nfailed).c_str());
    }
    suites.append_attribute("time").set_value(std::to_string(
        std::accumulate(suitesmap.begin(), suitesmap.end(), 0.0, [](auto current_sum, auto &value) { return current_sum + value.second.time; })).c_str());
    suites.append_attribute("tests").set_value(std::to_string(
        std::accumulate(suitesmap.begin(), suitesmap.end(), 0, [](auto current_sum, auto &value) { return current_sum + value.second.nall; })).c_str());
    suites.append_attribute("skipped").set_value(std::to_string(
        std::accumulate(suitesmap.begin(), suitesmap.end(), 0, [](auto current_sum, auto &value) { return current_sum + value.second.nskipped; })).c_str());
    suites.append_attribute("errors").set_value(std::to_string(
        std::accumulate(suitesmap.begin(), suitesmap.end(), 0, [](auto current_sum, auto &value) { return current_sum + value.second.nerrors; })).c_str());
    suites.append_attribute("failures").set_value(std::to_string(
        std::accumulate(suitesmap.begin(), suitesmap.end(), 0, [](auto current_sum, auto &value) { return current_sum + value.second.nfailed; })).c_str());

    auto fn = getBuildDirectory() / "test" / "results.xml";
    std::ofstream ofile(fn);
    if (!ofile)
        throw SW_RUNTIME_ERROR("Cannot save test results to " + to_printable_string(fn));
    doc.save(ofile);

    if (errors)
        throw SW_RUNTIME_ERROR("Some tests failed");
}

}
