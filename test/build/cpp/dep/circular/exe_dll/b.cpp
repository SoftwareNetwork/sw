#include <iostream>

MY_B_API int b(int a)
{
    return a * 2;
}
MY_A_API int a(int);

int main()
{
    auto r = b(5);
    r = a(r);
    std::cout << r;
    return 0;
}
