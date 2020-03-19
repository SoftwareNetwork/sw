#include <iostream>

#include <dlfcn.h>

API extern "C" int f()
{
    std::cout << "wow f\n";
    return 1;
}

__attribute__ ((visibility ("default")))
int g()
{
    std::cout << "wow g\n";
    return 2;
}

int main()
{
    auto h = dlopen(0, RTLD_LAZY | RTLD_GLOBAL);
    if (!h)
    {
        std::cout << dlerror() << "\n";
        return 1;
    }

    auto f = (int(*)())dlsym(h, "f");
    if (!f)
    {
        std::cout << dlerror() << "\n";
        //return 2;
    }

    if (f)
        std::cout << f() << "\n";

    f = (int(*)())dlsym(h, "_Z1gv");
    if (!f)
    {
        std::cout << dlerror() << "\n";
        //return 3;
    }

    if (f)
        std::cout << f() << "\n";

    dlclose(h);
    return 0;
}
