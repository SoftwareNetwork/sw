#include "other.h"

#include <directories.h>

namespace sw
{

bool CSharpTarget::init()
{
    Target::init();

    // propagate this pointer to all
    TargetOptionsGroup::iterate<WithSourceFileStorage, WithoutNativeOptions>([this](auto &v, auto &gs)
    {
        v.target = this;
    });
    //LanguageStorage::target = this;

    if (auto p = SourceFileStorage::findProgramByExtension(".cs"); p)
        compiler = std::dynamic_pointer_cast<CSharpCompiler>(p->clone());
    else
        throw SW_RUNTIME_ERROR("No C# compiler found");

    /* || add a considiton so user could change nont build output dir*/
    if (Scope == TargetScope::Build)
    {
        compiler->setOutputFile(getOutputFileName(getUserDirectories().storage_dir_bin));
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
    if (SW_IS_LOCAL_BINARY_DIR)
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
    for (auto f : gatherSourceFiles<CSharpSourceFile>(*this))
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
    TargetOptionsGroup::iterate<WithSourceFileStorage, WithoutNativeOptions>([this](auto &v, auto &gs)
    {
        v.target = this;
    });
    //LanguageStorage::target = this;

    if (auto p = SourceFileStorage::findProgramByExtension(".rs"); p)
        compiler = std::dynamic_pointer_cast<RustCompiler>(p->clone());
    else
        throw SW_RUNTIME_ERROR("No Rust compiler found");

    /* || add a considiton so user could change nont build output dir*/
    if (Scope == TargetScope::Build)
    {
        compiler->setOutputFile(getOutputFileName(getUserDirectories().storage_dir_bin));
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
    if (SW_IS_LOCAL_BINARY_DIR)
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
    for (auto f : gatherSourceFiles<RustSourceFile>(*this))
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
    TargetOptionsGroup::iterate<WithSourceFileStorage, WithoutNativeOptions>([this](auto &v, auto &gs)
    {
        v.target = this;
    });
    //LanguageStorage::target = this;

    if (auto p = SourceFileStorage::findProgramByExtension(".go"); p)
        compiler = std::dynamic_pointer_cast<GoCompiler>(p->clone());
    else
        throw SW_RUNTIME_ERROR("No Go compiler found");

    /* || add a considiton so user could change nont build output dir*/
    if (Scope == TargetScope::Build)
    {
        compiler->setOutputFile(getOutputFileName(getUserDirectories().storage_dir_bin));
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
    if (SW_IS_LOCAL_BINARY_DIR)
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
    for (auto f : gatherSourceFiles<GoSourceFile>(*this))
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
    TargetOptionsGroup::iterate<WithSourceFileStorage, WithoutNativeOptions>([this](auto &v, auto &gs)
    {
        v.target = this;
    });
    //LanguageStorage::target = this;

    if (auto p = SourceFileStorage::findProgramByExtension(".f"); p)
        compiler = std::dynamic_pointer_cast<FortranCompiler>(p->clone());
    else
        throw SW_RUNTIME_ERROR("No Fortran compiler found");

    /* || add a considiton so user could change nont build output dir*/
    if (Scope == TargetScope::Build)
    {
        compiler->setOutputFile(getOutputFileName(getUserDirectories().storage_dir_bin));
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
    if (SW_IS_LOCAL_BINARY_DIR)
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
    for (auto f : gatherSourceFiles<FortranSourceFile>(*this))
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
    TargetOptionsGroup::iterate<WithSourceFileStorage, WithoutNativeOptions>([this](auto &v, auto &gs)
    {
        v.target = this;
    });
    //LanguageStorage::target = this;

    if (auto p = SourceFileStorage::findProgramByExtension(".java"); p)
        compiler = std::dynamic_pointer_cast<JavaCompiler>(p->clone());
    else
        throw SW_RUNTIME_ERROR("No Java compiler found");

    /* || add a considiton so user could change nont build output dir*/
    /*if (Scope == TargetScope::Build)
    {
        //compiler->setOutputFile(getOutputFileName(getUserDirectories().storage_dir_bin));
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
    if (SW_IS_LOCAL_BINARY_DIR)
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
    Commands cmds;
    for (auto f : gatherSourceFiles<JavaSourceFile>(*this))
    {
        compiler->setSourceFile(f->file);
        cmds.insert(compiler->prepareCommand(*this));
    }

    //auto c = compiler->getCommand(*this);
    //cmds.insert(c);
    return cmds;
}

bool KotlinTarget::init()
{
    Target::init();

    // propagate this pointer to all
    TargetOptionsGroup::iterate<WithSourceFileStorage, WithoutNativeOptions>([this](auto &v, auto &gs)
    {
        v.target = this;
    });
    //LanguageStorage::target = this;

    if (auto p = SourceFileStorage::findProgramByExtension(".kt"); p)
        compiler = std::dynamic_pointer_cast<KotlinCompiler>(p->clone());
    else
        throw SW_RUNTIME_ERROR("No Kotlin compiler found");

    /* || add a considiton so user could change nont build output dir*/
    if (Scope == TargetScope::Build)
    {
        compiler->setOutputFile(getOutputFileName(getUserDirectories().storage_dir_bin));
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
    if (SW_IS_LOCAL_BINARY_DIR)
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
    for (auto f : gatherSourceFiles<KotlinSourceFile>(*this))
        compiler->setSourceFile(f->file);

    Commands cmds;
    auto c = compiler->getCommand(*this);
    cmds.insert(c);
    return cmds;
}

bool DTarget::init()
{
    Target::init();

    // propagate this pointer to all
    TargetOptionsGroup::iterate<WithSourceFileStorage, WithoutNativeOptions>([this](auto &v, auto &gs)
    {
        v.target = this;
    });
    //LanguageStorage::target = this;

    if (auto p = SourceFileStorage::findProgramByExtension(".d"); p)
        compiler = std::dynamic_pointer_cast<DCompiler>(p->clone());
    else
        throw SW_RUNTIME_ERROR("No D compiler found");

    /* || add a considiton so user could change nont build output dir*/
    if (Scope == TargetScope::Build)
    {
        compiler->setOutputFile(getOutputFileName(getUserDirectories().storage_dir_bin));
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
    if (SW_IS_LOCAL_BINARY_DIR)
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
    for (auto f : gatherSourceFiles<DSourceFile>(*this))
        compiler->setSourceFile(f->file);

    Commands cmds;
    auto c = compiler->getCommand(*this);
    cmds.insert(c);
    return cmds;
}

}
