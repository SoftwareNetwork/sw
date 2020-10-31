#include <iostream>

int main()
{
    printf(R"(
#include <iostream>

int main()
{
    printf("Hello, world!\n");
    std::cout << 123;
    std::cout << "123";
    return 0;
}
    )");
}
