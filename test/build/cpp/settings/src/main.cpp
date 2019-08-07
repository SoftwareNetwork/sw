#include "lib.h"

#include <stdio.h>

int main()
{
    int i = 0;
    i += f1();
#ifdef WANT_FEATURE2
    i += f2();
#endif
#ifdef WANT_FEATURE3
    i += f3();
#endif
    printf("%d\n", i);
}
