#pragma once

#include "xstdc/types.h"

// Terminate with ret, running exit handlers and flushing libc streams.
_Noreturn void xexit(i32 ret);
// Terminate immediately with ret, without exit handlers or stream flushing.
_Noreturn void xexit_hard(i32 ret);
