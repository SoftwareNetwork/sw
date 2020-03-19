#include <exception>
#include <iostream>
#include <stdexcept>

API
void f()
{
    try { throw std::runtime_error("x"); }
    catch(std::runtime_error &){ std::cout << "Hello, World!\n"; }
    catch(std::exception &){}
}
