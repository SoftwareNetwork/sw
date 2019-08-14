#include <iostream>

#include <primitives/exceptions.h>
#include <primitives/sw/main.h>

int main(int argc, char **argv)
{
    try { throw SW_RUNTIME_ERROR("1"); }
    catch (sw::RuntimeError) { std::cout << "RuntimeError 1\n"; }
    try { throw SW_RUNTIME_ERROR("2"); }
    catch (std::runtime_error) { std::cout << "RuntimeError 2\n"; }
    try { throw SW_RUNTIME_ERROR("3"); }
    catch (std::exception) { std::cout << "RuntimeError 3\n"; }
    try { throw SW_RUNTIME_ERROR("4"); }
    catch (...) { std::cout << "RuntimeError 4\n"; }

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
