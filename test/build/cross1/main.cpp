#include <iostream>

void f();

int main(int argc, char **argv)
{
    std::cout << "main()\n";

    try { throw 5; }
    catch (int) { std::cout << "int\n"; }

    try { throw std::runtime_error("x"); }
    catch (std::runtime_error &) { printf("Hello, World!\n"); }
    catch (std::exception &) {}

    f();

    return 0;
}
