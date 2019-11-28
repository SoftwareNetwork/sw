#include "exceptions.h"

#include <iostream>

void print(const char *s)
{
    //printf("\n%s\n\n", s); // works
    std::cout << "\n" << s << "\n\n"; // has strong impact, does not work with lld
    //try { throw 1; } catch (...) {}
}

void f()
{
    try { throw std::runtime_error(""); }
    catch (std::exception&) { print("std::exception&"); }

    try { throw std::runtime_error(""); }
    catch (std::exception) { print("std::exception"); }






    try { throw 5; }
    catch (int) { print("int"); }

    try { throw sw::RuntimeError(""); }
    catch (sw::RuntimeError &) { print("sw::RuntimeError&"); }

    try { throw sw::RuntimeError(""); }
    catch (sw::RuntimeError) { print("sw::RuntimeError"); }

    try { throw sw::RuntimeError(""); }
    catch (std::exception&) { print("sw::RuntimeError std::exception&"); }

    try { throw std::runtime_error(""); }
    catch (std::exception) { print("std::exception"); }

    try { throw std::runtime_error(""); }
    catch (std::exception&) { print("std::exception&"); }
}

int main()
{
    f();
    return 0;
}
