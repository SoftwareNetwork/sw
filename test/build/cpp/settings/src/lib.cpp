#include "lib.h"

int f1() { return 1; }

#ifdef FEATURE2
int f2() { return 1; }
#endif

#ifdef FEATURE3
int f3() { return 1; }
#endif
