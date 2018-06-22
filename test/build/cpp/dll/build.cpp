#include <iostream>

__declspec(dllexport)
void f()
{
    std::cout << "Hello, World!\n";
}
