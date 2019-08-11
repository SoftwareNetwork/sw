#include <iostream>

#include <primitives/sw/main.h>

int main(int argc, char **argv)
{
    try { throw 5; }
    catch (int) { std::cout << "int\n"; }
    try { throw 5.0; }
    catch (double) { std::cout << "double\n"; }
    struct X {};
    try { throw X{}; }
    catch (X) { std::cout << "struct X\n"; }

    try { throw std::runtime_error("x"); }
    catch (std::runtime_error &) { std::cout << "Hello, World!\n"; }
    catch (std::exception &) {}

    return 0;
}
