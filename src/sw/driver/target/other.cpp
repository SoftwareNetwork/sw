#include "other.h"

#include "sw/driver/build.h"
#include "sw/driver/sw_context.h"

#include <sw/manager/storage.h>

namespace sw
{

bool CSharpTarget::init()
{
    Target::init();

    // propagate this pointer to all
    TargetOptionsGroup::iterate([this](auto &v, auto i)
    {
        v.target = this;
    });

    if (auto p = findProgramByExtension(".cs"); p)
        compiler = std::dynamic_pointer_cast<CSharpCompiler>(p->clone());
    else
        throw SW_RUNTIME_ERROR("No C# compiler found");

    /* || add a considiton so user could change nont build output dir*/
    if (Scope == TargetScope::Build)
    {
        SW_UNIMPLEMENTED;
        //compiler->setOutputFile(getOutputFileName(getSolution().swctx.getLocalStorage().storage_dir_bin));
    }
    else
    {
        auto base = BinaryDir.parent_path() / "out" / getOutputFileName();
        compiler->setOutputFile(base);
    }

    SW_RETURN_MULTIPASS_END;
}

path CSharpTarget::getOutputFileName(const path &root) const
{
    path p;
    if (isLocal())
    {
        p = getTargetsDir().parent_path() / getOutputFileName();
    }
    else
    {
        p = root / getConfig() / getOutputFileName();
    }
    return p;
}

Commands CSharpTarget::getCommands1() const
{
    SW_UNIMPLEMENTED;

    /*for (auto f : gatherSourceFiles<SourceFile>(*this, compiler->input_extensions))
        compiler->addSourceFile(f->file);

    Commands cmds;
    auto c = compiler->getCommand(*this);
    cmds.insert(c);
    return cmds;*/
}

bool RustTarget::init()
{
    Target::init();

    // propagate this pointer to all
    TargetOptionsGroup::iterate([this](auto &v, auto i)
    {
        v.target = this;
    });

    if (auto p = findProgramByExtension(".rs"); p)
        compiler = std::dynamic_pointer_cast<RustCompiler>(p->clone());
    else
        throw SW_RUNTIME_ERROR("No Rust compiler found");

    /* || add a considiton so user could change nont build output dir*/
    if (Scope == TargetScope::Build)
    {
        SW_UNIMPLEMENTED;
        //compiler->setOutputFile(getOutputFileName(getSolution().swctx.getLocalStorage().storage_dir_bin));
    }
    else
    {
        auto base = BinaryDir.parent_path() / "out" / getOutputFileName();
        compiler->setOutputFile(base);
    }

    SW_RETURN_MULTIPASS_END;
}

path RustTarget::getOutputFileName(const path &root) const
{
    path p;
    if (isLocal())
    {
        p = getTargetsDir().parent_path() / getOutputFileName();
    }
    else
    {
        p = root / getConfig() / getOutputFileName();
    }
    return p;
}

Commands RustTarget::getCommands1() const
{
    SW_UNIMPLEMENTED;

    /*for (auto f : gatherSourceFiles<SourceFile>(*this, compiler->input_extensions))
        compiler->setSourceFile(f->file);

    Commands cmds;
    auto c = compiler->getCommand(*this);
    cmds.insert(c);
    return cmds;*/
}

bool GoTarget::init()
{
    Target::init();

    // propagate this pointer to all
    TargetOptionsGroup::iterate([this](auto &v, auto i)
    {
        v.target = this;
    });

    if (auto p = findProgramByExtension(".go"); p)
        compiler = std::dynamic_pointer_cast<GoCompiler>(p->clone());
    else
        throw SW_RUNTIME_ERROR("No Go compiler found");

    /* || add a considiton so user could change nont build output dir*/
    if (Scope == TargetScope::Build)
    {
        SW_UNIMPLEMENTED;
        //compiler->setOutputFile(getOutputFileName(getSolution().swctx.getLocalStorage().storage_dir_bin));
    }
    else
    {
        auto base = BinaryDir.parent_path() / "out" / getOutputFileName();
        compiler->setOutputFile(base);
    }

    SW_RETURN_MULTIPASS_END;
}

path GoTarget::getOutputFileName(const path &root) const
{
    path p;
    if (isLocal())
    {
        p = getTargetsDir().parent_path() / getOutputFileName();
    }
    else
    {
        p = root / getConfig() / getOutputFileName();
    }
    return p;
}

Commands GoTarget::getCommands1() const
{
    SW_UNIMPLEMENTED;

    /*for (auto f : gatherSourceFiles<SourceFile>(*this, compiler->input_extensions))
        compiler->setSourceFile(f->file);

    Commands cmds;
    auto c = compiler->getCommand(*this);
    cmds.insert(c);
    return cmds;*/
}

bool FortranTarget::init()
{
    Target::init();

    // propagate this pointer to all
    TargetOptionsGroup::iterate([this](auto &v, auto i)
    {
        v.target = this;
    });

    if (auto p = findProgramByExtension(".f"); p)
        compiler = std::dynamic_pointer_cast<FortranCompiler>(p->clone());
    else
        throw SW_RUNTIME_ERROR("No Fortran compiler found");

    /* || add a considiton so user could change nont build output dir*/
    if (Scope == TargetScope::Build)
    {
        SW_UNIMPLEMENTED;
        //compiler->setOutputFile(getOutputFileName(getSolution().swctx.getLocalStorage().storage_dir_bin));
    }
    else
    {
        auto base = BinaryDir.parent_path() / "out" / getOutputFileName();
        compiler->setOutputFile(base);
    }

    SW_RETURN_MULTIPASS_END;
}

path FortranTarget::getOutputFileName(const path &root) const
{
    path p;
    if (isLocal())
    {
        p = getTargetsDir().parent_path() / getOutputFileName();
    }
    else
    {
        p = root / getConfig() / getOutputFileName();
    }
    return p;
}

Commands FortranTarget::getCommands1() const
{
    SW_UNIMPLEMENTED;

    /*for (auto f : gatherSourceFiles<SourceFile>(*this, compiler->input_extensions))
        compiler->setSourceFile(f->file);

    Commands cmds;
    auto c = compiler->getCommand(*this);
    cmds.insert(c);
    return cmds;*/
}

bool JavaTarget::init()
{
    Target::init();

    // propagate this pointer to all
    TargetOptionsGroup::iterate([this](auto &v, auto i)
    {
        v.target = this;
    });

    if (auto p = findProgramByExtension(".java"); p)
        compiler = std::dynamic_pointer_cast<JavaCompiler>(p->clone());
    else
        throw SW_RUNTIME_ERROR("No Java compiler found");

    /* || add a considiton so user could change nont build output dir*/
    /*if (Scope == TargetScope::Build)
    {
        //compiler->setOutputFile(getOutputFileName(getSolution().swctx.getLocalStorage().storage_dir_bin));
    }
    else
    {*/
    auto base = BinaryDir.parent_path() / "out";
    compiler->setOutputDir(base);
    //}

    SW_RETURN_MULTIPASS_END;
}

path JavaTarget::getOutputFileName(const path &root) const
{
    path p;
    if (isLocal())
    {
        p = getTargetsDir().parent_path() / getOutputFileName();
    }
    else
    {
        p = root / getConfig() / getOutputFileName();
    }
    return p;
}

Commands JavaTarget::getCommands1() const
{
    SW_UNIMPLEMENTED;

    /*Commands cmds;
    for (auto f : gatherSourceFiles<SourceFile>(*this, compiler->input_extensions))
    {
        compiler->setSourceFile(f->file);
        cmds.insert(compiler->prepareCommand(*this));
    }

    //auto c = compiler->getCommand(*this);
    //cmds.insert(c);
    return cmds;*/
}

bool KotlinTarget::init()
{
    Target::init();

    // propagate this pointer to all
    TargetOptionsGroup::iterate([this](auto &v, auto i)
    {
        v.target = this;
    });

    if (auto p = findProgramByExtension(".kt"); p)
        compiler = std::dynamic_pointer_cast<KotlinCompiler>(p->clone());
    else
        throw SW_RUNTIME_ERROR("No Kotlin compiler found");

    /* || add a considiton so user could change nont build output dir*/
    if (Scope == TargetScope::Build)
    {
        SW_UNIMPLEMENTED;
        //compiler->setOutputFile(getOutputFileName(getSolution().swctx.getLocalStorage().storage_dir_bin));
    }
    else
    {
        auto base = BinaryDir.parent_path() / "out" / getOutputFileName();
        compiler->setOutputFile(base);
    }

    SW_RETURN_MULTIPASS_END;
}

path KotlinTarget::getOutputFileName(const path &root) const
{
    path p;
    if (isLocal())
    {
        p = getTargetsDir().parent_path() / getOutputFileName();
    }
    else
    {
        p = root / getConfig() / getOutputFileName();
    }
    return p;
}

Commands KotlinTarget::getCommands1() const
{
    SW_UNIMPLEMENTED;

    /*for (auto f : gatherSourceFiles<SourceFile>(*this, compiler->input_extensions))
        compiler->setSourceFile(f->file);

    Commands cmds;
    auto c = compiler->getCommand(*this);
    cmds.insert(c);
    return cmds;*/
}

bool DTarget::init()
{
    Target::init();

    // propagate this pointer to all
    TargetOptionsGroup::iterate([this](auto &v, auto i)
    {
        v.target = this;
    });

    if (auto p = findProgramByExtension(".d"); p)
        compiler = std::dynamic_pointer_cast<DCompiler>(p->clone());
    else
        throw SW_RUNTIME_ERROR("No D compiler found");

    /* || add a considiton so user could change nont build output dir*/
    if (Scope == TargetScope::Build)
    {
        SW_UNIMPLEMENTED;
        //compiler->setOutputFile(getOutputFileName(getSolution().swctx.getLocalStorage().storage_dir_bin));
    }
    else
    {
        auto base = BinaryDir.parent_path() / "out" / getOutputFileName();
        compiler->setOutputFile(base);
    }
    compiler->setObjectDir(BinaryDir.parent_path() / "obj");

    SW_RETURN_MULTIPASS_END;
}

path DTarget::getOutputFileName(const path &root) const
{
    path p;
    if (isLocal())
    {
        p = getTargetsDir().parent_path() / getOutputFileName();
    }
    else
    {
        p = root / getConfig() / getOutputFileName();
    }
    return p;
}

Commands DTarget::getCommands1() const
{
    SW_UNIMPLEMENTED;

    /*for (auto f : gatherSourceFiles<SourceFile>(*this, compiler->input_extensions))
        compiler->setSourceFile(f->file);

    Commands cmds;
    auto c = compiler->getCommand(*this);
    cmds.insert(c);
    return cmds;*/
}

}
