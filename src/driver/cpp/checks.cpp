// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "checks.h"

#include <checks_storage.h>
#include <db.h>
#include <solution.h>
#include <target/native.h>

#include <execution_plan.h>
#include <filesystem.h>
#include <hash.h>

#include <boost/algorithm/string.hpp>
#include <primitives/sw/settings.h>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "checks");

static cl::opt<bool> print_checks("print-checks", cl::desc("Save extended checks info to file"));

namespace
{

bool bSilentChecks = true;

}

namespace sw
{

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

#define MANUAL_CHECKS ".manual.txt"
    auto mf = path(fn) += MANUAL_CHECKS;
    if (fs::exists(mf))
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

    loaded = true;
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

String make_function_var(const String &d, const String &prefix = "HAVE_")
{
    return prefix + boost::algorithm::to_upper_copy(d);
}

String make_include_var(const String &i)
{
    auto v_def = make_function_var(i);
    for (auto &c : v_def)
    {
        if (!isalnum(c))
            c = '_';
    }
    return v_def;
}

String make_type_var(const String &t, const String &prefix = "HAVE_")
{
    String v_def = make_function_var(t, prefix);
    for (auto &c : v_def)
    {
        if (c == '*')
            c = 'P';
        else if (!isalnum(c))
            c = '_';
    }
    return v_def;
}

String make_struct_member_var(const String &s, const String &m)
{
    return make_include_var(s + " " + m);
}

String make_alignment_var(const String &i)
{
    return make_type_var(i, "ALIGNOF_");
}

void check_def(const String &d)
{
    if (d.empty())
        throw SW_RUNTIME_ERROR("Empty check definition");
}

CheckSet::CheckSet(Checker &checker)
    : checker(checker)
{
}

Checker::Checker()
{
    checksStorage = std::make_unique<ChecksStorage>();
}

CheckSet &Checker::addSet(const String &name)
{
    auto p = sets[current_gn].emplace(name, CheckSet(*this));
    return p.first->second;
}

void Checker::performChecks(path fn)
{
    fn /= "checks.3.txt";

    // load
    checksStorage->load(fn);

    // add common checks
    for (auto &[gn, s2] : sets)
    {
        for (auto &s : s2)
        {
            s.second.checkSourceRuns("WORDS_BIGENDIAN", R"(
int IsBigEndian()
{
    volatile int i=1;
    return ! *((char *)&i);
}
int main() { return IsBigEndian(); }
)");
        }
    }

    // returns true if inserted
    auto add_dep = [this](auto &s, auto &c)
    {
        auto h = c->getHash();
        auto ic = checks.find(h);
        if (ic != checks.end())
        {
            //if (c->Value != ic->second->Value)
                //throw SW_RUNTIME_ERROR("Different check values");
            s.checks[h] = ic->second;
            ic->second->Definitions.insert(c->Definitions.begin(), c->Definitions.end());
            ic->second->Prefixes.insert(c->Prefixes.begin(), c->Prefixes.end());
            return std::pair{ false, ic->second };
        }
        checks[h] = c;
        s.checks[h] = c;

        auto i = checksStorage->all_checks.find(h);
        if (i != checksStorage->all_checks.end())
            c->Value = i->second;
        return std::pair{ true, c };
    };

    // prepare loaded checks
    for (auto &[gn, s2] : sets)
    {
        for (auto &[n, s] : s2)
        {
            for (auto &c : s.all)
            {
                auto[inserted, dep] = add_dep(s, c);
                auto deps = c->gatherDependencies();
                for (auto &d : deps)
                {
                    auto [inserted, dep2] = add_dep(s, d);
                    dep->dependencies.insert(dep2);
                }

                // add to check_values only requested defs
                // otherwise we'll get also defs from other sets (e.g. with prefixes from ICU 'U_')
                for (auto &d : c->Definitions)
                {
                    s.check_values[d];
                    for (auto &p : c->Prefixes)
                        s.check_values[p + d];
                }
            }
            s.all.clear();
        }
    }

    // perform
    std::unordered_set<CheckPtr> unchecked;
    for (auto &[h, c] : checks)
    {
        if (!c->isChecked())
            unchecked.insert(c);
    }

    SCOPE_EXIT
    {
        for (auto &[gn, s2] : sets)
        {
            for (auto &[n, set] : s2)
            {
                set.prepareChecksForUse();
                if (print_checks)
                {
                    std::ofstream o(fn.parent_path() / (std::to_string(gn) + "." + n +  + ".checks.txt"));
                    if (!o)
                        continue;
                    std::map<String, CheckPtr> check_values(set.check_values.begin(), set.check_values.end());
                    for (auto &[d, c] : check_values)
                    {
                        if (c->Value)
                            o << d << " " << c->Value.value() << " " << c->getHash() << "\n";
                    }
                }
                // cleanup
                for (auto &[h, c] : set.checks)
                {
                    c->clean();
                }
            }
        }
    };

    if (unchecked.empty())
    {
        if (checksStorage->new_manual_checks_loaded)
            checksStorage->save(fn);
        return;
    }

    auto ep = ExecutionPlan<Check>::createExecutionPlan(unchecked);
    if (ep)
    {
        LOG_INFO(logger, "Performing " << unchecked.size() << " check(s)");

        //auto &e = getExecutor();
        Executor e(getExecutor().numberOfThreads()); // separate executor!
        //ep.throw_on_errors = false;
        //ep.skip_errors = ep.commands.size();
        ep.execute(e);

        // remove tmp dir
        error_code ec;
        fs::remove_all(solution->getChecksDir(), ec);

        for (auto &[gn, s2] : sets)
        {
            for (auto &[d, set] : s2)
            {
                for (auto &[h, c] : set.checks)
                {
                    checksStorage->add(*c);
                }
            }
        }

        auto cc_dir = fn.parent_path() / "cc";

        // separate loop
        if (!checksStorage->manual_checks.empty())
        {
            fs::remove_all(cc_dir);
            fs::create_directories(cc_dir);

            for (auto &[gn, s2] : sets)
            {
                for (auto &[d, set] : s2)
                {
                    for (auto &[h, c] : set.checks)
                    {
                        if (c->requires_manual_setup)
                        {
                            auto dst = (cc_dir / std::to_string(c->getHash())) += solution->Settings.TargetOS.getExecutableExtension();
                            if (!fs::exists(dst))
                                fs::copy_file(c->executable, dst, fs::copy_options::overwrite_existing);
                        }
                    }
                }
            }
        }

        // save
        checksStorage->save(fn);

        if (!checksStorage->manual_checks.empty())
        {
            // save executables
            auto &os = solution->Settings.TargetOS;
            auto mfn = (path(fn) += MANUAL_CHECKS).filename().u8string();

            auto bat = os.getShellType() == ShellType::Batch;

            String s;
            if (!bat)
                s += "#!/bin/sh\n\n";
            s += "echo \"\" > " + mfn + "\n\n";
            for (auto &[h, c] : checksStorage->manual_checks)
            {
                String defs;
                for (auto &d : c->Definitions)
                    defs += d + " ";
                defs.resize(defs.size() - 1);

                s += bat ? "::" : "#";
                s += " " + defs + "\n";
                s += "echo ";
                //if (!bat)
                    //s += "-n ";
                s += "\"Checking: " + defs + "... \"\n";
                s += "echo \"# " + defs + "\" >> " + mfn + "\n";
                if (!bat)
                    s += "./";
                s += std::to_string(c->getHash()) + solution->Settings.TargetOS.getExecutableExtension() + "\n";
                s += "echo " + std::to_string(c->getHash()) + " ";
                if (!bat)
                    s += "$? ";
                else
                    s += "%errorlevel% ";
                s += ">> " + mfn + "\n";
                if (!bat)
                    s += "echo ok\n";
                s += "echo \"\" >> " + mfn + "\n";
                s += "\n";
            }
            write_file((cc_dir / "run") += os.getShellExtension(), s);

            throw SW_RUNTIME_ERROR("Some manual checks are missing, please set them in order to continue. "
                "Manual checks file: " + (path(fn) += MANUAL_CHECKS).u8string() + ". "
                "You also may copy produced binaries to target platform and run them there using prepared script. "
                "Results will be gathered into required file. "
                "Binaries directory: " + cc_dir.u8string()
            );
        }

        return;
    }

    // error!

    // print our deps graph
    String s;
    s += "digraph G {\n";
    for (auto &c : ep.unprocessed_commands_set)
    {
        for (auto &d : c->dependencies)
        {
            if (ep.unprocessed_commands_set.find(static_cast<Check*>(d.get())) == ep.unprocessed_commands_set.end())
                continue;
            s += *c->Definitions.begin() + "->" + *std::static_pointer_cast<Check>(d)->Definitions.begin() + ";";
        }
    }
    s += "}";

    auto d = solution->getServiceDir();
    auto cyclic_path = d / "cyclic";
    write_file(cyclic_path / "deps_checks.dot", s);

    throw SW_RUNTIME_ERROR("Cannot create execution plan because of cyclic dependencies");
}

Check::~Check()
{
    clean();
}

void Check::clean() const
{
    for (auto &c : commands)
        c->clean();
    commands.clear();
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
    hash_combine(h, cpp);
    for (auto &d : Definitions)
        hash_combine(h, d);
    for (auto &d : Includes)
        hash_combine(h, d);
    for (auto &d : IncludeDirectories)
        hash_combine(h, d);
    for (auto &d : Libraries)
        hash_combine(h, d);
    for (auto &d : Options)
        hash_combine(h, d);
    return h;
}

size_t Check::getHash() const
{
    size_t h = 0;
    hash_combine(h, data);
    hash_combine(h, Parameters.getHash());
    hash_combine(h, CPP);
    return h;
}

void Check::execute()
{
    if (isChecked())
        return;
    //Value = 0; // mark as checked

    //LOG_TRACE(logger, "Checking " << data);

    // value must be set inside?
    run();

    if (Definitions.empty())
        throw SW_RUNTIME_ERROR("Check " + data + ": definition was not set");
    if (!Value)
    {
        if (requires_manual_setup)
        {
            LOG_INFO(logger, "Check " << *Definitions.begin() << " requires to be set up manually");
            return;
        }
        throw SW_RUNTIME_ERROR("Check " + *Definitions.begin() + ": value was not set");
    }
    LOG_DEBUG(logger, "Checking " << *Definitions.begin() << ": " << Value.value());
}

std::vector<CheckPtr> Check::gatherDependencies()
{
    std::vector<CheckPtr> deps;
    for (auto &d : Parameters.Includes)
        deps.push_back(check_set->add<IncludeExists>(d));
    return deps;
}

bool Check::lessDuringExecution(const Check &rhs) const
{
    // improve sorting! it's too stupid
    // simple "0 0 0 0 1 2 3 6 7 8 9 11" is not enough

    if (dependencies.size() != rhs.dependencies.size())
        return dependencies.size() < rhs.dependencies.size();
    return dependendent_commands.size() > dependendent_commands.size();
}

path Check::getOutputFilename() const
{
    auto d = check_set->checker.solution->getChecksDir();
    //static std::atomic_int64_t n = 0;
    auto up = unique_path();
    //auto up = std::to_string(++n);
    d /= up;
    //::create_directories(d);
    auto f = d;
    if (!CPP)
        f /= "x.c";
    else
        f /= "x.cpp";
    return f;
}

static path getUniquePath(const path &p)
{
    return p.parent_path().filename();
}

Solution Check::setupSolution(const path &f) const
{
    auto s = *check_set->checker.solution;
    s.silent = bSilentChecks;
    s.command_storage = builder::Command::CS_DO_NOT_SAVE;
    //s.throw_exceptions = false;
    s.BinaryDir = f.parent_path();
    return s;
}

bool Check::execute(Solution &s) const
{
    s.prepare();
    try
    {
        auto p = s.getExecutionPlan();
        for (auto &c : p.commands)
            commands.push_back(c->shared_from_this());
        s.execute(p);
    }
    catch (std::exception &e)
    {
        Value = 0;
        LOG_TRACE(logger, "Check " + data + ": check issue: " << e.what());
        return false;
    }
    catch (...)
    {
        Value = 0;
        LOG_TRACE(logger, "Check " + data + ": check unknown issue");
        return false;
    }
    return true;
}

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

    auto s = setupSolution(f);

    auto &e = s.addTarget<ExecutableTarget>(getUniquePath(f).string());
    e.GenerateWindowsResource = false;
    if (auto L = e.getSelectedTool()->as<VisualStudioLinker>())
        L->DisableIncrementalLink = true;
    e += f;
    e.Definitions["CHECK_FUNCTION_EXISTS"] = data;

    if (!execute(s))
        return;

    auto cmd = e.getCommand();
    Value = (cmd && cmd->exit_code && cmd->exit_code.value() == 0) ? 1 : 0;
}

IncludeExists::IncludeExists(const String &i, const String &def)
{
    if (i.empty())
        throw SW_RUNTIME_ERROR("Empty include");
    data = i;

    if (def.empty())
        Definitions.insert(make_include_var(data));
    else
        Definitions.insert(def);

    check_def(*Definitions.begin());
}

String IncludeExists::getSourceFileContents() const
{
    String src = "#include <" + data + ">";
    if (!CPP)
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

    auto c = std::dynamic_pointer_cast<NativeCompiler>(check_set->checker.solution->findProgramByExtension(f.extension().string())->clone());
    auto o = f;
    c->setSourceFile(f, o += ".obj");

    auto cmd = c->getCommand(*check_set->checker.solution);
    cmd->command_storage = builder::Command::CS_DO_NOT_SAVE;
    if (!cmd)
    {
        Value = 0;
        return;
    }
    error_code ec;
    cmd->execute(ec);
    Value = (cmd->exit_code && cmd->exit_code.value() == 0) ? 1 : 0;
}

TypeSize::TypeSize(const String &t, const String &def)
{
    if (t.empty())
        throw SW_RUNTIME_ERROR("Empty type");
    data = t;

    Definitions.insert(make_type_var(data));
    Definitions.insert(make_type_var(data, "SIZEOF_"));
    Definitions.insert(make_type_var(data, "SIZE_OF_"));
    // some libs want these
    Definitions.insert(make_type_var(data, "HAVE_SIZEOF_"));
    Definitions.insert(make_type_var(data, "HAVE_SIZE_OF_"));

    if (!def.empty())
        Definitions.insert(def);

    check_def(*Definitions.begin());

    for (auto &h : { "sys/types.h", "stdint.h", "stddef.h", "inttypes.h" })
        Parameters.Includes.push_back(h);
}

String TypeSize::getSourceFileContents() const
{
    String src;
    for (auto &d : Parameters.Includes)
    {
        auto c = check_set->get<IncludeExists>(d);
        if (c->Value && c->Value.value())
            src += "#include <" + d + ">\n";
    }
    src += "int main() { return sizeof(" + data + "); }";

    return src;
}

void TypeSize::run() const
{
    auto f = getOutputFilename();
    write_file(f, getSourceFileContents());

    auto s = setupSolution(f);

    auto &e = s.addTarget<ExecutableTarget>(getUniquePath(f).string());
    e.GenerateWindowsResource = false;
    if (auto L = e.getSelectedTool()->as<VisualStudioLinker>())
        L->DisableIncrementalLink = true;
    e += f;

    if (!execute(s))
        return;

    auto cmd = e.getCommand();
    if (!cmd)
    {
        Value = 0;
        return;
    }

    if (!s.canRunTargetExecutables())
    {
        requires_manual_setup = true;
        executable = e.getOutputFile();
        return;
    }

    primitives::Command c;
    c.program = e.getOutputFile();
    error_code ec;
    c.execute(ec);
    Value = c.exit_code;
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
        auto c = check_set->get<IncludeExists>(d);
        if (c->Value && c->Value.value())
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

    auto s = setupSolution(f);

    auto &e = s.addTarget<ExecutableTarget>(getUniquePath(f).string());
    e.GenerateWindowsResource = false;
    if (auto L = e.getSelectedTool()->as<VisualStudioLinker>())
        L->DisableIncrementalLink = true;
    e += f;

    if (!execute(s))
        return;

    auto cmd = e.getCommand();
    if (!cmd)
    {
        Value = 0;
        return;
    }

    if (!s.canRunTargetExecutables())
    {
        requires_manual_setup = true;
        executable = e.getOutputFile();
        return;
    }

    primitives::Command c;
    c.program = e.getOutputFile();
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
        auto c = check_set->get<IncludeExists>(d);
        if (c->Value && c->Value.value())
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

    auto s = setupSolution(f);

    auto &e = s.addTarget<ExecutableTarget>(getUniquePath(f).string());
    e.GenerateWindowsResource = false;
    if (auto L = e.getSelectedTool()->as<VisualStudioLinker>())
        L->DisableIncrementalLink = true;
    e += f;

    if (!execute(s))
        return;

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
        auto c = check_set->get<IncludeExists>(d);
        if (c->Value && c->Value.value())
            src += "#include <" + d + ">\n";
    }
    src += "int main() { (void)" + data + "; return 0; }";

    return src;
}

void DeclarationExists::run() const
{
    auto f = getOutputFilename();
    write_file(f, getSourceFileContents());

    auto s = setupSolution(f);

    auto &e = s.addTarget<ExecutableTarget>(getUniquePath(f).string());
    e.GenerateWindowsResource = false;
    if (auto L = e.getSelectedTool()->as<VisualStudioLinker>())
        L->DisableIncrementalLink = true;
    e += f;

    if (!execute(s))
        return;

    auto cmd = e.getCommand();
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
        auto c = check_set->get<IncludeExists>(d);
        if (c->Value && c->Value.value())
            src += "#include <" + d + ">\n";
    }
    src += "int main() { sizeof(((" + struct_ + " *)0)->" + member + "); return 0; }";

    return src;
}

void StructMemberExists::run() const
{
    auto f = getOutputFilename();
    write_file(f, getSourceFileContents());

    auto s = setupSolution(f);

    auto &e = s.addTarget<ExecutableTarget>(getUniquePath(f).string());
    e.GenerateWindowsResource = false;
    if (auto L = e.getSelectedTool()->as<VisualStudioLinker>())
        L->DisableIncrementalLink = true;
    e += f;

    if (!execute(s))
        return;

    auto cmd = e.getCommand();
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

String LibraryFunctionExists::getSourceFileContents() const
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

void LibraryFunctionExists::run() const
{
    auto f = getOutputFilename();
    write_file(f, getSourceFileContents());

    auto s = setupSolution(f);

    auto &e = s.addTarget<ExecutableTarget>(getUniquePath(f).string());
    e.GenerateWindowsResource = false;
    if (auto L = e.getSelectedTool()->as<VisualStudioLinker>())
        L->DisableIncrementalLink = true;
    e += f;
    e.Definitions["CHECK_FUNCTION_EXISTS"] = data;
    e.NativeLinkerOptions::System.LinkLibraries.push_back(library);

    if (!execute(s))
        return;

    auto cmd = e.getCommand();
    Value = (cmd && cmd->exit_code && cmd->exit_code.value() == 0) ? 1 : 0;
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

    auto c = std::dynamic_pointer_cast<NativeCompiler>(check_set->checker.solution->findProgramByExtension(f.extension().string())->clone());
    auto o = f;
    c->setSourceFile(f, o += ".obj");

    auto cmd = c->getCommand(*check_set->checker.solution);
    cmd->command_storage = builder::Command::CS_DO_NOT_SAVE;
    if (!cmd)
    {
        Value = 0;
        return;
    }
    error_code ec;
    cmd->execute(ec);
    Value = (cmd->exit_code && cmd->exit_code.value() == 0) ? 1 : 0;
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

    auto c = std::dynamic_pointer_cast<NativeCompiler>(check_set->checker.solution->findProgramByExtension(f.extension().string())->clone());

    auto s = setupSolution(f);

    auto &e = s.addTarget<ExecutableTarget>(getUniquePath(f).string());
    e.GenerateWindowsResource = false;
    if (auto L = e.getSelectedTool()->as<VisualStudioLinker>())
        L->DisableIncrementalLink = true;
    e += f;

    if (!execute(s))
        return;

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

    auto s = setupSolution(f);

    auto &e = s.addTarget<ExecutableTarget>(getUniquePath(f).string());
    e.GenerateWindowsResource = false;
    if (auto L = e.getSelectedTool()->as<VisualStudioLinker>())
        L->DisableIncrementalLink = true;
    e += f;

    if (!execute(s))
        return;

    auto cmd = e.getCommand();
    if (!cmd)
    {
        Value = 0;
        return;
    }

    if (!s.canRunTargetExecutables())
    {
        requires_manual_setup = true;
        executable = e.getOutputFile();
        return;
    }

    primitives::Command c;
    c.program = e.getOutputFile();
    error_code ec;
    c.execute(ec);
    Value = c.exit_code;
}

FunctionExists &CheckSet::checkFunctionExists(const String &function, LanguageType L)
{
    auto c = add<FunctionExists>(function);
    c->CPP = L == LanguageType::CPP;
    return *c;
}

FunctionExists &CheckSet::checkFunctionExists(const String &function, const String &def, LanguageType L)
{
    auto c = add<FunctionExists>(function, def);
    c->CPP = L == LanguageType::CPP;
    return *c;
}

Check &CheckSet::checkIncludeExists(const String &include, LanguageType L)
{
    auto c = add<IncludeExists>(include);
    c->CPP = L == LanguageType::CPP;
    return *c;
}

Check &CheckSet::checkIncludeExists(const String &include, const String &def, LanguageType L)
{
    auto c = add<IncludeExists>(include, def);
    c->CPP = L == LanguageType::CPP;
    return *c;
}

Check &CheckSet::checkLibraryFunctionExists(const String &library, const String &function, LanguageType L)
{
    auto c = add<LibraryFunctionExists>(library, function);
    c->CPP = L == LanguageType::CPP;
    return *c;
}

Check &CheckSet::checkLibraryFunctionExists(const String &library, const String &function, const String &def, LanguageType L)
{
    auto c = add<LibraryFunctionExists>(library, function, def);
    c->CPP = L == LanguageType::CPP;
    return *c;
}

Check &CheckSet::checkLibraryExists(const String &library, LanguageType L)
{
    return *add<FunctionExists>(library);
}

Check &CheckSet::checkLibraryExists(const String &library, const String &def, LanguageType L)
{
    return *add<FunctionExists>(library);
}

Check &CheckSet::checkSymbolExists(const String &symbol, LanguageType L)
{
    auto c = add<SymbolExists>(symbol);
    c->CPP = L == LanguageType::CPP;
    return *c;
}

Check &CheckSet::checkSymbolExists(const String &symbol, const String &def, LanguageType L)
{
    auto c = add<SymbolExists>(symbol, def);
    c->CPP = L == LanguageType::CPP;
    return *c;
}

Check &CheckSet::checkStructMemberExists(const String &s, const String &member, LanguageType L)
{
    auto c = add<StructMemberExists>(s, member);
    c->CPP = L == LanguageType::CPP;
    return *c;
}

Check &CheckSet::checkStructMemberExists(const String &s, const String &member, const String &def, LanguageType L)
{
    auto c = add<StructMemberExists>(s, member, def);
    c->CPP = L == LanguageType::CPP;
    return *c;
}

Check &CheckSet::checkDeclarationExists(const String &decl, LanguageType L)
{
    auto c = add<DeclarationExists>(decl);
    c->CPP = L == LanguageType::CPP;
    return *c;
}

Check &CheckSet::checkDeclarationExists(const String &decl, const String &def, LanguageType L)
{
    auto c = add<DeclarationExists>(decl, def);
    c->CPP = L == LanguageType::CPP;
    return *c;
}

Check &CheckSet::checkTypeSize(const String &type, LanguageType L)
{
    auto c = add<TypeSize>(type);
    c->CPP = L == LanguageType::CPP;
    return *c;
}

Check &CheckSet::checkTypeSize(const String &type, const String &def, LanguageType L)
{
    auto c = add<TypeSize>(type, def);
    c->CPP = L == LanguageType::CPP;
    return *c;
}

Check &CheckSet::checkTypeAlignment(const String &type, LanguageType L)
{
    auto c = add<TypeAlignment>(type);
    c->CPP = L == LanguageType::CPP;
    return *c;
}

Check &CheckSet::checkTypeAlignment(const String &type, const String &def, LanguageType L)
{
    auto c = add<TypeAlignment>(type, def);
    c->CPP = L == LanguageType::CPP;
    return *c;
}

Check &CheckSet::checkSourceCompiles(const String &def, const String &src, LanguageType L)
{
    auto c = add<SourceCompiles>(def, src);
    c->CPP = L == LanguageType::CPP;
    return *c;
}

Check &CheckSet::checkSourceLinks(const String &def, const String &src, LanguageType L)
{
    auto c = add<SourceLinks>(def, src);
    c->CPP = L == LanguageType::CPP;
    return *c;
}

Check &CheckSet::checkSourceRuns(const String &def, const String &src, LanguageType L)
{
    auto c = add<SourceRuns>(def, src);
    c->CPP = L == LanguageType::CPP;
    return *c;
}

void CheckSet::prepareChecksForUse()
{
    for (auto &[h, c] : checks)
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
    }
}

}
