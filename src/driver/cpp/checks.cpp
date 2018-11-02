// Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "checks.h"

#include <checks_storage.h>
#include <db.h>
#include <solution.h>

#include <filesystem.h>
#include <hash.h>

#include <boost/algorithm/string.hpp>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "checks");

namespace
{

bool bSilentChecks = true;

}

namespace sw
{

ChecksStorage::ChecksStorage()
{
    //load();
}

ChecksStorage::ChecksStorage(const ChecksStorage &rhs)
    : checks(rhs.checks)
{
}

ChecksStorage::~ChecksStorage()
{
    try
    {
        //save();
    }
    catch (...)
    {
        LOG_ERROR(logger, "Error during scripts db save");
    }
}

static void load(const path &fn, ChecksContainer &checks)
{
    std::ifstream i(fn.string());
    if (!i)
    {
        if (fs::exists(fn))
            throw std::runtime_error("Cannot open file: " + fn.string());
        return;
    }
    while (i)
    {
        String d;
        int v;
        i >> d;
        if (!i)
            return;
        i >> v;
        checks[d] = v;
    }
}

static void save(const path &fn, const ChecksContainer &c)
{
    const std::map<String, int> checks(c.begin(), c.end());

    fs::create_directories(fn.parent_path());
    //primitives::filesystem::create(fn);
    std::ofstream o(fn.string());
    if (!o)
        throw std::runtime_error("Cannot open file: " + fn.string());
    for (auto &[d, v] : checks)
        o << d << " " << v << "\n";
}

void ChecksStorage::load(const path &fn)
{
    if (loaded)
        return;
    ::sw::load(fn, checks);
    loaded = true;
}

void ChecksStorage::save(const path &fn) const
{
    ::sw::save(fn, checks);
}

bool ChecksStorage::isChecked(const String &d) const
{
    std::shared_lock<std::shared_mutex> lk(m);
    return checks.find(d) != checks.end();
}

bool ChecksStorage::isChecked(const String &d, int &v) const
{
    std::shared_lock<std::shared_mutex> lk(m);
    auto i = checks.find(d);
    if (i != checks.end())
    {
        v = i->second;
        return true;
    }
    return false;
}

void ChecksStorage::add(const String &d, int v)
{
    if (isChecked(d, v))
        return;
    std::unique_lock<std::shared_mutex> lk(m);
    checks[d] = v;
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
        throw std::runtime_error("Empty check definition");
}

std::optional<String> Check::getDefinition() const
{
    return getDefinition(Definition);
}

std::optional<String> Check::getDefinition(const String &d) const
{
    if (Value == 0)
    {
        if (DefineIfZero)
            return d + "=0";
        else
            return std::nullopt;
    }
    return d + "=" + std::to_string(Value);
}

bool Check::isChecked() const
{
    return checker->solution->checksStorage.isChecked(Definition, Value) &&
        std::all_of(Definitions.begin(), Definitions.end(),
            [this](const auto &d) {
        return checker->solution->checksStorage.isChecked(d, Value) &&
            std::all_of(Prefixes.begin(), Prefixes.end(),
                [this, &d](const auto &p) {
            return checker->solution->checksStorage.isChecked(p + d, Value);
        });
    });
}

void Check::execute()
{
    if (isChecked())
        return;
    run();
}

void Check::updateDependencies()
{
    for (auto &d : Parameters.Includes)
        dependencies.insert(checker->add<IncludeExists>(d));
}

FunctionExists::FunctionExists(const String &f, const String &def)
{
    if (f.empty())
        throw std::runtime_error("Empty function");
    data = f;

    if (def.empty())
        Definition = make_function_var(data);
    else
        Definition = def;

    check_def(Definition);
}

void FunctionExists::run() const
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

    auto d = checker->solution->getChecksDir();
    auto up = unique_path();
    d /= up;
    ::create_directories(d);
    auto f = d;
    if (!CPP)
        f /= "x.c";
    else
        f /= "x.cpp";
    write_file(f, src);

    auto s = *checker->solution;
    s.silent = bSilentChecks;
    //s.throw_exceptions = false;
    s.BinaryDir = d;

    auto &e = s.addTarget<ExecutableTarget>(up.string());
    e += f;
    e.Definitions["CHECK_FUNCTION_EXISTS"] = data;
    s.prepare();
    try
    {
        s.execute();

        auto cmd = e.getCommand();
        if (cmd && cmd->exit_code)
            Value = cmd->exit_code.value() == 0 ? 1 : 0;
        else
            Value = 0;
    }
    catch (...)
    {
        Value = 0;
    }
}

IncludeExists::IncludeExists(const String &i, const String &def)
{
    if (i.empty())
        throw std::runtime_error("Empty include");
    data = i;

    if (def.empty())
        Definition = make_include_var(data);
    else
        Definition = def;

    check_def(Definition);
}

void IncludeExists::run() const
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
        src = R"(
int main()
{
  return 0;
}
)";

    auto d = checker->solution->getChecksDir();
    d /= unique_path();
    ::create_directories(d);
    auto f = d;
    if (!CPP)
        f /= "x.c";
    else
        f /= "x.cpp";
    write_file(f, src);
    auto c = std::dynamic_pointer_cast<NativeCompiler>(checker->solution->findProgramByExtension(f.extension().string())->clone());
    auto o = f;
    c->setSourceFile(f, o += ".obj");

    std::error_code ec;
    auto cmd = c->getCommand();
    cmd->execute(ec);
    if (cmd && cmd->exit_code)
        Value = cmd->exit_code.value() == 0 ? 1 : 0;
    else
        Value = 0;
}

TypeSize::TypeSize(const String &t, const String &def)
{
    if (t.empty())
        throw std::runtime_error("Empty type");
    data = t;

    Definition = make_type_var(data);
    Definitions.insert(make_type_var(data, "SIZEOF_"));
    Definitions.insert(make_type_var(data, "SIZE_OF_"));
    // some libs want these
    Definitions.insert(make_type_var(data, "HAVE_SIZEOF_"));
    Definitions.insert(make_type_var(data, "HAVE_SIZE_OF_"));

    if (!def.empty())
        Definition = def;

    check_def(Definition);
}

void TypeSize::init()
{
    for (auto &h : { "sys/types.h", "stdint.h", "stddef.h", "inttypes.h" })
        Parameters.Includes.push_back(h);
}

void TypeSize::run() const
{
    String src;
    for (auto &d : Parameters.Includes)
    {
        auto c = checker->add<IncludeExists>(d);
        if (c->Value)
            src += "#include <" + d + ">\n";
    }
    src += "int main() { return sizeof(" + data + "); }";

    auto d = checker->solution->getChecksDir();
    auto up = unique_path();
    d /= up;
    ::create_directories(d);
    auto f = d;
    if (!CPP)
        f /= "x.c";
    else
        f /= "x.cpp";
    write_file(f, src);

    auto s = *checker->solution;
    s.silent = bSilentChecks;
    //s.throw_exceptions = false;
    s.BinaryDir = d;

    auto &e = s.addTarget<ExecutableTarget>(up.string());
    e += f;
    s.prepare();
    try
    {
        s.execute();

        auto cmd = e.getCommand();
        if (cmd)
        {
            std::error_code ec;
            primitives::Command c;
            c.program = e.getOutputFile();
            c.execute(ec);
            if (c.exit_code)
                Value = c.exit_code.value();
            else
                Value = 0;
        }
        else
            Value = 0;
    }
    catch (...)
    {
        Value = 0;
    }
}

TypeAlignment::TypeAlignment(const String &t, const String &def)
{
    if (t.empty())
        throw std::runtime_error("Empty type");
    data = t;

    if (def.empty())
        Definition = make_alignment_var(data);
    else
        Definition = def;

    check_def(Definition);
}

void TypeAlignment::init()
{
    for (auto &h : { "sys/types.h", "stdint.h", "stddef.h", "stdio.h", "stdlib.h", "inttypes.h" })
        Parameters.Includes.push_back(h);
}

void TypeAlignment::run() const
{
    String src;
    for (auto &d : Parameters.Includes)
    {
        auto c = checker->add<IncludeExists>(d);
        if (c->Value)
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

    auto d = checker->solution->getChecksDir();
    auto up = unique_path();
    d /= up;
    ::create_directories(d);
    auto f = d;
    if (!CPP)
        f /= "x.c";
    else
        f /= "x.cpp";
    write_file(f, src);

    auto s = *checker->solution;
    s.silent = bSilentChecks;
    //s.throw_exceptions = false;
    s.BinaryDir = d;

    auto &e = s.addTarget<ExecutableTarget>(up.string());
    e += f;
    s.prepare();
    try
    {
        s.execute();

        auto cmd = e.getCommand();
        if (cmd)
        {
            std::error_code ec;
            primitives::Command c;
            c.program = e.getOutputFile();
            c.execute(ec);
            if (c.exit_code)
                Value = c.exit_code.value();
            else
                Value = 0;
        }
        else
            Value = 0;
    }
    catch (...)
    {
        Value = 0;
    }
}

SymbolExists::SymbolExists(const String &s, const String &def)
{
    if (s.empty())
        throw std::runtime_error("Empty symbol");
    data = s;

    if (def.empty())
        Definition = make_function_var(data);
    else
        Definition = def;

    check_def(Definition);
}

void SymbolExists::run() const
{
    String src;
    for (auto &d : Parameters.Includes)
    {
        auto c = checker->add<IncludeExists>(d);
        if (c->Value)
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

    auto d = checker->solution->getChecksDir();
    auto up = unique_path();
    d /= up;
    ::create_directories(d);
    auto f = d;
    if (!CPP)
        f /= "x.c";
    else
        f /= "x.cpp";
    write_file(f, src);

    auto s = *checker->solution;
    s.silent = bSilentChecks;
    //s.throw_exceptions = false;
    s.BinaryDir = d;

    auto &e = s.addTarget<ExecutableTarget>(up.string());
    e += f;
    s.prepare();
    try
    {
        s.execute();

        /*auto cmd = e.getCommand();
        if (cmd && cmd->exit_code)
            Value = cmd->exit_code.value() == 0 ? 1 : 0;
        else*/
        Value = 1;
    }
    catch (...)
    {
        Value = 0;
    }
}

DeclarationExists::DeclarationExists(const String &d, const String &def)
{
    if (d.empty())
        throw std::runtime_error("Empty declaration");
    data = d;

    if (def.empty())
        Definition = make_function_var(data, "HAVE_DECL_");
    else
        Definition = def;

    check_def(Definition);
}

void DeclarationExists::init()
{
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

void DeclarationExists::run() const
{
    String src;
    for (auto &d : Parameters.Includes)
    {
        auto c = checker->add<IncludeExists>(d);
        if (c->Value)
            src += "#include <" + d + ">\n";
    }
    src += "int main() { (void)" + data + "; return 0; }";

    auto d = checker->solution->getChecksDir();
    auto up = unique_path();
    d /= up;
    ::create_directories(d);
    auto f = d;
    if (!CPP)
        f /= "x.c";
    else
        f /= "x.cpp";
    write_file(f, src);

    auto s = *checker->solution;
    s.silent = bSilentChecks;
    //s.throw_exceptions = false;
    s.BinaryDir = d;

    auto &e = s.addTarget<ExecutableTarget>(up.string());
    e += f;
    s.prepare();
    try
    {
        s.execute();

        auto cmd = e.getCommand();
        if (cmd && cmd->exit_code)
            Value = cmd->exit_code.value() == 0 ? 1 : 0;
        else
            Value = 0;
    }
    catch (...)
    {
        Value = 0;
    }
}

StructMemberExists::StructMemberExists(const String &s, const String &member, const String &def)
    : s(s), member(member)
{
    if (s.empty() || member.empty())
        throw std::runtime_error("Empty struct/member");
    data = s + "." + member;

    if (def.empty())
        Definition = make_struct_member_var(s, member);
    else
        Definition = def;

    check_def(Definition);
}

void StructMemberExists::run() const
{
    String src;
    for (auto &d : Parameters.Includes)
    {
        auto c = checker->add<IncludeExists>(d);
        if (c->Value)
            src += "#include <" + d + ">\n";
    }
    src += "int main() { sizeof(((" + s + " *)0)->" + member + "); return 0; }";

    auto d = checker->solution->getChecksDir();
    auto up = unique_path();
    d /= up;
    ::create_directories(d);
    auto f = d;
    if (!CPP)
        f /= "x.c";
    else
        f /= "x.cpp";
    write_file(f, src);

    auto s = *checker->solution;
    s.silent = bSilentChecks;
    //s.throw_exceptions = false;
    s.BinaryDir = d;

    auto &e = s.addTarget<ExecutableTarget>(up.string());
    e += f;
    s.prepare();
    try
    {
        s.execute();

        auto cmd = e.getCommand();
        if (cmd && cmd->exit_code)
            Value = cmd->exit_code.value() == 0 ? 1 : 0;
        else
            Value = 0;
    }
    catch (...)
    {
        Value = 0;
    }
}

LibraryFunctionExists::LibraryFunctionExists(const String &library, const String &function, const String &def)
    : library(library), function(function)
{
    if (library.empty() || function.empty())
        throw std::runtime_error("Empty library/function");
    data = library + "." + function;

    if (def.empty())
        Definition = make_function_var(function);
    else
        Definition = def;

    check_def(Definition);
}

void LibraryFunctionExists::run() const
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

    auto d = checker->solution->getChecksDir();
    auto up = unique_path();
    d /= up;
    ::create_directories(d);
    auto f = d;
    if (!CPP)
        f /= "x.c";
    else
        f /= "x.cpp";
    write_file(f, src);

    auto s = *checker->solution;
    s.silent = bSilentChecks;
    //s.throw_exceptions = false;
    s.BinaryDir = d;

    auto &e = s.addTarget<ExecutableTarget>(up.string());
    e += f;
    e.Definitions["CHECK_FUNCTION_EXISTS"] = data;
    e.LinkLibraries.push_back(library);
    s.prepare();
    try
    {
        s.execute();

        auto cmd = e.getCommand();
        if (cmd && cmd->exit_code)
            Value = cmd->exit_code.value() == 0 ? 1 : 0;
        else
            Value = 0;
    }
    catch (...)
    {
        Value = 0;
    }
}

SourceCompiles::SourceCompiles(const String &def, const String &source)
{
    if (def.empty() || source.empty())
        throw std::runtime_error("Empty def/source");
    data = source;
    Definition = def;
    check_def(Definition);
}

void SourceCompiles::run() const
{
    auto d = checker->solution->getChecksDir();
    d /= unique_path();
    ::create_directories(d);
    auto f = d;
    if (!CPP)
        f /= "x.c";
    else
        f /= "x.cpp";
    write_file(f, data);
    auto c = std::dynamic_pointer_cast<NativeCompiler>(checker->solution->findProgramByExtension(f.extension().string())->clone());
    auto o = f;
    c->setSourceFile(f, o += ".obj");

    std::error_code ec;
    auto cmd = c->getCommand();
    cmd->execute(ec);
    if (cmd && cmd->exit_code)
        Value = cmd->exit_code.value() == 0 ? 1 : 0;
    else
        Value = 0;
}

SourceLinks::SourceLinks(const String &def, const String &source)
{
    if (def.empty() || source.empty())
        throw std::runtime_error("Empty def/source");
    data = source;
    Definition = def;
    check_def(Definition);
}

void SourceLinks::run() const
{
    auto d = checker->solution->getChecksDir();
    auto up = unique_path();
    d /= up;
    ::create_directories(d);
    auto f = d;
    if (!CPP)
        f /= "x.c";
    else
        f /= "x.cpp";
    write_file(f, data);
    auto c = std::dynamic_pointer_cast<NativeCompiler>(checker->solution->findProgramByExtension(f.extension().string())->clone());

    auto s = *checker->solution;
    s.silent = bSilentChecks;
    //s.throw_exceptions = false;
    s.BinaryDir = d;

    auto &e = s.addTarget<ExecutableTarget>(up.string());
    e += f;
    s.prepare();
    try
    {
        s.execute();
        Value = 1;
    }
    catch (...)
    {
        Value = 0;
    }
}

SourceRuns::SourceRuns(const String &def, const String &source)
{
    if (def.empty() || source.empty())
        throw std::runtime_error("Empty def/source");
    data = source;
    Definition = def;
    check_def(Definition);
}

void SourceRuns::run() const
{
    auto d = checker->solution->getChecksDir();
    auto up = unique_path();
    d /= up;
    ::create_directories(d);
    auto f = d;
    if (!CPP)
        f /= "x.c";
    else
        f /= "x.cpp";
    write_file(f, data);
    auto c = std::dynamic_pointer_cast<NativeCompiler>(checker->solution->findProgramByExtension(f.extension().string())->clone());

    auto s = *checker->solution;
    s.silent = bSilentChecks;
    //s.throw_exceptions = false;
    s.BinaryDir = d;

    auto &e = s.addTarget<ExecutableTarget>(up.string());
    e += f;
    s.prepare();
    try
    {
        s.execute();

        auto cmd = e.getCommand();
        if (cmd)
        {
            std::error_code ec;
            primitives::Command c;
            c.program = e.getOutputFile();
            c.execute(ec);
            if (c.exit_code)
                Value = c.exit_code.value();
            else
                Value = 0;
        }
        else
            Value = 0;
    }
    catch (...)
    {
        Value = 0;
    }
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

}
