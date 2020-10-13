#include <iostream>

MY_A_API int a(int b)
{
    return b + 2;
}
MY_B_API int b(int);

int main()
{
    auto r = a(5);
    r = b(r);
    std::cout << r;
    return 0;
}
