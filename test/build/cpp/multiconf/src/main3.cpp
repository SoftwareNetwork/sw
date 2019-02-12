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
    return 0;
}
    )");
}
