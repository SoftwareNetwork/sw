#include "other.h"

#include "sw/driver/build.h"

#include <sw/core/sw_context.h>
#include <sw/manager/storage.h>

namespace sw
{

static path getOutputFileName(const Target &t)
{
    SW_UNIMPLEMENTED;
}

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
        auto base = BinaryDir.parent_path() / "out" / ::sw::getOutputFileName(*this);
        compiler->setOutputFile(base);
    }

    SW_RETURN_MULTIPASS_END;
}

path CSharpTarget::getOutputFileName(const path &root) const
{
    path p;
    if (isLocal())
    {
        p = getLocalOutputBinariesDirectory() / ::sw::getOutputFileName(*this);
    }
    else
    {
        p = root / getConfig() / ::sw::getOutputFileName(*this);
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
        auto base = BinaryDir.parent_path() / "out" / ::sw::getOutputFileName(*this);
        compiler->setOutputFile(base);
    }

    SW_RETURN_MULTIPASS_END;
}

path RustTarget::getOutputFileName(const path &root) const
{
    path p;
    if (isLocal())
    {
        p = getLocalOutputBinariesDirectory() / ::sw::getOutputFileName(*this);
    }
    else
    {
        p = root / getConfig() / ::sw::getOutputFileName(*this);
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
        auto base = BinaryDir.parent_path() / "out" / ::sw::getOutputFileName(*this);
        compiler->setOutputFile(base);
    }

    SW_RETURN_MULTIPASS_END;
}

path GoTarget::getOutputFileName(const path &root) const
{
    path p;
    if (isLocal())
    {
        p = getLocalOutputBinariesDirectory() / ::sw::getOutputFileName(*this);
    }
    else
    {
        p = root / getConfig() / ::sw::getOutputFileName(*this);
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
        auto base = BinaryDir.parent_path() / "out" / ::sw::getOutputFileName(*this);
        compiler->setOutputFile(base);
    }

    SW_RETURN_MULTIPASS_END;
}

path FortranTarget::getOutputFileName(const path &root) const
{
    path p;
    if (isLocal())
    {
        p = getLocalOutputBinariesDirectory() / ::sw::getOutputFileName(*this);
    }
    else
    {
        p = root / getConfig() / ::sw::getOutputFileName(*this);
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
        p = getLocalOutputBinariesDirectory() / ::sw::getOutputFileName(*this);
    }
    else
    {
        p = root / getConfig() / ::sw::getOutputFileName(*this);
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
        auto base = BinaryDir.parent_path() / "out" / ::sw::getOutputFileName(*this);
        compiler->setOutputFile(base);
    }

    SW_RETURN_MULTIPASS_END;
}

path KotlinTarget::getOutputFileName(const path &root) const
{
    path p;
    if (isLocal())
    {
        p = getLocalOutputBinariesDirectory() / ::sw::getOutputFileName(*this);
    }
    else
    {
        p = root / getConfig() / ::sw::getOutputFileName(*this);
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

void DTarget::activateCompiler(const UnresolvedPackage &id, const StringSet &exts)
{
    auto &cld = getMainBuild().getTargets();

    TargetSettings oss; // empty for now
    auto i = cld.find(id, oss);
    if (!i)
    {
        for (auto &e : exts)
            setExtensionProgram(e, id);
        return;
    }
    auto t = i->as<PredefinedProgram *>();
    if (!t)
        throw SW_RUNTIME_ERROR("Target without PredefinedProgram: " + i->getPackage().toString());

    auto set_compiler_type = [this, &id, &exts](const auto &c)
    {
        for (auto &e : exts)
            setExtensionProgram(e, c->clone());
    };

    auto c = std::dynamic_pointer_cast<CompilerBaseProgram>(t->getProgram().clone());
    if (c)
    {
        set_compiler_type(c);
        return;
    }

    bool created = false;
    auto create_command = [this, &created, &t, &c]()
    {
        if (created)
            return;
        c->file = t->getProgram().file;
        auto C = c->createCommand(getMainBuild().getContext());
        static_cast<primitives::Command&>(*C) = *t->getProgram().getCommand();
        created = true;
    };

    compiler = std::make_shared<DCompiler>(getMainBuild().getContext());
    c = compiler;
    create_command();

    set_compiler_type(c);
}

NativeLinker *DTarget::getSelectedTool() const
{
    return compiler.get();
}

bool DTarget::init()
{
    switch (init_pass)
    {
    case 1:
    {
        Target::init();

        // propagate this pointer to all
        TargetOptionsGroup::iterate([this](auto &v, auto i)
        {
            v.target = this;
        });

        activateCompiler("org.dlang.dmd.dmd"s, { ".d" });
        if (!compiler)
            throw SW_RUNTIME_ERROR("Cannot find d compiler");

        compiler->setObjectDir(BinaryDir.parent_path() / "obj");
    }
    SW_RETURN_MULTIPASS_NEXT_PASS(init_pass);
    case 2:
    {
        setOutputFile();
    }
    SW_RETURN_MULTIPASS_END;
    }
    SW_RETURN_MULTIPASS_END;

    /* || add a considiton so user could change nont build output dir*/
    /*if (Scope == TargetScope::Build)
    {
        auto base = BinaryDir.parent_path() / "out" / ::sw::getOutputFileName(*this);
        compiler->setOutputFile(base);
        //compiler->setOutputFile(getOutputFileName(getSolution().getContext().getLocalStorage().storage_dir_bin));
    }
    else
    {
        auto base = BinaryDir.parent_path() / "out" / ::sw::getOutputFileName(*this);
        compiler->setOutputFile(base);
    }*/
    //compiler->setObjectDir(BinaryDir.parent_path() / "obj");

    SW_RETURN_MULTIPASS_END;
}

Commands DTarget::getCommands1() const
{
    for (auto f : gatherSourceFiles<SourceFile>(*this, {".d"}))
        compiler->setSourceFile(f->file/*, BinaryDir.parent_path() / "obj" / f->file.filename()*/);

    // add prepare() to propagate deps
    // here we check only our deps
    for (auto &d : this->gatherDependencies())
        compiler->setSourceFile(d->getTarget().as<DTarget &>().compiler->getOutputFile());

    Commands cmds;
    auto c = compiler->getCommand(*this);
    cmds.insert(c);
    return cmds;
}

bool DStaticLibrary::init()
{
    auto r = DTarget::init();
    compiler->Extension = getBuildSettings().TargetOS.getStaticLibraryExtension();
    compiler->BuildLibrary = true;
    return r;
}

bool DSharedLibrary::init()
{
    auto r = DTarget::init();
    compiler->Extension = getBuildSettings().TargetOS.getSharedLibraryExtension();
    compiler->BuildDll = true;
    return r;
}

bool DExecutable::init()
{
    auto r = DTarget::init();
    compiler->Extension = getBuildSettings().TargetOS.getExecutableExtension();
    return r;
}

}
