#pragma once

#include <vector>

namespace sw
{

struct Package
{
    void getPath() const;
    void getVersion() const;
    void getFlags() const;
};

struct Command
{
    virtual ~Command() = default;

    virtual void execute() const = 0;
};

// or just pair?
// or json for maximum flexibility?
struct TargetDescription
{
    std::string name; // key
    std::string value;
    // other types?
    // variant?
    // arrays?
};

// TObject?
// TargetObject?
// metatarget?
struct Target
{
    virtual ~Target() = default;

    virtual const Package &getPackage() const = 0;

    // get KV parameters to display on the page
    virtual std::vector<TargetDescription> getDescriptor() const = 0;

    virtual std::vector<Command> getCommands() const = 0;
    // git dirs? or extract from commands?

    // target can return its config
    // getConfig(); // config can have its name
    // getConfigName() ?

    // we also can push config to it
    // setConfig

    // extracting deps
    // getDependencies()?

    /*
    target can ask about available packages
    it can request needed package
    */
};

// drivers? - control deps resolving for targets?

// solution - single config?

// build - whole build

// sw - global class?

}
