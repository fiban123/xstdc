#include "xstdc/alloc.h"
#include "xstdc/print.h"
#include <string.h>

int main(int argc, char **argv) {
    if (argc < 2 || (argc == 2 && !strcmp(argv[1], "--help"))) {
        xprint("Executes some allocations, and should show some error in every "
               "case except the correct one.\n"
               "Commands:\n"
               " * double_free\n"
               " * leak\n"
               " * different_size\n"
               " * different_alignment\n"
               " * yottabyte_alloc\n"
               " * correct\n");
        return 0;
    }
    const char *command = argv[1];
    if (argc != 2 ||
        (strcmp(command, "double_free") && strcmp(command, "leak") &&
         strcmp(command, "different_size") &&
         strcmp(command, "different_alignment") &&
         strcmp(command, "yottabyte_alloc") && strcmp(command, "correct"))) {
        xprint_err("Unknown command or extra arguments. Use %z --help.\n",
                   argv[0]);
        return 1;
    }
    if (!strcmp(command, "yottabyte_alloc")) {
        // A yottabyte cannot fit in usize. Request its largest representable
        // size instead, rather than silently wrapping a yottabyte constant.
        xprint("A yottabyte exceeds usize; requesting the maximum size "
               "instead.\n");
        void *p = xalloc((usize)-1);
        xfree(p, (usize)-1);
        return 1; // The checked allocation must terminate first.
    }
    if (strcmp(command, "correct") && !xalloc_has_debug()) {
        xprint_err("Allocation tracking is disabled. Rebuild without fast=1 "
                   "or define XSTDC_DEBUG_ALLOC when compiling alloc.c.\n");
        return 1;
    }
    if (!strcmp(command, "double_free")) {
        void *p = xalloc(32);
        xfree(p, 32);
        xfree(p, 32);
    } else if (!strcmp(command, "leak")) {
        (void)xalloc(32);
        xalloc_debug_unfreed();
    } else if (!strcmp(command, "different_size")) {
        void *p = xalloc(32);
        xfree(p, 31);
    } else if (!strcmp(command, "different_alignment")) {
        void *p = xalloca(32, 64);
        xfreea(p, 32, 32);
    } else {
        void *p = xalloc(32);
        p = xrealloc(p, 32, 64);
        xfree(p, 64);
        p = xallocza(32, 64);
        p = xrealloca(p, 32, 128, 64);
        xfreea(p, 128, 64);
        xalloc_debug_unfreed();
        xprint("Correct allocation sequence completed.\n");
    }
    return 0;
}
