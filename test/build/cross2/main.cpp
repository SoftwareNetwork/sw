#include "exceptions.h"

#include <iostream>

void f()
{
    try { throw 5; }
    catch (int) { std::cout << "int\n"; }
    try { throw sw::RuntimeError(""); }
    catch (sw::RuntimeError) { std::cout << "sw::RuntimeError\n"; }
    try { throw sw::RuntimeError(""); }
    catch (std::exception&) { std::cout << "std::exception\n"; }
}

int main()
{
    f();
    return 0;
}
