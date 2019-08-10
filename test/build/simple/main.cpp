#include <exception>
#include <iostream>
#include <stdexcept>
#include <thread>

API
void f();

int main()
{
    std::cout << "Hello, World!\n";

    f();
    std::thread t([] {f(); });
    t.join();

    return 0;
}
