/*
 * sw: a client tool for Software Network
 * Copyright (C) 2017-2018 Egor Pugin <egor.pugin@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

#include <command.h>
#include <config.h>
#include <database.h>
#include <directories.h>
#include <exceptions.h>
#include <file.h>
#include <file_storage.h>
#include <resolver.h>
#include <settings.h>

#include <sw/builder/build.h>
#include <sw/builder/driver.h>
#include <sw/driver/cpp/driver.h>

#include <args.hxx>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string_regex.hpp>
#include <boost/dll.hpp>
//#include <boost/nowide/args.hpp>
#include <boost/regex.hpp>
#include <primitives/executor.h>
#include <primitives/minidump.h>
#include <primitives/win32helpers.h>

#include <iostream>

#include <primitives/log.h>
DECLARE_STATIC_LOGGER(logger, "main");

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#include <Objbase.h>
#include <Shlobj.h>

#include <WinReg.hpp>
#endif

using namespace sw;

bool bConsoleMode = true;
bool bUseSystemPause = false;

/*
// check args here to see if we want gui or not!

// 1. if 'uri' arg - console depends
// 2. if no args, no sw.cpp, *.sw files in cwd - gui
*/

int main(int argc, char **argv);
int main1(int argc, char **argv);
int main_setup(int argc, char **argv);
int cppan_main(int argc, char **argv);
void stop();
void setup_log(const std::string &log_level);
std::tuple<bool, std::string> parseCmd(int argc, char **argv);

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
    const std::wstring s = GetCommandLine();
    bConsoleMode = s.find(L"uri sw:") == -1;
    if (bConsoleMode)
    {
        SetupConsole();
    }
    else
    {
        CoInitialize(0);
    }

    return main(__argc, __argv);
}
#endif

#include <sw/driver/cpp/driver.h>
namespace sw::driver::cpp
{

SW_REGISTER_PACKAGE_DRIVER(Driver);

}

int main(int argc, char **argv)
{
#ifndef _WIN32
    auto r = main1(argc, argv);
    return r;
#else
    primitives::minidump::dir = L"cppan2\\dump";
    primitives::minidump::v_major = VERSION_MAJOR;
    primitives::minidump::v_minor = VERSION_MINOR;
    primitives::minidump::v_patch = VERSION_PATCH;
    primitives::executor::bExecutorUseSEH = true;

    __try
    {
        auto r = main1(argc, __argv);
        return r;
    }
    __except (PRIMITIVES_GENERATE_DUMP)
    {
        return 1;
    }
#endif
}

int main1(int argc, char **argv)
{
    int r = 0;
    String error;
    bool supress = false;
    try
    {
        r = main_setup(argc, argv);
    }
    catch (SupressOutputException &)
    {
        supress = true;
    }
    catch (const std::exception &e)
    {
        error = e.what();
        //if (auto st = boost::get_error_info<traced_exception>(e))
        //    std::cerr << *st << '\n';
    }
    catch (...)
    {
        error = "Unhandled unknown exception\n";
        //if (auto st = boost::get_error_info<traced_exception>(e))
        //    std::cerr << *st << '\n';
    }

    stop();

    if (!error.empty() || supress)
    {
        if (!supress)
        {
            LOG_ERROR(logger, error);
#ifdef _WIN32
            system("pause");
#endif
        }
        r = 1;

        if (!bConsoleMode)
        {
#ifdef _WIN32
            if (bUseSystemPause)
                system("pause");
            else
                message_box(error);
#endif
        }
    }

    LOG_FLUSH();

    return r;
}



#include <property_tree.h>

struct Clonable
{
    virtual ~Clonable() = default;
    virtual Clonable *clone() const = 0;
};

namespace detail
{

template <class Key, class Data, template <class...> class Container = std::map, class Compare = std::less<Key>>
struct Node
{
    Data data;
    Container<Key, std::unique_ptr<Node>, Compare> children;
};


struct PropertyTree
{
    //toBoostPtree
};


}




/////////////////

#include <boost/core/demangle.hpp>

std::string demangle(const char *name)
{
    /*int status = -4; // some arbitrary value to eliminate the compiler warning

    std::unique_ptr<char, void(*)(void *)> res{
    abi::__cxa_demangle(name, NULL, NULL, &status)
    , std::free };

    return (status == 0) ? res.get() : name;*/

    auto n = boost::core::demangle(name);
#ifdef _MSC_VER
    if (n.find("class") == 0)
        n = n.substr(6);
    else if (n.find("struct") == 0)
        n = n.substr(7);
#endif
    return n;
}

template <class Base, class... Args>
struct Factory
{
    template <class... T>
    static std::unique_ptr<Base> make(const std::string &s, T &&... args)
    {
        return data().at(s)(std::forward<T>(args)...);
    }

    template <class T>
    struct Registrar : Base
    {
        static bool registerType()
        {
            const auto name = demangle(typeid(T).name());
            Factory::data()[name] = [](Args... args) -> std::unique_ptr<Base>
            {
                return std::make_unique<T>(std::forward<Args>(args)...);
            };
            return true;
        }
        static inline bool registered = Factory<Base, Args...>::Registrar<T>::registerType();

    private:
        Registrar() : Base(Key{}) {}

        friend T;
    };

private:
    class Key
    {
        Key() {}

        template <class T>
        friend struct Registrar;
    };

    using FuncType = std::unique_ptr<Base>(*)(Args...);

    Factory() = default;

    static auto &data()
    {
        static std::unordered_map<std::string, FuncType> s;
        return s;
    }

    friend Base;
};

struct Animal : Factory<Animal>
{
    Animal(Key) {}
    virtual ~Animal() = default;
    virtual void makeNoise() = 0;
};

struct Dog : Animal::Registrar<Dog>
{
    void makeNoise()
    {
        std::cerr << "Dog: " << "\n";
    }
};

namespace a::b::c::d
{

struct Dog2 : Animal::Registrar<Dog2>
{
    void makeNoise()
    {
        std::cerr << "Dog: " << "\n";
    }
};

}

struct Cat : Animal::Registrar<Cat>
{
    void makeNoise()
    {
        std::cerr << "Cat: " << "\n";
    }
};

struct Creature : Factory<Creature, std::unique_ptr<int>>
{
    Creature(Key)
    {
    }
    virtual void makeNoise() = 0;
};

class Ghost : public Creature::Registrar<Ghost>
{
public:
    Ghost(std::unique_ptr<int> &&x)
        : m_x(*x)
    {
    }

    void makeNoise()
    {
        std::cerr << "Ghost: " << m_x << "\n";
    }

private:
    int m_x;
};

struct pkg //_item? // creator?
{
    String type;

    /*create() or get()
    {
    this pkg
    return typeid(type)::make(pkg.id, args...);
    }*/

    // cast<real type>(pkg.get());
};

#include <property.h>

#include <boost/property_tree/info_parser.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/property_tree/xml_parser.hpp>

int main_setup(int argc, char **argv)
{
    //create_directories("xxx");

    /*auto a = Animal::make("a::b::c::d::Dog2");

    auto x = Animal::make("Dog");
    auto y = Animal::make("Cat");
    x->makeNoise();
    y->makeNoise();
    auto z = Creature::make("Ghost", std::make_unique<int>(4));
    z->makeNoise();
    return 0;*/


    {
        ptree p;
        p.put("a.b.c", 1);
        p.put("a.b.c.d", 1);
        p.add("a.b.c.d", 1.5);
        p.add("a.b.c.d", 1.5f);
        p.add("a.b.c.d", "x");
        p.add("a.b.c.d", "5");
        p.add("a.b.c.d", "5 55");
        p.add("a.b.c.d", "a b");
        p.add(pt::path("a|b.c|d", '|'), "a b");

        {
            std::ofstream o("d:\\1.info");
            boost::property_tree::write_info(o, p);
        }
    }


    // fix arguments - make them UTF-8
    //boost::nowide::args wargs(argc, argv);

#ifdef NDEBUG
    setup_log("INFO");
#else
    setup_log("DEBUG");
#endif

    getServiceDatabase();

    return cppan_main(argc, argv);
}

int cppan_main(int argc, char **argv)
{
    if (auto r = parseCmd(argc, argv); !std::get<0>(r))
    {
        if (!sw::build(current_thread_path()))
            LOG_INFO(logger, std::get<1>(r));
    }
    return 0;
}

void stop()
{
    getExecutor().join();
}

void setup_log(const std::string &log_level)
{
    LoggerSettings log_settings;
    log_settings.log_level = log_level;
    if (bConsoleMode)
        log_settings.log_file = (get_root_directory() / "cppan").string();
    log_settings.simple_logger = true;
    log_settings.print_trace = true;
    initLogger(log_settings);

    // first trace message
    LOG_TRACE(logger, "----------------------------------------");
    LOG_TRACE(logger, "Starting cppan...");
}

#define SUBCOMMAND_DECL(x) \
    void cli_##x(const std::string &progname, std::vector<std::string>::const_iterator beginargs, std::vector<std::string>::const_iterator endargs)

#define SUBCOMMAND(x) SUBCOMMAND_DECL(x);
#include <commands.inl>
#undef SUBCOMMAND

using command_type = std::function<void(const std::string &, std::vector<std::string>::const_iterator, std::vector<std::string>::const_iterator)>;

#define SUBCOMMAND_DECL_URI(c) SUBCOMMAND_DECL(uri_ ## c)

std::tuple<bool, String> parseCmd(int argc, char **argv)
{
    const std::unordered_map<std::string, command_type> map{
#define SUBCOMMAND(x) { #x, cli_##x },
#include <commands.inl>
#undef SUBCOMMAND
    };
    String command_to_execute;
    for (auto &[k, v] : map)
        command_to_execute += k + ", ";
    command_to_execute.resize(command_to_execute.size() - 2);

    args::ArgumentParser parser("cppan client v2 (0.3.0)");
    parser.helpParams.showTerminator = false;
    args::HelpFlag help(parser, "help", "Display this help menu", { 'h', "help" });
    args::Flag force_server_query(parser, "server", "Force server check", { 's' });
    args::ValueFlag<std::string> working_directory(parser, "working_directory", "Working directory", { 'd' });
    args::ValueFlag<int> configuration(parser, "configuration", "Configuration to build", { 'c' });
    //args::ValueFlag<std::string> generator(parser, "generator", "Generator", { 'G' });
    args::Flag explain_outdated(parser, "explain_outdated", "Explain outdated files", { "explain" });
    args::Flag print_commands(parser, "print_commands", "Print file with build commands", { "commands" });
    args::Flag verbose(parser, "verbose", "Verbose output", { 'v', "verbose" });
    args::Flag trace(parser, "trace", "Trace output", { "trace" });
    parser.Prog(argv[0]);
    args::MapPositional<std::string, command_type> command(parser, "command", "Command to execute: {" + command_to_execute + "}", map);
    command.KickOut(true);

    const std::vector<std::string> args0(argv + 1, argv + argc);
    std::vector<std::string> args;
    for (auto &a : args0)
    {
        std::vector<std::string> t;
        boost::split_regex(t, a, boost::regex("%20"));
        args.insert(args.end(), t.begin(), t.end());
    }
    auto next = parser.ParseArgs(args);

    if (verbose)
        setup_log("DEBUG");
    if (trace)
        setup_log("TRACE");
    if (force_server_query)
        Settings::get_user_settings().force_server_query = true;
    if (working_directory)
        fs::current_path(working_directory.Get());
    if (explain_outdated)
        Settings::get_user_settings().explain_outdated = true;
    if (print_commands)
        Settings::get_user_settings().print_commands = true;
    if (configuration)
        Settings::get_user_settings().configuration = configuration.Get();

    if (command)
    {
        args::get(command)(argv[0], next, std::end(args));
        return { true, "" };
    }
    return { false, parser.Help() };
}

SUBCOMMAND_DECL(build)
{
    args::ArgumentParser parser("");
    parser.helpParams.showTerminator = false;
    args::HelpFlag help(parser, "help", "Display this help menu", { 'h', "help" });
    args::Positional<std::string> fn(parser, "name", "File or directory to build", ".");

    parser.ParseArgs(beginargs, endargs);
    if (fn)
        sw::build(fn.Get());
}

SUBCOMMAND_DECL(ide)
{
    args::ArgumentParser parser("");
    parser.helpParams.showTerminator = false;
    args::HelpFlag help(parser, "help", "Display this help menu", { 'h', "help" });
    args::ValueFlag<std::string> generator(parser, "generator", "Generator", { 'g', 'G' });
    args::Flag clean(parser, "clean", "Clean", { "clean" });
    args::Flag rebuild(parser, "rebuild", "Rebuild", { "rebuild" });
    args::Positional<std::string> name(parser, "name", "File or directory to build", ".");

    parser.ParseArgs(beginargs, endargs);

    if (generator || (!clean && !rebuild && !name))
        Settings::get_user_settings().generator = generator.Get();

    //build(args::get(name));
    if (fs::exists("sw.cpp"))
    {
        sw::build("sw.cpp"s);
    }
    else
    {
        LOG_INFO(logger, parser);
    }
}

SUBCOMMAND_DECL(init)
{
    elevate();

#ifdef _WIN32
    auto prog = boost::dll::program_location().wstring();

    // set common environment variable
    //winreg::RegKey env(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment");
    //env.SetStringValue(L"SW_TOOL", boost::dll::program_location().wstring());

    // set up protocol handler
    {
        const std::wstring id = L"sw";

        winreg::RegKey url(HKEY_CLASSES_ROOT, id);
        url.SetStringValue(L"URL Protocol", L"");

        winreg::RegKey icon(HKEY_CLASSES_ROOT, id + L"\\DefaultIcon");
        icon.SetStringValue(L"", prog);

        winreg::RegKey open(HKEY_CLASSES_ROOT, id + L"\\shell\\open\\command");
        open.SetStringValue(L"", prog + L" uri %1");
    }

    // register .sw extension
    {
        const std::wstring id = L"sw.1";

        winreg::RegKey ext(HKEY_CLASSES_ROOT, L".sw");
        ext.SetStringValue(L"", id);

        winreg::RegKey icon(HKEY_CLASSES_ROOT, id + L"\\DefaultIcon");
        icon.SetStringValue(L"", prog);

        winreg::RegKey p(HKEY_CLASSES_ROOT, id + L"\\shell\\open\\command");
        p.SetStringValue(L"", prog + L" build %1");
    }
#endif
}

SUBCOMMAND_DECL_URI(sdir)
{
    /*args::ArgumentParser parser("");
    parser.helpParams.showTerminator = false;
    args::HelpFlag help(parser, "help", "Display this help menu", { 'h', "help" });
    args::Positional<std::string> package(parser, "package", "Opens package source dir");

    auto next = parser.ParseArgs(beginargs, endargs);
    if (package)
    {
        auto p = extractFromString(package.Get());

        auto &sdb = getServiceDatabase();
#ifdef _WIN32
        if (sdb.isPackageInstalled(p))
        {
            auto pidl = ILCreateFromPath(p.getDirSrc2().wstring().c_str());
            if (pidl)
            {
                SHOpenFolderAndSelectItems(pidl, 0, 0, 0);
                ILFree(pidl);
            }
        }
        else
        {
            message_box("Package '" + p.target_name + "' not installed");
        }
#endif
    }*/
}

SUBCOMMAND_DECL_URI(install)
{
    args::ArgumentParser parser("");
    parser.helpParams.showTerminator = false;
    args::HelpFlag help(parser, "help", "Display this help menu", { 'h', "help" });
    args::Positional<std::string> package(parser, "package", "Install package");

    auto next = parser.ParseArgs(beginargs, endargs);
    if (package)
    {
        auto p = extractFromString(package.Get());
        auto p_real = p.resolve();

        auto &sdb = getServiceDatabase();
#ifdef _WIN32
        if (!sdb.isPackageInstalled(p_real))
        {
            SetupConsole();
            bUseSystemPause = true;
            Resolver r;
            r.resolve_dependencies({ p });
        }
        else
        {
            message_box("Package '" + p_real.target_name + "' is already installed");
        }
#endif
    }
}

SUBCOMMAND_DECL_URI(remove)
{
    args::ArgumentParser parser("");
    parser.helpParams.showTerminator = false;
    args::HelpFlag help(parser, "help", "Display this help menu", { 'h', "help" });
    args::Positional<std::string> package(parser, "package", "Remove package");

    auto next = parser.ParseArgs(beginargs, endargs);
    if (package)
    {
        auto p = extractFromString(package.Get()).resolve();
        auto &sdb = getServiceDatabase();
        sdb.removeInstalledPackage(p);
        fs::remove_all(p.getDir());
    }
}

SUBCOMMAND_DECL_URI(build)
{
    args::ArgumentParser parser("");
    parser.helpParams.showTerminator = false;
    args::HelpFlag help(parser, "help", "Display this help menu", { 'h', "help" });
    args::Positional<std::string> package(parser, "package", "Install package");

    auto next = parser.ParseArgs(beginargs, endargs);
    if (package)
    {
        auto p = extractFromString(package.Get());

#ifdef _WIN32
        SetupConsole();
        bUseSystemPause = true;
#endif
        auto d = getUserDirectories().storage_dir_tmp / "build";// / fs::unique_path();
        fs::create_directories(d);
        current_thread_path(d);

        Resolver r;
        r.resolve_dependencies({ p });

        /*Build b;
        b.Local = true;
        b.build_package(package.Get());*/
    }
}

SUBCOMMAND_DECL(uri)
{
    const std::unordered_map<std::string, command_type> map{
#define ADD_COMMAND(c) { "sw:" #c, cli_uri_ ## c }
        ADD_COMMAND(sdir),
        ADD_COMMAND(install),
        ADD_COMMAND(remove),
        ADD_COMMAND(build),
    };
    String command_to_execute;
    for (auto &[k, v] : map)
        command_to_execute += k + ", ";
    command_to_execute.resize(command_to_execute.size() - 2);

    args::ArgumentParser parser("");
    parser.helpParams.showTerminator = false;
    args::HelpFlag help(parser, "help", "Display this help menu", { 'h', "help" });
    args::MapPositional<std::string, command_type> command(parser, "command", "Command to execute: {" + command_to_execute + "}", map);
    command.KickOut(true);

    auto next = parser.ParseArgs(beginargs, endargs);
    if (command)
    {
        args::get(command)(progname, next, endargs);
    }
}
