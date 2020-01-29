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

template <class CompilerType>
static std::shared_ptr<CompilerType> activateCompiler(Target &t, const UnresolvedPackage &id, const StringSet &exts)
{
    auto &cld = t.getMainBuild().getTargets();

    TargetSettings oss; // empty for now
    auto i = cld.find(id, oss);
    if (!i)
    {
        for (auto &e : exts)
            t.setExtensionProgram(e, id);
        return {};
    }
    auto prog = i->as<PredefinedProgram *>();
    if (!prog)
        throw SW_RUNTIME_ERROR("Target without PredefinedProgram: " + i->getPackage().toString());

    auto set_compiler_type = [&t, &id, &exts](const auto &c)
    {
        for (auto &e : exts)
            t.setExtensionProgram(e, c->clone());
    };

    auto c = std::dynamic_pointer_cast<CompilerBaseProgram>(prog->getProgram().clone());
    if (c)
    {
        set_compiler_type(c);
        return {};
    }

    bool created = false;
    auto create_command = [&prog, &created, &t, &c]()
    {
        if (created)
            return;
        c->file = prog->getProgram().file;
        auto C = c->createCommand(t.getMainBuild().getContext());
        static_cast<primitives::Command&>(*C) = *prog->getProgram().getCommand();
        created = true;
    };

    auto compiler = std::make_shared<CompilerType>(t.getMainBuild().getContext());
    c = compiler;
    create_command();

    set_compiler_type(c);

    return compiler;
}

bool CSharpTarget::init()
{
    Target::init();

    // propagate this pointer to all
    TargetOptionsGroup::iterate([this](auto &v, auto i)
    {
        v.target = this;
    });

    compiler = activateCompiler<VisualStudioCSharpCompiler>(*this, "com.Microsoft.VisualStudio.Roslyn.csc"s, { ".cs" });
    if (!compiler)
        throw SW_RUNTIME_ERROR("No C# compiler found");

    compiler->Extension = getBuildSettings().TargetOS.getExecutableExtension();
    compiler->setOutputFile(getBaseOutputFileName(*this, {}, "bin"));

    SW_RETURN_MULTIPASS_END;
}

Commands CSharpTarget::getCommands1() const
{
    for (auto f : gatherSourceFiles<SourceFile>(*this, { ".cs" }))
        compiler->addSourceFile(f->file);

    Commands cmds;
    auto c = compiler->getCommand(*this);
    cmds.insert(c);
    return cmds;
}

bool RustTarget::init()
{
    Target::init();

    // propagate this pointer to all
    TargetOptionsGroup::iterate([this](auto &v, auto i)
    {
        v.target = this;
    });

    compiler = activateCompiler<decltype(compiler)::element_type>(*this, "org.rust.rustc"s, { ".rs" });
    if (!compiler)
        throw SW_RUNTIME_ERROR("No Rust compiler found");

    compiler->Extension = getBuildSettings().TargetOS.getExecutableExtension();
    compiler->setOutputFile(getBaseOutputFileName(*this, {}, "bin"));

    SW_RETURN_MULTIPASS_END;
}

Commands RustTarget::getCommands1() const
{
    for (auto f : gatherSourceFiles<SourceFile>(*this, {".rs"}))
        compiler->setSourceFile(f->file);

    Commands cmds;
    auto c = compiler->getCommand(*this);
    cmds.insert(c);
    return cmds;
}

bool GoTarget::init()
{
    Target::init();

    // propagate this pointer to all
    TargetOptionsGroup::iterate([this](auto &v, auto i)
    {
        v.target = this;
    });

    compiler = activateCompiler<decltype(compiler)::element_type>(*this, "org.google.golang.go"s, { ".go" });
    if (!compiler)
        throw SW_RUNTIME_ERROR("No Go compiler found");

    compiler->Extension = getBuildSettings().TargetOS.getExecutableExtension();
    compiler->setOutputFile(getBaseOutputFileName(*this, {}, "bin"));

    SW_RETURN_MULTIPASS_END;
}

Commands GoTarget::getCommands1() const
{
    for (auto f : gatherSourceFiles<SourceFile>(*this, {".go"}))
        compiler->setSourceFile(f->file);

    Commands cmds;
    auto c = compiler->getCommand(*this);
    cmds.insert(c);
    return cmds;
}

bool FortranTarget::init()
{
    Target::init();

    // propagate this pointer to all
    TargetOptionsGroup::iterate([this](auto &v, auto i)
    {
        v.target = this;
    });

    compiler = activateCompiler<decltype(compiler)::element_type>(*this, "org.gnu.gcc.fortran"s, { ".f" });
    if (!compiler)
        throw SW_RUNTIME_ERROR("No Fortran compiler found");

    compiler->Extension = getBuildSettings().TargetOS.getExecutableExtension();
    compiler->setOutputFile(getBaseOutputFileName(*this, {}, "bin"));

    SW_RETURN_MULTIPASS_END;
}

Commands FortranTarget::getCommands1() const
{
    for (auto f : gatherSourceFiles<SourceFile>(*this, {".f"}))
        compiler->setSourceFile(f->file);

    Commands cmds;
    auto c = compiler->getCommand(*this);
    cmds.insert(c);
    return cmds;
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

NativeLinker *DTarget::getSelectedTool() const
{
    return compiler.get();
}

bool DTarget::init()
{
    // https://dlang.org/dmd-windows.html
    // https://wiki.dlang.org/Win32_DLLs_in_D
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

        compiler = activateCompiler<decltype(compiler)::element_type>(*this, "org.dlang.dmd.dmd"s, { ".d" });
        if (!compiler)
            throw SW_RUNTIME_ERROR("No D compiler found");

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
