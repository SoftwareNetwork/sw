#include <iostream>

void f()
{
    printf("f()\n");
    std::cout << "f()\n";
    printf("f()\n");

    try { throw 5; }
    catch (int) { printf("int\n"); }

    try { throw std::runtime_error("x"); }
    catch (std::runtime_error &) { printf("Hello, World!\n"); }
    catch (std::exception &) {}
}
