// Copyright (C) 2016-2019 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "build.h"

#include "driver.h"
#include "input.h"
#include "sw_context.h"

#include <sw/builder/execution_plan.h>

#include <boost/current_function.hpp>
#include <magic_enum.hpp>
#include <nlohmann/json.hpp>
#include <primitives/date_time.h>
#include <primitives/executor.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "build");

#define CHECK_STATE(from)                                                                 \
    if (state != from)                                                                    \
    throw SW_RUNTIME_ERROR("Unexpected build state = " + std::to_string(toIndex(state)) + \
                           ", expected = " + std::to_string(toIndex(from)))

#define CHECK_STATE_AND_CHANGE_RAW(from, to, scope_exit) \
    CHECK_STATE(from);                                   \
    scope_exit                                           \
    {                                                    \
        if (std::uncaught_exceptions() == 0)             \
            state = to;                                  \
    };                                                   \
    LOG_TRACE(logger, "build id " << this << " performing " << BOOST_CURRENT_FUNCTION)

#define CHECK_STATE_AND_CHANGE(from, to) CHECK_STATE_AND_CHANGE_RAW(from, to, SCOPE_EXIT)

#define SW_CURRENT_LOCK_FILE_VERSION 1

namespace sw
{

std::unordered_map<UnresolvedPackage, PackageId> loadLockFile(const path &fn/*, SwContext &swctx*/)
{
    auto j = nlohmann::json::parse(read_file(fn));
    if (j["schema"]["version"] != SW_CURRENT_LOCK_FILE_VERSION)
    {
        throw SW_RUNTIME_ERROR("Cannot use this lock file: bad version " + std::to_string((int)j["version"]) +
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

void SwBuild::build()
{
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
        LOG_DEBUG(logger, "build step " << magic_enum::enum_name(state) << " time: " << t.getTimeFloat() << " s.");

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
        iv.insert((Input*)&i.getInput());
    swctx.loadEntryPoints(iv, true);

    // and load packages
    for (auto &i : inputs)
    {
        auto tgts = i.loadTargets(*this);
        for (auto &tgt : tgts)
        {
            if (tgt->getSettings()["dry-run"] == "true")
                continue;
            addKnownPackage(tgt->getPackage()); // also mark them as known
            getTargets()[tgt->getPackage()].push_back(tgt);
        }
    }
}

const PackageIdSet &SwBuild::getKnownPackages() const
{
    return known_packages;
}

void SwBuild::addKnownPackage(const PackageId &id)
{
    known_packages.insert(id);
}

void SwBuild::resolvePackages()
{
    CHECK_STATE_AND_CHANGE_RAW(BuildState::TargetsToBuildSet, BuildState::PackagesResolved, auto se = SCOPE_EXIT_NAMED);

    // gather
    UnresolvedPackages upkgs;
    // remove first loop?
    // we have already loaded inputs
    /*for (const auto &[pkg, tgts] : getTargetsToBuild())
    {
        for (const auto &tgt : tgts)
        {
            // for package id inputs we also load themselves
            auto pkg = tgt->getPackage();
            //                                skip checks
            if (pkg.getPath().isAbsolute() && !pkg.getPath().is_loc())
                upkgs.insert(pkg);
            break;
        }
    }*/
    for (const auto &[pkg, tgts] : getTargets())
    {
        for (const auto &tgt : tgts)
        {
            auto deps = tgt->getDependencies();
            for (auto &d : deps)
            {
                // filter out existing targets as they come from same module
                // reconsider?
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

    se.~ScopeGuard();
    resolvePackages(upkgs);
}

void SwBuild::resolvePackages(const UnresolvedPackages &upkgs)
{
    CHECK_STATE_AND_CHANGE(BuildState::PackagesResolved, BuildState::PackagesResolved);

    // this is simple lock file: u->p
    //
    // more complex lock file will be
    // when we able to set dependency per each target with its settings
    bool should_update_lock_file = true;
    if (1
        && build_settings["update_lock_file"] != "true" // update flag
        && build_settings["lock_file"].isValue()
        && fs::exists(build_settings["lock_file"].getValue())
        )
    {
        should_update_lock_file = false; // no need to update, we are loading

        auto m = loadLockFile(build_settings["lock_file"].getValue()/*, getContext()*/);
        if (build_settings["update_lock_file_packages"])
        {
            for (auto &[u, p] : build_settings["update_lock_file_packages"].getSettings())
            {
                m.erase(u);
                should_update_lock_file = true; // must update lock file
            }
        }
        getContext().setCachedPackages(m);
        UnresolvedPackages upkgs;
        for (auto &[u, p] : m)
            upkgs.insert(p); // add exactly p, not u!
        swctx.install(upkgs, false);
    }

    // install
    auto m = swctx.install(upkgs);
    for (auto &[_, p] : m)
        addKnownPackage(p);

    if (build_settings["lock_file"].isValue() && should_update_lock_file)
    {
        saveLockFile(build_settings["lock_file"].getValue(), m);
    }

    // now we know all drivers
    std::set<Input *> iv;
    for (auto &[u, p] : m)
    {
        // use addInput to prevent doubling already existing and loaded inputs
        // like when we loading dependency that is already loaded from the input
        // test: sw build org.sw.demo.gnome.pango.pangocairo-1.44
        iv.insert(&swctx.addInput(p));
    }
    swctx.loadEntryPoints(iv, false);
}

void SwBuild::loadPackages()
{
    CHECK_STATE_AND_CHANGE(BuildState::PackagesResolved, BuildState::PackagesLoaded);

    loadPackages(swctx.getPredefinedTargets());
}

void SwBuild::loadPackages(const TargetMap &predefined)
{
    // first, we create all package ids with EPs in targets
    //for (auto &[p, _] : swctx.getTargetData())
    for (auto &p : getKnownPackages())
        targets[p];

    // load
    int r = 1;
    while (1)
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
                        }

                        // package was not resolved
                        throw SW_RUNTIME_ERROR(tgt->getPackage().toString() + ": No target resolved: " + d->getUnresolvedPackage().toString());
                    }

                    auto k = i->second.findSuitable(d->getSettings());
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
        bool loaded = false;
        for (auto &[s, d] : load)
        {
            // empty settings mean we want dependency only to be present
            if (s.empty())
                continue;

            if (build_settings["use_saved_configs"] == "true" &&
                build_settings["master_build"] == "true") // allow only in the main build for now
            {
                LocalPackage p(getContext().getLocalStorage(), d.first);
                auto cfg = s.getHash();
                auto base = p.getDirObj(cfg);
#define BASE_SETTINGS "settings.16"
#define USE_JSON
#ifndef USE_JSON
#define SETTINGS_FN BASE_SETTINGS ".bin"
#else
#define SETTINGS_FN BASE_SETTINGS ".json"
#endif
                auto sfn = base / SETTINGS_FN;
                if (fs::exists(sfn))
                {
                    LOG_TRACE(logger, "loading " << d.first.toString() << ": " << s.getHash() << " from settings file");

                    auto tgt = std::make_shared<PredefinedTarget>(d.first, s);
#ifndef USE_JSON
                    tgt->public_ts = loadSettings(sfn);
#else
                    TargetSettings its;
                    its.mergeFromString(read_file(sfn));
                    tgt->public_ts = its;
#endif
                    getTargets()[tgt->getPackage()].push_back(tgt);
                    loaded = true;
                    continue;
                }
            }

            LOG_TRACE(logger, "build id " << this << " " << BOOST_CURRENT_FUNCTION << " loading " << d.first.toString());

            loaded = true;

            auto ep = getEntryPoint(d.first);
            if (!ep)
                throw SW_RUNTIME_ERROR("no entry point for " + d.first.toString());
            auto pp = d.first.getPath().slice(0, LocalPackage(getContext().getLocalStorage(), d.first).getData().prefix);
            //auto tgts = ep->loadPackages(*this, s, { d.first }, pp);
            auto tgts = ep->loadPackages(*this, s, known_packages, pp);

            bool added = false;
            for (auto &tgt : tgts)
            {
                if (tgt->getSettings()["dry-run"] == "true")
                    continue;
                getTargets()[tgt->getPackage()].push_back(tgt);
                added = true;
            }

            auto k = d.second->findSuitable(s);
            if (k == d.second->end())
            {
                String e;
                e += d.first.toString() + " with current settings\n" + s.toString();
                e += "\navailable targets:\n";
                for (auto &tgt : tgts)
                {
                    if (tgt->getSettings()["dry-run"] == "true")
                        continue;
                    e += tgt->getSettings().toString() + "\n";
                }
                e.resize(e.size() - 1);

                // We add this check inside if (k == d.second->end()) condition,
                // because 'load' variable may contain more than 1 request
                // and needed target will be loaded with another (previous) one.
                // So, added check will not pass, but k == d.second->end() passes.

                // assert in fact
                if (!added)
                    throw SW_LOGIC_ERROR("no packages loaded " + e);

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
    CHECK_STATE_AND_CHANGE(BuildState::InputsLoaded, BuildState::TargetsToBuildSet);

    // mark existing targets as targets to build
    // only in case if not present?
    if (!targets_to_build.empty())
        return;
    targets_to_build = getTargets();
    for (auto &[pkg, d] : swctx.getPredefinedTargets())
        targets_to_build.erase(pkg.getPath());
}

void SwBuild::prepare()
{
    CHECK_STATE_AND_CHANGE(BuildState::PackagesLoaded, BuildState::Prepared);

    while (prepareStep())
        ;
}

void SwBuild::execute() const
{
    auto p = getExecutionPlan();
    execute(p);
}

void SwBuild::execute(ExecutionPlan &p) const
{
    CHECK_STATE_AND_CHANGE(BuildState::Prepared, BuildState::Executed);

    p.build_always |= build_settings["build_always"] == "true";
    p.write_output_to_file |= build_settings["write_output_to_file"] == "true";
    if (build_settings["skip_errors"].isValue())
        p.skip_errors = std::stoll(build_settings["skip_errors"].getValue());
    if (build_settings["time_limit"].isValue())
        p.setTimeLimit(parseTimeLimit(build_settings["time_limit"].getValue()));

    ScopedTime t;
    p.execute(getExecutor());
    if (build_settings["measure"] == "true")
        LOG_DEBUG(logger, BOOST_CURRENT_FUNCTION << " time: " << t.getTimeFloat() << " s.");

    if (build_settings["time_trace"] == "true")
        p.saveChromeTrace(getBuildDirectory() / "misc" / "time_trace.json");

    path ide_fast_path = build_settings["build_ide_fast_path"].isValue() ? build_settings["build_ide_fast_path"].getValue() : "";
    if (!ide_fast_path.empty())
    {
        String s;
        for (auto &f : fast_path_files)
            s += normalize_path(f) + "\n";
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

    // only after build it is possible to record our targets
    // to skip many previous steps in the future
    if (build_settings["master_build"] != "true")
        return;

    for (const auto &[pkg, tgts] : targets_to_build)
    {
        if (!pkg.getPath().isAbsolute())
            continue;
        for (auto &tgt : tgts)
        {
            // do not overwrite settings again
            // but settings may change during development, so we should not do this check
            //if (auto dt = tgt->as<const PredefinedTarget *>())
                //continue;

            LocalPackage p(getContext().getLocalStorage(), tgt->getPackage());
            if (p.isOverridden())
                continue;
            auto cfg = tgt->getSettings().getHash();
            auto base = p.getDirObj(cfg);
            auto sfn = base / SETTINGS_FN;
            auto sfncfg = base / BASE_SETTINGS ".cfg";
            auto sptrfn = base / "settings.hash";

            if (!fs::exists(sfn) || !fs::exists(sptrfn) || read_file(sptrfn) != tgt->getInterfaceSettings().getHash())
            {
#ifndef USE_JSON
                saveSettings(sfn, tgt->getInterfaceSettings());
#else
                write_file(sfn, nlohmann::json::parse(tgt->getInterfaceSettings().toString()).dump(4));
                write_file(sfncfg, nlohmann::json::parse(tgt->getSettings().toString()).dump(4));
#endif
                write_file(sptrfn, tgt->getInterfaceSettings().getHash());
            }
        }
    }
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

    StringSet in_ttb;
    StringSet in_ttb_exclude;
    for (auto &t : build_settings["target-to-build"].getArray())
        in_ttb.insert(std::get<TargetSetting::Value>(t));
    for (auto &t : build_settings["target-to-exclude"].getArray())
    {
        if (in_ttb.find(std::get<sw::TargetSetting::Value>(t)) != in_ttb.end())
            throw SW_RUNTIME_ERROR("Target " + std::get<sw::TargetSetting::Value>(t) + " specified both in include and exclude lists");
        in_ttb_exclude.insert(std::get<sw::TargetSetting::Value>(t));
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

        ttb[p] = tgts;

        for (auto &tgt : tgts)
        {
            // gather targets to build
            const auto &s = tgt->getInterfaceSettings();

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
                            auto i = getTargets().find(PackageId(k));
                            if (i == getTargets().end())
                                throw SW_RUNTIME_ERROR("dep not found: " + k);
                            auto j = i->second.findSuitable(v.getSettings());
                            if (j == i->second.end())
                                throw SW_RUNTIME_ERROR("dep+settings not found: " + k + ": " + v.getSettings().toString());

                            auto m = ttb[PackageId(k)].findEqual((*j)->getSettings());
                            if (m != ttb[PackageId(k)].end())
                                continue;
                            ttb[PackageId(k)].push_back(*j);

                            const auto &s = (*j)->getInterfaceSettings();
                            gather_ttb(s);
                            process_deps(s);
                        }
                    };

                    get_deps(s["dependencies"]["link"].getSettings());
                    get_deps(s["dependencies"]["dummy"].getSettings());
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
    path copy_dir = build_settings["build_ide_copy_to_dir"].isValue() ? build_settings["build_ide_copy_to_dir"].getValue() : "";
    //bool copy_deps_of_local_pkgs = false;
    bool copy_deps_of_local_pkgs = copy_dir.empty();
    std::unordered_map<path, path> copy_files;

    // gather commands
    Commands cmds;
    for (auto &[p, tgts] : ttb)
    {
        for (auto &tgt : tgts)
        {
            auto c = tgt->getCommands();
            for (auto &c2 : c)
            {
                c2->maybe_unused &= ~builder::Command::MU_TRUE;
                c2->show_output = cl_show_output || cl_write_output_to_file; // only for selected targets
            }
            cmds.insert(c.begin(), c.end());

            if (copy_dir.empty() && !copy_deps_of_local_pkgs)
                continue;

            // copy output files
            const auto &s = tgt->getInterfaceSettings();

            // do not copy such deps
            // example: when processing qt plugins, without the condition we'll copy
            // main dlls to plugins dir which is not desirable
            if (s["output_dir"].isValue())
                continue;

            if (copy_deps_of_local_pkgs)
            {
                if (p.getPath().isAbsolute())
                    continue;
                if (s["header_only"] == "true")
                    continue;
                if (!(s["type"] == "native_shared_library" || s["type"] == "native_static_library" || s["type"] == "native_executable"))
                    continue;
                copy_dir = path(s["output_file"].getValue()).parent_path();
            }

            PackageIdSet visited_pkgs;
            std::function<void(const TargetSettings &)> copy_file;
            copy_file = [this, &cmds, &copy_dir, &copy_files, &copy_file, &visited_pkgs](const auto &s)
            {
                if (s["header_only"] == "true")
                    return;

                if (!(s["type"] == "native_shared_library" || s["type"] == "native_static_library" || s["type"] == "native_executable"))
                    return;

                path in = s["output_file"].getValue();
                fast_path_files.insert(in);

                if (s["import_library"].isValue())
                {
                    path il = s["import_library"].getValue();
                    fast_path_files.insert(il);
                }

                if (s["type"] == "native_shared_library"
                    // copy only for wintgt?
                    //&& PackagePath(s["os"]["kernel"].getValue()) == PackagePath("com.Microsoft.Windows.NT")
                    )
                {
                    auto o = copy_dir;
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
                    for (auto &[k, v] : s["dependencies"]["link"].getSettings())
                    {
                        PackageId p(k);
                        if (visited_pkgs.find(p) != visited_pkgs.end())
                            continue;
                        visited_pkgs.insert(p);
                        auto i = getTargets().find(p);
                        if (i == getTargets().end())
                            throw SW_RUNTIME_ERROR("dep not found");
                        auto j = i->second.findSuitable(v.getSettings());
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
        //SW_MAKE_EXECUTE_BUILTIN_COMMAND(copy_cmd, *nt, "sw_copy_file", nullptr);
        auto copy_cmd = std::make_shared<::sw::builder::ExecuteBuiltinCommand>(getContext(), "sw_copy_file", nullptr);
        copy_cmd->arguments.push_back(f);
        copy_cmd->arguments.push_back(t);
        copy_cmd->addInput(f);
        copy_cmd->addOutput(t);
        //copy_cmd->dependencies.insert(nt->getCommand());
        copy_cmd->name = "copy: " + normalize_path(t);
        copy_cmd->maybe_unused = builder::Command::MU_ALWAYS;
        copy_cmd->command_storage = &getContext().getCommandStorage(getBuildDirectory() / "cs");
        cmds.insert(copy_cmd);
        commands_storage.insert(copy_cmd); // prevents early destruction
    }

    return cmds;
}

ExecutionPlan SwBuild::getExecutionPlan() const
{
    return getExecutionPlan(getCommands());
}

ExecutionPlan SwBuild::getExecutionPlan(const Commands &cmds) const
{
    auto ep = ExecutionPlan::create(cmds);
    if (ep)
        return ep;

    // error!

    auto d = getBuildDirectory() / "misc";

    auto [g, n, sc] = ep.getStrongComponents();

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

    ep.printGraph(ep.getGraph(), cyclic_path / "processed", ep.getCommands(), true);
    ep.printGraph(ep.getGraphUnprocessed(), cyclic_path / "unprocessed", ep.getUnprocessedCommand(), true);

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
    p.save(in);
}

void SwBuild::runSavedExecutionPlan(const path &in) const
{
    auto [cmds, p] = ExecutionPlan::load(in, getContext());

    // change state
    overrideBuildState(BuildState::Prepared);
    SCOPE_EXIT
    {
        // fallback
        overrideBuildState(BuildState::InputsLoaded);
    };

    execute(p);
}

const std::vector<InputWithSettings> &SwBuild::getInputs() const
{
    return inputs;
}

void SwBuild::setSettings(const TargetSettings &bs)
{
    build_settings = bs;
}

const TargetSettings &SwBuild::getExternalVariables() const
{
    return getSettings()["D"].getSettings();
}

void SwBuild::setServiceEntryPoint(const PackageId &p, const TargetEntryPointPtr &ep)
{
    service_entry_points[p] = ep;
}

TargetEntryPointPtr SwBuild::getEntryPoint(const PackageId &p) const
{
    auto i = service_entry_points.find(p);
    if (i != service_entry_points.end())
        return i->second;
    return getContext().getEntryPoint(p);
}

path SwBuild::getTestDir() const
{
    return getBuildDirectory() / "test";
}

void SwBuild::test()
{
    build();

    auto dir = getTestDir();
    fs::remove_all(dir); // also make a condition here - what condition?

    // prepare
    for (const auto &[pkg, tgts] : getTargetsToBuild())
    {
        for (auto &tgt : tgts)
        {
            for (auto &c : tgt->getTests())
            {
                auto test_dir = dir / tgt->getSettings().getHash() / c->getName();
                fs::create_directories(test_dir);

                c->maybe_unused = builder::Command::MU_TRUE; // why?

                //
                c->name = "test: [" + c->name + "]";
                c->always = true;
                c->working_directory = test_dir;
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
    ep.execute(getExecutor());
}

}

