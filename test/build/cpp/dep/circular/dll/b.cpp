#include <iostream>

MY_B_API void b()
{
    std::cout << "Hello, B World!\n";
}
MY_A_API void a();

int main()
{
    a();
    b();
    return 0;
}
