#include "xstdc/process.h"
#include <stdlib.h>

_Noreturn void xexit(i32 ret) { exit(ret); }

_Noreturn void xexit_hard(i32 ret) { _Exit(ret); }
