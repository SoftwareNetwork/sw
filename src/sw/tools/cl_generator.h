#include <primitives/emitter.h>

struct EnumValue
{
    String name;
    String ide_name;

    String getIdeName() const;
};

struct Flag
{
    String name;
    String ide_name;
    String flag;
    String ns; // c++ namespace
    String type;
    String default_value;
    String default_ide_value;
    String ide_value;
    String function;
    String function_current;
    String struct_;
    StringSet properties;
    int order = 0;
    StringMap<EnumValue> enum_vals;
    bool print_to_ide = false;
    bool disabled = false;

    String getTypeWithNs() const;
    String getIdeName() const;

    void printDecl(primitives::CppEmitter &) const;
    void printEnum(primitives::CppEmitter &) const;
    void printStruct(primitives::CppEmitter &) const;
    void printStructFunction(primitives::CppEmitter &) const;
    void printToIde(primitives::CppEmitter &) const;
    void printCommandLine(primitives::CppEmitter &) const;
};

using Flags = std::map<String, Flag>;

struct Type
{
    String name;
    String parent;
    Flags flags;

    mutable bool printed = false;

    void print(primitives::CppEmitter &h, primitives::CppEmitter &cpp) const;

private:
    void printH(primitives::CppEmitter &) const;
    void printCpp(primitives::CppEmitter &) const;
    std::vector<const Flag *> sortFlags() const;
};

using Types = std::map<String, Type>;

struct File
{
    Flags flags;
    Types types;

    void print(primitives::CppEmitter &h, primitives::CppEmitter &cpp) const;

private:
    void print_type(const Type &t, primitives::CppEmitter &h, primitives::CppEmitter &cpp) const;
};
