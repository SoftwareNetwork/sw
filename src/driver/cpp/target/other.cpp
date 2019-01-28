#include "other.h"

#include <directories.h>

namespace sw
{

void CSharpTarget::init()
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

Commands CSharpTarget::getCommands() const
{
    for (auto f : gatherSourceFiles<CSharpSourceFile>(*this))
        compiler->addSourceFile(f->file);

    Commands cmds;
    auto c = compiler->getCommand(*this);
    cmds.insert(c);
    return cmds;
}

bool CSharpTarget::prepare()
{
    return false;
}

void RustTarget::init()
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

Commands RustTarget::getCommands() const
{
    for (auto f : gatherSourceFiles<RustSourceFile>(*this))
        compiler->setSourceFile(f->file);

    Commands cmds;
    auto c = compiler->getCommand(*this);
    cmds.insert(c);
    return cmds;
}

bool RustTarget::prepare()
{
    return false;
}

void GoTarget::init()
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

Commands GoTarget::getCommands() const
{
    for (auto f : gatherSourceFiles<GoSourceFile>(*this))
        compiler->setSourceFile(f->file);

    Commands cmds;
    auto c = compiler->getCommand(*this);
    cmds.insert(c);
    return cmds;
}

bool GoTarget::prepare()
{
    return false;
}

void FortranTarget::init()
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

Commands FortranTarget::getCommands() const
{
    for (auto f : gatherSourceFiles<FortranSourceFile>(*this))
        compiler->setSourceFile(f->file);

    Commands cmds;
    auto c = compiler->getCommand(*this);
    cmds.insert(c);
    return cmds;
}

bool FortranTarget::prepare()
{
    return false;
}

void JavaTarget::init()
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

Commands JavaTarget::getCommands() const
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

bool JavaTarget::prepare()
{
    return false;
}

void KotlinTarget::init()
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

Commands KotlinTarget::getCommands() const
{
    for (auto f : gatherSourceFiles<KotlinSourceFile>(*this))
        compiler->setSourceFile(f->file);

    Commands cmds;
    auto c = compiler->getCommand(*this);
    cmds.insert(c);
    return cmds;
}

bool KotlinTarget::prepare()
{
    return false;
}

void DTarget::init()
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

Commands DTarget::getCommands() const
{
    for (auto f : gatherSourceFiles<DSourceFile>(*this))
        compiler->setSourceFile(f->file);

    Commands cmds;
    auto c = compiler->getCommand(*this);
    cmds.insert(c);
    return cmds;
}

bool DTarget::prepare()
{
    return false;
}

}
