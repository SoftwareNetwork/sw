// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2017-2020 Egor Pugin <egor.pugin@gmail.com>

#include "checks.h"

#include "checks_storage.h"
#include "build.h"
#include "entry_point.h"
#include "target/native.h"

#include <sw/builder/execution_plan.h>
#include <sw/core/build.h>
#include <sw/core/sw_context.h>
#include <sw/manager/storage.h>
#include <sw/support/filesystem.h>
#include <sw/support/hash.h>

#include <boost/algorithm/string.hpp>
#include <nlohmann/json.hpp>
#include <primitives/emitter.h>
#include <primitives/executor.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "checks");

namespace sw
{

static path getServiceDir(const path &bdir)
{
    return bdir / "misc";
}

static path getChecksDir(const path &bdir)
{
    return getServiceDir(bdir) / "checks";
}

static String toString(CheckType t)
{
    switch (t)
    {
    case CheckType::Function:
        return "function";
    case CheckType::Include:
        return "include";
    case CheckType::Type:
        return "type";
    case CheckType::TypeAlignment:
        return "alignment";
    //case CheckType::Library:
        //return "library";
    case CheckType::LibraryFunction:
        return "library function";
    case CheckType::Symbol:
        return "symbol";
    case CheckType::StructMember:
        return "struct member";
    case CheckType::SourceCompiles:
        return "source compiles";
    case CheckType::SourceLinks:
        return "source links";
    case CheckType::SourceRuns:
        return "source runs";
    case CheckType::Declaration:
        return "source declaration";
    case CheckType::CompilerFlag:
        return "compiler flag";
    case CheckType::Custom:
        return "custom";
    default:
        SW_UNREACHABLE;
    }
}

static std::unordered_map<String, std::unique_ptr<ChecksStorage>> &getChecksStorages()
{
    static std::unordered_map<String, std::unique_ptr<ChecksStorage>> checksStorages;
    return checksStorages;
}

static ChecksStorage &getChecksStorage(const String &config)
{
    auto i = getChecksStorages().find(config);
    if (i == getChecksStorages().end())
    {
        auto [i, _] = getChecksStorages().emplace(config, std::make_unique<ChecksStorage>());
        return *i->second;
    }
    return *i->second;
}

static ChecksStorage &getChecksStorage(const String &config, const path &fn)
{
    auto i = getChecksStorages().find(config);
    if (i == getChecksStorages().end())
    {
        auto [i, _] = getChecksStorages().emplace(config, std::make_unique<ChecksStorage>());
        i->second->load(fn);
        return *i->second;
    }
    return *i->second;
}

void ChecksStorage::load(const path &fn)
{
    if (loaded)
        return;

    {
        std::ifstream i(fn);
        if (!i)
            return;
        while (i)
        {
            size_t h;
            i >> h;
            if (!i)
                break;
            i >> all_checks[h];
        }
    }

    load_manual(fn);

    loaded = true;
}

void ChecksStorage::load_manual(const path &fn)
{
#define MANUAL_CHECKS ".manual.txt"
    auto mf = path(fn) += MANUAL_CHECKS;
    if (!fs::exists(mf))
        return;
    for (auto &l : read_lines(mf))
    {
        if (l[0] == '#')
            continue;
        auto v = split_string(l, " ");
        if (v.size() != 2)
            continue;
        //throw SW_RUNTIME_ERROR("bad manual checks line: " + l);
        if (v[1] == "?")
            continue;
        //throw SW_RUNTIME_ERROR("unset manual check: " + l);
        all_checks[std::stoull(v[0])] = std::stoi(v[1]);
        new_manual_checks_loaded = true;
    }
    fs::remove(mf);
}

void ChecksStorage::save(const path &fn) const
{
    fs::create_directories(fn.parent_path());
    {
        String s;
        for (auto &[h, v] : std::map<decltype(all_checks)::key_type, decltype(all_checks)::mapped_type>(all_checks.begin(), all_checks.end()))
            s += std::to_string(h) + " " + std::to_string(v) + "\n";
        write_file(fn, s);
    }

    if (!manual_checks.empty())
    {
        String s;
        for (auto &[h, c] : std::map<decltype(manual_checks)::key_type, decltype(manual_checks)::mapped_type>(manual_checks.begin(), manual_checks.end()))
        {
            s += "# ";
            for (auto &d : c->Definitions)
                s += d + " ";
            s.resize(s.size() - 1);
            s += "\n";
            s += std::to_string(h) + " ?\n\n";
        }
        write_file(path(fn) += MANUAL_CHECKS, s);
    }
}

void ChecksStorage::add(const Check &c)
{
    auto h = c.getHash();
    if (c.requires_manual_setup && !c.Value)
    {
        manual_checks[h] = &c;
        return;
    }
    all_checks[h] = c.Value.value();
}

static String make_function_var(const String &d, const String &prefix = "HAVE_", const String &suffix = {})
{
    return prefix + boost::algorithm::to_upper_copy(d) + suffix;
}

static String make_include_var(const String &i)
{
    auto v_def = make_function_var(i);
    for (auto &c : v_def)
    {
        if (!isalnum(c))
            c = '_';
    }
    return v_def;
}

static String make_type_var(const String &t, const String &prefix = "HAVE_", const String &suffix = {})
{
    String v_def = make_function_var(t, prefix, suffix);
    for (auto &c : v_def)
    {
        if (c == '*')
            c = 'P';
        else if (!isalnum(c))
            c = '_';
    }
    return v_def;
}

static String make_struct_member_var(const String &s, const String &m)
{
    return make_include_var(s + " " + m);
}

static String make_alignment_var(const String &i)
{
    return make_type_var(i, "ALIGNOF_");
}

static void check_def(const String &d)
{
    if (d.empty())
        throw SW_RUNTIME_ERROR("Empty check definition");
}

CheckSet::CheckSet()
{
}

CheckSet::CheckSet(Checker &checker)
    : checker(&checker)
{
    // add common checks
    testBigEndian();
}

Checker &CheckSet::getChecker() const
{
    if (!checker)
        throw SW_RUNTIME_ERROR("CheckSet was not prepared for this!");
    return *checker;
}

Check &CheckSet::registerCheck(CheckStorage &s, Check &c)
{
    auto i = s.find(c.getHash());
    if (i == s.end())
    {
        s.emplace(c.getHash(), &c);
        return c;
    }
    return *i->second;
}

Check &CheckSet::registerCheck(Check &c) const
{
    return registerCheck(getChecker().all_checks, c);
}

void CheckSet::performChecks(const SwBuild &mb, const PackageSettings &ts)
{
    static const auto checks_dir = getChecker().swbld.getContext().getLocalStorage().storage_dir_etc / "sw" / "checks";

    if (!t)
        throw SW_RUNTIME_ERROR("Target was not set");

    auto config = ts.getHashString();
    auto fn = checks_dir / config / "checks.3.txt";
    auto &cs = getChecksStorage(config, fn);

    // gather deps
    {
        std::unordered_map<size_t, std::unique_ptr<Check>> deps;
        // initial deps
        for (auto &c : all)
        {
            for (auto &d : c->gatherDependencies())
                deps.emplace(d->getHash(), std::move(d));
        }
        while (1)
        {
            auto sz = deps.size();
            decltype(deps) deps2;
            for (auto &[_, c] : deps)
            {
                for (auto &d : c->gatherDependencies())
                    deps2.emplace(d->getHash(), std::move(d));
            }
            deps.merge(deps2);
            if (sz == deps.size())
                break;
        }

        // store deps
        for (auto &[_, c] : deps)
            all.emplace_back(std::move(c));
    }

    // register checks in global storage, gather unchecked
    std::unordered_set<Check*> unchecked;
    for (auto &c : all)
    {
        auto &c2 = registerCheck(*c);
        if (c2.isChecked())
            continue;
        // maybe we already know it?
        // this path is used with wait_for_cc_checks
        auto i = cs.all_checks.find(c2.getHash());
        if (i != cs.all_checks.end())
        {
            c2.Value = i->second;
            continue;
        }
        unchecked.insert(&c2);
    }

    // set deps
    for (auto &&c : unchecked)
    {
        for (auto &d : c->gatherDependencies())
        {
            if (auto &c2 = registerCheck(*d); !c2.isChecked())
                c->addDependency(c2);
        }
    }

    SCOPE_EXIT
    {
        //prepareChecksForUse();
        if (mb.getSettings()["print_checks"])
        {
            std::ofstream o(fn.parent_path() / (t->getPackage().toString() + "." + name + ".txt"));
            if (!o)
                return;
            auto r = getResults(true);
            std::map<String, Check*> cv(r.begin(), r.end());
            for (auto &[d, c] : cv)
            {
                if (c->Value)
                    o << d << " " << c->Value.value() << " " << c->getHash() << "\n";
            }
        }
        // cleanup
        for (auto &c1 : all)
            c1->clean();
    };

    if (mb.getSettings()["print_checks"])
    {
        write_file(checks_dir / config / "cfg.json", nlohmann::json::parse(ts.toString(PackageSettings::Json)).dump(4));
    }

    if (unchecked.empty())
    {
        if (cs.new_manual_checks_loaded)
            cs.save(fn);
        return;
    }

    auto ep = ExecutionPlan::create(unchecked);
    if (ep)
    {
        LOG_INFO(logger, "Performing " << unchecked.size() << " check(s): "
            << t->getPackage().toString() << " (" << name << "), config " + config);

        SCOPE_EXIT
        {
            // remove tmp dir
            error_code ec;
            fs::remove_all(getChecksDir(getChecker().swbld.getBuildDirectory()), ec);
        };

        //auto &e = getExecutor();
        static Executor e(mb.getSettings()["checks_single_thread"] ? 1 : getExecutor().numberOfThreads()); // separate executor!

        try
        {
            ep->execute(e);
        }
        catch (std::exception &)
        {
            // in case of error, some checks may be unchecked
            // and we record only checked checks
            for (auto &&c1 : all)
            {
                auto &c2 = registerCheck(*c1);
                if (c2.Value)
                    cs.add(c2);
            }
            cs.save(fn);
            throw;
        }

        for (auto &&c1 : all)
            cs.add(registerCheck(*c1));

        auto cc_dir = fn.parent_path() / "cc";

        // separate loop
        if (!cs.manual_checks.empty())
        {
            std::error_code ec;
            fs::remove_all(cc_dir, ec);
            if (ec)
                LOG_WARN(logger, "Cannot remove checks dir: " + to_string(cc_dir.u8string()));
            fs::create_directories(cc_dir, ec);

            for (auto &&c1 : all)
            {
                auto &c2 = registerCheck(*c1);
                if (c2.requires_manual_setup)
                {
                    auto dst = (cc_dir / std::to_string(c2.getHash())) += BuildSettings(ts).TargetOS.getExecutableExtension();
                    if (!fs::exists(dst))
                        fs::copy_file(c2.executable, dst, fs::copy_options::overwrite_existing);
                }
            }
        }

        // save
        cs.save(fn);

        if (!cs.manual_checks.empty())
        {
            // prevent multiple threads, but allow us to enter more than one time
            static std::recursive_mutex m;
            std::unique_lock lk(m);

            // save executables
            auto os = BuildSettings(ts).TargetOS;
            auto mfn = to_string((path(fn) += MANUAL_CHECKS).filename().u8string());

            auto bat = os.getShellType() == ShellType::Batch;

            primitives::Emitter ctx;
            if (!bat)
            {
                ctx.addLine("#!/bin/sh");
                ctx.addLine();
            }

            ctx.addLine("OUTF=\"" + mfn + "\"");
            ctx.addLine("OUT=\""s + (mb.getSettings()["wait_for_cc_checks"] ? "../" : "") + "$OUTF\"");
            ctx.addLine();

            mfn = "$OUT";
            ctx.addLine("echo \"\" > " + mfn);
            ctx.addLine();

            for (auto &[h, c] : cs.manual_checks)
            {
                String defs;
                for (auto &d : c->Definitions)
                    defs += d + " ";
                defs.resize(defs.size() - 1);

                ctx.addLine((bat ? "::"s : "#"s) + " " + defs);
                //if (!bat)
                //s += "-n ";

                auto fn = std::to_string(c->getHash());

                ctx.increaseIndent("if [ ! -f " + fn + " ]; then");
                ctx.addLine("echo missing file: " + fn);
                ctx.addLine("exit 1");
                ctx.decreaseIndent("fi");

                ctx.addLine("echo \"Checking: " + defs + "... \"");
                ctx.addLine("echo \"# " + defs + "\" >> " + mfn);

                if (!bat)
                {
                    ctx.addLine("chmod 755 " + fn);
                    ctx.addLine();
                    if (c->manual_setup_use_stdout)
                        ctx.addText("V=`");
                    ctx.addText("./");
                }
                ctx.addText(fn + BuildSettings(ts).TargetOS.getExecutableExtension());
                if (!bat)
                {
                    if (c->manual_setup_use_stdout)
                        ctx.addText("`");
                    else
                        ctx.addLine("V=$?");
                }

                if (!bat)
                {
                    // 126, 127 are used by shells
                    // 128 + signal - error values
                    ctx.addLine("if [ ! $? -ge 125 ]; then");
                    ctx.increaseIndent();
                }
                ctx.addLine("echo " + std::to_string(c->getHash()) + " ");
                if (!bat)
                    ctx.addText("$V ");
                else
                    ctx.addText("%errorlevel% ");
                ctx.addText(">> " + mfn);
                if (!bat)
                    ctx.addLine("echo \"ok (result = $V)\"");
                ctx.addLine("echo \"\" >> " + mfn);
                if (!bat)
                {
                    ctx.decreaseIndent();
                    ctx.addLine("fi");
                }
                ctx.addLine();
            }
            path out = (cc_dir / "run") += os.getShellExtension();
            write_file(out, ctx.getText());

            if (mb.getSettings()["wait_for_cc_checks"])
            {
                if (!mb.getSettings()["cc_checks_command"].getValue().empty())
                {
                    ScopedCurrentPath scp(cc_dir);
                    int r = system(mb.getSettings()["cc_checks_command"].getValue().c_str());
                    if (r)
                        throw SW_RUNTIME_ERROR("cc_checks_command exited abnormally: " + std::to_string(r));
                }
                else
                {
                    std::cout << "Waiting for completing cc checks.\n";
                    std::cout << "Run '" << to_string(normalize_path(out)) << "' and press and key to continue...\n";
                    getchar();
                }
                cs.load_manual(fn);
                for (auto &[h, c] : cs.manual_checks)
                {
                    if (cs.all_checks.find(h) == cs.all_checks.end())
                        continue;
                    c->requires_manual_setup = false;
                }
                cs.manual_checks.clear();
                return performChecks(mb, ts);
            }

            throw SW_RUNTIME_ERROR("Some manual checks are missing, please set them in order to continue. "
                "Manual checks file: " + to_string((path(fn) += MANUAL_CHECKS).u8string()) + ". "
                "You also may copy produced binaries to target platform and run them there using prepared script. "
                "Results will be gathered into required file. "
                "Binaries directory: " + to_string(cc_dir.u8string())
            );
        }

        return;
    }

    // error!

    // print our deps graph
    String s;
    s += "digraph G {\n";
    for (auto &c : ep->getUnprocessedCommandsSet())
    {
        for (auto &d : c->getDependencies())
        {
            if (ep->getUnprocessedCommandsSet().find(static_cast<Check*>(d)) == ep->getUnprocessedCommandsSet().end())
                continue;
            s += *static_cast<Check*>(c)->Definitions.begin() + "->" + *static_cast<Check*>(d)->Definitions.begin() + ";";
        }
    }
    s += "}";

    auto d = getServiceDir(getChecker().swbld.getBuildDirectory());
    auto cyclic_path = d / "cyclic";
    write_file(cyclic_path / "deps_checks.dot", s);

    throw SW_RUNTIME_ERROR("Cannot create execution plan because of cyclic dependencies");
}

std::unordered_map<String, Check*> CheckSet::getResults(bool allow_partial) const
{
    std::unordered_map<String, Check*> r;
    auto add_val = [&r](auto &&def, auto &&val)
    {
        r[def] = &val;
    };
    for (auto &&c1 : all)
    {
        auto &c2 = registerCheck(*c1);
        if (!c2.isChecked() && !allow_partial)
            throw SW_RUNTIME_ERROR("Check was not executed");

        // add to check_values only requested defs
        // otherwise we'll get also defs from other sets (e.g. with prefixes from ICU 'U_')
        for (auto &d : c1->Definitions)
        {
            add_val(d, c2);
            for (auto &p : c1->Prefixes)
                add_val(p + d, c2);
        }
    }
    return r;
}

Check::Check()
{
    setFileName("x.c");
}

Check::~Check()
{
    clean();
}

void Check::setCpp()
{
    setFileName("x.cpp");
}

void Check::clean() const
{
    commands.clear();
}

String Check::getName() const
{
    auto d = getDefinition();
    if (d)
        return *d;
    return {};
}

std::optional<String> Check::getDefinition() const
{
    return getDefinition(*Definitions.begin());
}

std::optional<String> Check::getDefinition(const String &d) const
{
    if (Value.value() != 0 || DefineIfZero)
        return d + "=" + std::to_string(Value.value());
    return std::nullopt;
}

bool Check::isChecked() const
{
    return !!Value;
}

size_t CheckParameters::getHash() const
{
    size_t h = 0;
    for (auto &d : Definitions)
        hash_combine(h, d);
    for (auto &d : Includes)
        hash_combine(h, d);
    for (auto &d : IncludeDirectories)
        hash_combine(h, d);
    for (auto &d : Libraries)
        hash_combine(h, d);
    for (auto &d : CompileOptions)
        hash_combine(h, d);
    for (auto &d : LinkOptions)
        hash_combine(h, d);
    return h;
}

size_t Check::getHash() const
{
    size_t h = 0;
    hash_combine(h, data);
    hash_combine(h, Parameters.getHash());
    hash_combine(h, filename);
    hash_combine(h, getVersion());
    return h;
}

void Check::execute()
{
    if (isChecked())
        return;
    //Value = 0; // mark as checked

    String log_string = "[" + std::to_string((*current_command)++) + "/" + std::to_string(total_commands->load()) + "] ";
    //LOG_TRACE(logger, "Checking " << data);

    // value must be set inside?
    run();

    if (Definitions.empty())
        throw SW_RUNTIME_ERROR(log_string + "Check " + data + ": definition was not set");
    if (!Value)
    {
        if (requires_manual_setup)
        {
            LOG_INFO(logger, log_string + "Check " << *Definitions.begin() << " requires to be set up manually");
            return;
        }
        throw SW_RUNTIME_ERROR(log_string + "Check " + *Definitions.begin() + ": value was not set");
    }
    LOG_DEBUG(logger, log_string + "Checking " << toString(getType()) << " " << *Definitions.begin() << ": " << Value.value());
}

std::vector<std::unique_ptr<Check>> Check::gatherDependencies() const
{
    std::vector<std::unique_ptr<Check>> deps;
    for (auto &d : Parameters.Includes)
        deps.emplace_back(check_set->addRaw<IncludeExists>(d));
    return deps;
}

bool Check::lessDuringExecution(const CommandNode &in) const
{
    // improve sorting! it's too stupid
    // simple "0 0 0 0 1 2 3 6 7 8 9 11" is not enough

    auto &rhs = (const Check &)in;

    if (getDependencies().size() != rhs.getDependencies().size())
        return getDependencies().size() < rhs.getDependencies().size();
    return dependent_commands.size() > dependent_commands.size();
}

const path &Check::getUniqueName() const
{
    if (uniq_name.empty())
    {
        // two parts:
        // 1. check hash
        // 2. some unique string.
        //
        // Second part is needed to prevent file use when running the following same check
        // of other config. On Windows old executables may still exist and not removed by the system,
        // so linking a new one will result in error.
        uniq_name = std::to_string(getHash()) / unique_path();
    }
    return uniq_name;
}

path Check::getOutputFilename() const
{
    auto d = getChecksDir(check_set->getChecker().swbld.getBuildDirectory());
    auto up = getUniqueName();
    d /= up;
    auto f = d;
    f /= filename;
    return f;
}

static path getUniquePath(const path &p)
{
    return boost::replace_all_copy(p.parent_path().filename().u8string(), "-", "_");
}

static String getTargetName(const path &p)
{
    return "loc." + getUniquePath(p).string();
}

static Build setupSolution(SwBuild &b, const path &f)
{
    Build s(b);
    s.BinaryDir = f.parent_path();
    s.NamePrefix.clear();
    s.DryRun = false;
    return s;
}

PackageSettings Check::getSettings() const
{
    auto ss = check_set->t->getSettings();

    // some checks may fail in msvc release (functions become intrinsics (mem*) etc.)
    if (check_set->t->getCompilerType() == CompilerType::MSVC ||
        check_set->t->getCompilerType() == CompilerType::ClangCl)
        ss["native"]["configuration"] = "debug"s;

    // set output dir for check binaries
    auto d = getChecksDir(check_set->getChecker().swbld.getBuildDirectory());
    auto up = getUniqueName();
    d /= up;
    ss["output_dir"] = to_string(normalize_path(d));
    ss["output_dir"].ignoreInComparison(true);

    return ss;
}

void Check::setupTarget(NativeCompiledTarget &t) const
{
    t.GenerateWindowsResource = false;
    // TODO: restore!
    //if (auto L = t.getSelectedTool()->as<VisualStudioLinker*>())
        //L->DisableIncrementalLink = true; // do not create .ilk?
    t.command_storage = nullptr;
}

static std::shared_ptr<builder::Command> getLinkerCommand(const NativeCompiledTarget &t, const path &srcfn)
{
    auto cmds = t.getCommands();
    auto i = std::find_if(cmds.begin(), cmds.end(), [&srcfn](auto &c)
    {
        return !c->inputs.contains(srcfn);
    });
    if (i == cmds.end())
    {
        // no command found
        // this means zero result
        return {};
    }
    return *i;
}

bool Check::execute(SwBuild &b) const
{
    b.overrideBuildState(BuildState::InputsLoaded);
    //b.setTargetsToBuild();
    //b.resolvePackages();
    //b.loadPackages();
    //b.prepare();

    try
    {
        // save commands for cleanup
        auto p = b.getExecutionPlan();
        // we must save comands here, because later we need to check results of specific commands
        for (auto &c : p->getCommands())
            commands.push_back(std::static_pointer_cast<builder::Command>(c->shared_from_this()));
        p->silent = true;

        b.execute(*p);
    }
    catch (std::exception &e)
    {
        Value = 0;
        LOG_TRACE(logger, "Check " + data + ": check issue: " << e.what());
        return false;
    }
    return true;
}

#define SETUP_SOLUTION()                                                \
    auto b = check_set->getChecker().swbld.getContext().createBuild();  \
    auto s = setupSolution(*b, f);                                      \
    auto cs = getSettings();                                            \
    s.module_data.current_settings = &cs

#define ADD_TARGETS                             \
    for (auto &t : s.module_data.getTargets())  \
    b->getTargets()[t->getPackage()].push_back(*t)

#define EXECUTE_SOLUTION() \
    ADD_TARGETS;           \
    if (!execute(*b))      \
    return

// without exception
#define EXECUTE_SOLUTION_RET() \
    ADD_TARGETS;               \
    auto r = execute(*b)

FunctionExists::FunctionExists(const String &f, const String &def)
{
    if (f.empty())
        throw SW_RUNTIME_ERROR("Empty function");
    data = f;

    if (def.empty())
        Definitions.insert(make_function_var(data));
    else
        Definitions.insert(def);

    check_def(*Definitions.begin());
}

String FunctionExists::getSourceFileContents() const
{
    static const String src{ R"(
#ifdef __cplusplus
extern "C"
#endif
  char
  CHECK_FUNCTION_EXISTS(void);
#ifdef __CLASSIC_C__
int main()
{
  int ac;
  char* av[];
#else
int main(int ac, char* av[])
{
#endif
  CHECK_FUNCTION_EXISTS();
  if (ac > 1000) {
    return *av[0];
  }
  return 0;
}
)"
    };

    return src;
}

void FunctionExists::run() const
{
    auto f = getOutputFilename();
    write_file(f, getSourceFileContents());

    SETUP_SOLUTION();

    auto &e = s.addTarget<ExecutableTarget>(getTargetName(f));
    e.Definitions["CHECK_FUNCTION_EXISTS"] = data; // before setup, because it is changed later for LibraryFunctionExists
    setupTarget(e);
    e += f;

    EXECUTE_SOLUTION();

    auto cmd = getLinkerCommand(e, f);
    Value = (cmd && cmd->exit_code && cmd->exit_code.value() == 0) ? 1 : 0;
}

IncludeExists::IncludeExists(const String &i, const String &def)
{
    if (i.empty())
        throw SW_RUNTIME_ERROR("Empty include");
    data = i;

    if (def.empty())
    {
        Definitions.insert(make_include_var(data));

        // some libs expect HAVE_SYSTIME_H and not HAVE_SYS_TIME_H
        if (data.find("sys/") == 0)
        {
            auto d2 = data;
            d2 = "sys" + data.substr(4);
            Definitions.insert(make_include_var(d2));
        }
    }
    else
        Definitions.insert(def);

    check_def(*Definitions.begin());
}

String IncludeExists::getSourceFileContents() const
{
    String src = "#include <" + data + ">";
    if (filename.extension() == ".c")
        src += R"(
#ifdef __CLASSIC_C__
int main()
{
  return 0;
}
#else
int main(void)
{
  return 0;
}
#endif
)";
    else
        src += R"(
int main()
{
  return 0;
}
)";

    return src;
}

void IncludeExists::run() const
{
    auto f = getOutputFilename();
    write_file(f, getSourceFileContents());

    SETUP_SOLUTION();

    auto &e = s.addTarget<ExecutableTarget>(getTargetName(f));
    setupTarget(e);
    e += f;

    EXECUTE_SOLUTION();

    auto cmd = getLinkerCommand(e, f);
    Value = (cmd && cmd->exit_code && cmd->exit_code.value() == 0) ? 1 : 0;
}

TypeSize::TypeSize(const String &t, const String &def)
{
    if (t.empty())
        throw SW_RUNTIME_ERROR("Empty type");
    data = t;

    Definitions.insert(make_type_var(data));
    Definitions.insert(make_type_var(data, "SIZEOF_"));
    // some cmake new thing
    // https://cmake.org/cmake/help/latest/module/CheckTypeSize.html
    Definitions.insert(make_type_var(data, "SIZEOF_", "_CODE"));
    Definitions.insert(make_type_var(data, "SIZE_OF_"));
    // some libs want these
    Definitions.insert(make_type_var(data, "HAVE_SIZEOF_"));
    Definitions.insert(make_type_var(data, "HAVE_SIZE_OF_"));

    if (!def.empty())
        Definitions.insert(def);

    check_def(*Definitions.begin());

    for (auto &h : { "sys/types.h", "stdint.h", "stddef.h", "inttypes.h" })
        Parameters.Includes.push_back(h);
    // for printf
    Parameters.Includes.push_back("stdio.h");
}

String TypeSize::getSourceFileContents() const
{
    String src;
    for (auto &d : Parameters.Includes)
    {
        auto &c = check_set->get<IncludeExists>(d);
        if (c.Value && c.Value.value())
            src += "#include <" + d + ">\n";
    }
    // use printf because size of some struct may be greater than 128
    // and we cannot pass it via exit code
    src += "#include <stdio.h>\nint main() { printf(\"%d\", sizeof(" + data + ")); return 0; }";

    return src;
}

void TypeSize::run() const
{
    auto f = getOutputFilename();
    write_file(f, getSourceFileContents());

    SETUP_SOLUTION();

    auto &e = s.addTarget<ExecutableTarget>(getTargetName(f));
    setupTarget(e);
    e += f;

    EXECUTE_SOLUTION();

    auto cmd = getLinkerCommand(e, f);
    if (!cmd)
    {
        Value = 0;
        return;
    }

    if (!check_set->t->getContext().getHostOs().canRunTargetExecutables(check_set->t->getBuildSettings().TargetOS))
    {
        requires_manual_setup = true;
        manual_setup_use_stdout = true;
        executable = e.getOutputFile();
        return;
    }

    primitives::Command c;
    c.setProgram(e.getOutputFile());
    error_code ec;
    c.execute(ec);
    if (!ec)
        Value = std::stoi(c.out.text);
    else
        Value = 0;
}

TypeAlignment::TypeAlignment(const String &t, const String &def)
{
    if (t.empty())
        throw SW_RUNTIME_ERROR("Empty type");
    data = t;

    if (def.empty())
        Definitions.insert(make_alignment_var(data));
    else
        Definitions.insert(def);

    check_def(*Definitions.begin());

    for (auto &h : { "sys/types.h", "stdint.h", "stddef.h", "stdio.h", "stdlib.h", "inttypes.h" })
        Parameters.Includes.push_back(h);
}

String TypeAlignment::getSourceFileContents() const
{
    String src;
    for (auto &d : Parameters.Includes)
    {
        auto &c = check_set->get<IncludeExists>(d);
        if (c.Value && c.Value.value())
            src += "#include <" + d + ">\n";
    }
    src += R"(
int main()
{
    char diff;
    struct foo {char a; )" + data + R"( b;};
    struct foo *p = (struct foo *) malloc(sizeof(struct foo));
    diff = ((char *)&p->b) - ((char *)&p->a);
    return diff;
}
)";

    return src;
}

void TypeAlignment::run() const
{
    auto f = getOutputFilename();
    write_file(f, getSourceFileContents());

    SETUP_SOLUTION();

    auto &e = s.addTarget<ExecutableTarget>(getTargetName(f));
    setupTarget(e);
    e += f;

    EXECUTE_SOLUTION();

    auto cmd = getLinkerCommand(e, f);
    if (!cmd)
    {
        Value = 0;
        return;
    }

    if (!check_set->t->getContext().getHostOs().canRunTargetExecutables(check_set->t->getBuildSettings().TargetOS))
    {
        requires_manual_setup = true;
        executable = e.getOutputFile();
        return;
    }

    primitives::Command c;
    c.setProgram(e.getOutputFile());
    error_code ec;
    c.execute(ec);
    Value = c.exit_code;
}

SymbolExists::SymbolExists(const String &s, const String &def)
{
    if (s.empty())
        throw SW_RUNTIME_ERROR("Empty symbol");
    data = s;

    if (def.empty())
        Definitions.insert(make_function_var(data));
    else
        Definitions.insert(def);

    check_def(*Definitions.begin());
}

String SymbolExists::getSourceFileContents() const
{
    String src;
    for (auto &d : Parameters.Includes)
    {
        auto &c = check_set->get<IncludeExists>(d);
        if (c.Value && c.Value.value())
            src += "#include <" + d + ">\n";
    }
    src += R"(
int main(int argc, char** argv)
{
  (void)argv;
#ifndef )" + data + R"(
  return ((int*)(&)" + data + R"())[argc];
#else
  (void)argc;
  return 0;
#endif
}
)";

    return src;
}

void SymbolExists::run() const
{
    auto f = getOutputFilename();
    write_file(f, getSourceFileContents());

    SETUP_SOLUTION();

    auto &e = s.addTarget<ExecutableTarget>(getTargetName(f));
    setupTarget(e);
    e += f;

    EXECUTE_SOLUTION();

    Value = 1;
}

DeclarationExists::DeclarationExists(const String &d, const String &def)
{
    if (d.empty())
        throw SW_RUNTIME_ERROR("Empty declaration");
    data = d;

    if (def.empty())
        Definitions.insert(make_function_var(data, "HAVE_DECL_"));
    else
        Definitions.insert(def);

    check_def(*Definitions.begin());

    for (auto &h : { "sys/types.h",
                    "stdint.h",
                    "stddef.h",
                    "inttypes.h",
                    "stdio.h",
                    "sys/stat.h",
                    "stdlib.h",
                    "memory.h",
                    "string.h",
                    "strings.h",
                    "unistd.h" })
        Parameters.Includes.push_back(h);
}

String DeclarationExists::getSourceFileContents() const
{
    String src;
    for (auto &d : Parameters.Includes)
    {
        auto &c = check_set->get<IncludeExists>(d);
        if (c.Value && c.Value.value())
            src += "#include <" + d + ">\n";
    }
    src += "int main() { (void)" + data + "; return 0; }";

    return src;
}

void DeclarationExists::run() const
{
    auto f = getOutputFilename();
    write_file(f, getSourceFileContents());

    SETUP_SOLUTION();

    auto &e = s.addTarget<ExecutableTarget>(getTargetName(f));
    setupTarget(e);
    e += f;

    EXECUTE_SOLUTION();

    auto cmd = getLinkerCommand(e, f);
    Value = (cmd && cmd->exit_code && cmd->exit_code.value() == 0) ? 1 : 0;
}

StructMemberExists::StructMemberExists(const String &struct_, const String &member, const String &def)
    : struct_(struct_), member(member)
{
    if (struct_.empty() || member.empty())
        throw SW_RUNTIME_ERROR("Empty struct/member");
    data = struct_ + "." + member;

    if (def.empty())
        Definitions.insert(make_struct_member_var(struct_, member));
    else
        Definitions.insert(def);

    check_def(*Definitions.begin());
}

size_t StructMemberExists::getHash() const
{
    auto h = Check::getHash();
    hash_combine(h, struct_);
    hash_combine(h, member);
    return h;
}

String StructMemberExists::getSourceFileContents() const
{
    String src;
    for (auto &d : Parameters.Includes)
    {
        auto &c = check_set->get<IncludeExists>(d);
        if (c.Value && c.Value.value())
            src += "#include <" + d + ">\n";
    }
    src += "int main() { sizeof(((" + struct_ + " *)0)->" + member + "); return 0; }";

    return src;
}

void StructMemberExists::run() const
{
    auto f = getOutputFilename();
    write_file(f, getSourceFileContents());

    SETUP_SOLUTION();

    auto &e = s.addTarget<ExecutableTarget>(getTargetName(f));
    setupTarget(e);
    e += f;

    EXECUTE_SOLUTION();

    auto cmd = getLinkerCommand(e, f);
    Value = (cmd && cmd->exit_code && cmd->exit_code.value() == 0) ? 1 : 0;
}

LibraryFunctionExists::LibraryFunctionExists(const String &library, const String &function, const String &def)
    : library(library), function(function)
{
    if (library.empty() || function.empty())
        throw SW_RUNTIME_ERROR("Empty library/function");
    data = library + "." + function;

    if (def.empty())
        Definitions.insert(make_function_var(function));
    else
        Definitions.insert(def);

    check_def(*Definitions.begin());
}

size_t LibraryFunctionExists::getHash() const
{
    auto h = Check::getHash();
    hash_combine(h, library);
    hash_combine(h, function);
    return h;
}

void LibraryFunctionExists::setupTarget(NativeCompiledTarget &e) const
{
    FunctionExists::setupTarget(e);
    e.Definitions["CHECK_FUNCTION_EXISTS"] = function;
    e.NativeLinkerOptions::System.LinkLibraries.push_back(LinkLibrary{ library });
}

SourceCompiles::SourceCompiles(const String &def, const String &source)
{
    if (def.empty() || source.empty())
        throw SW_RUNTIME_ERROR("Empty def/source");
    data = source;
    Definitions.insert(def);
    check_def(*Definitions.begin());
}

String SourceCompiles::getSourceFileContents() const
{
    return data;
}

void SourceCompiles::run() const
{
    auto f = getOutputFilename();
    write_file(f, getSourceFileContents());

    SETUP_SOLUTION();

    auto &e = s.addTarget<ExecutableTarget>(getTargetName(f));
    setupTarget(e);
    for (auto &f : Parameters.CompileOptions)
        e.CompileOptions.push_back(f);
    e += f;

    EXECUTE_SOLUTION_RET();

    auto cmds = e.getCommands();
    auto i = std::find_if(cmds.begin(), cmds.end(), [&f](auto &c)
    {
        return c->inputs.contains(f);
    });
    if (i == cmds.end())
    {
        // no command found - we can't build provided file
        // this means zero result
        Value = 0;
        return;
    }
    auto &cmd = *i;
    Value = (cmd && cmd->exit_code && cmd->exit_code.value() == 0) ? 1 : 0;

    // fast return on fail
    if (*Value == 0)
        return;

    // skip fail checks
    if (fail_regex.empty())
        return;

    for (auto &fr : fail_regex)
    {
        std::regex r(fr);
        if (std::regex_search(cmd->out.text, r) || std::regex_search(cmd->err.text, r))
        {
            // if we found failed regex, this means we have no such flag
            // and we mark command as failed
            Value = 0;
            return;
        }
    }
    // leave value as is
}

SourceLinks::SourceLinks(const String &def, const String &source)
{
    if (def.empty() || source.empty())
        throw SW_RUNTIME_ERROR("Empty def/source");
    data = source;
    Definitions.insert(def);
    check_def(*Definitions.begin());
}

String SourceLinks::getSourceFileContents() const
{
    return data;
}

void SourceLinks::run() const
{
    auto f = getOutputFilename();
    write_file(f, getSourceFileContents());

    SETUP_SOLUTION();

    auto &e = s.addTarget<ExecutableTarget>(getTargetName(f));
    setupTarget(e);
    e += f;

    EXECUTE_SOLUTION();

    Value = 1;
}

SourceRuns::SourceRuns(const String &def, const String &source)
{
    if (def.empty() || source.empty())
        throw SW_RUNTIME_ERROR("Empty def/source");
    data = source;
    Definitions.insert(def);
    check_def(*Definitions.begin());
}

String SourceRuns::getSourceFileContents() const
{
    return data;
}

void SourceRuns::run() const
{
    auto f = getOutputFilename();
    write_file(f, getSourceFileContents());

    SETUP_SOLUTION();

    auto &e = s.addTarget<ExecutableTarget>(getTargetName(f));
    setupTarget(e);
    e += f;

    EXECUTE_SOLUTION();

    auto cmd = getLinkerCommand(e, f);
    if (!cmd)
    {
        Value = 0;
        return;
    }

    if (!check_set->t->getContext().getHostOs().canRunTargetExecutables(check_set->t->getBuildSettings().TargetOS))
    {
        requires_manual_setup = true;
        executable = e.getOutputFile();
        return;
    }

    primitives::Command c;
    c.setProgram(e.getOutputFile());
    error_code ec;
    c.execute(ec);
    Value = c.exit_code;
}

CompilerFlag::CompilerFlag(const String &def, const String &compiler_flag)
    : SourceCompiles(def, "int main() {return 0;}")
{
    Parameters.CompileOptions.push_back(compiler_flag);
}

CompilerFlag::CompilerFlag(const String &def, const Strings &compiler_flags)
    : SourceCompiles(def, "int main() {return 0;}")
{
    for (auto &f : compiler_flags)
        Parameters.CompileOptions.push_back(f);
}

FunctionExists &CheckSet::checkFunctionExists(const String &function, const String &def)
{
    return add<FunctionExists>(function, def);
}

IncludeExists &CheckSet::checkIncludeExists(const String &include, const String &def)
{
    return add<IncludeExists>(include, def);
}

/*FunctionExists &CheckSet::checkLibraryExists(const String &library, const String &def)
{
    auto c = add<FunctionExists>(library, def);
    return *c;
}*/

LibraryFunctionExists &CheckSet::checkLibraryFunctionExists(const String &library, const String &function, const String &def)
{
    return add<LibraryFunctionExists>(library, function, def);
}

SymbolExists &CheckSet::checkSymbolExists(const String &symbol, const String &def)
{
    return add<SymbolExists>(symbol, def);
}

StructMemberExists &CheckSet::checkStructMemberExists(const String &s, const String &member, const String &def)
{
    return add<StructMemberExists>(s, member, def);
}

DeclarationExists &CheckSet::checkDeclarationExists(const String &decl, const String &def)
{
    return add<DeclarationExists>(decl, def);
}

TypeSize &CheckSet::checkTypeSize(const String &type, const String &def)
{
    return add<TypeSize>(type, def);
}

TypeAlignment &CheckSet::checkTypeAlignment(const String &type, const String &def)
{
    return add<TypeAlignment>(type, def);
}

SourceCompiles &CheckSet::checkSourceCompiles(const String &def, const String &src)
{
    return add<SourceCompiles>(def, src);
}

SourceLinks &CheckSet::checkSourceLinks(const String &def, const String &src)
{
    return add<SourceLinks>(def, src);
}

SourceRuns &CheckSet::checkSourceRuns(const String &def, const String &src)
{
    return add<SourceRuns>(def, src);
}

SourceRuns &CheckSet::testBigEndian(const String &def)
{
    return testBigEndian(def, R"(
int IsBigEndian()
{
    volatile int i=1;
    return ! *((char *)&i);
}
int main() { return IsBigEndian(); }
)");
}

SourceRuns &CheckSet::testBigEndian(const String &def, const String &src)
{
    return checkSourceRuns(def, src);
}

void CheckSet::prepareChecksForUse()
{
    SW_UNIMPLEMENTED;
    /*for (auto &[h, c] : checks)
    {
        for (auto &d : c->Definitions)
        {
            if (check_values.find(d) != check_values.end())
                check_values[d] = c;
            for (auto &p : c->Prefixes)
            {
                if (check_values.find(p + d) != check_values.end())
                    check_values[p + d] = c;
            }
        }
    }*/
}

Checker::Checker(SwBuild &swbld)
    : swbld(swbld)
{
}

CheckSet &Checker::addSet(const String &name)
{
    auto [i,_] = sets.emplace(name, std::make_unique<CheckSet>(*this));
    i->second->name = name;
    return *i->second;
}

}
