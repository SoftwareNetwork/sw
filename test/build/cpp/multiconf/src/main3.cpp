#include "lib6.h"

#include <iostream>

int main()
{
    f();

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
