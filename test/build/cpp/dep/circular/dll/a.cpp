#include <iostream>

MY_A_API void a()
{
    std::cout << "Hello, A World!\n";
}
MY_B_API void b();

int main()
{
    a();
    b();
    return 0;
}
