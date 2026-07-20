#include "hylo/os.h"
#include <stdlib.h>

static int g_argc = 0;
static char **g_argv = NULL;

void hy_os_set_args(int argc, char **argv) {
    g_argc = argc;
    g_argv = argv;
}

void hy_os_exit(int64_t code) {
    exit((int)code);
}

const char *hy_os_getenv(const char *name) {
    if (!name) return "";
    const char *v = getenv(name);
    return v ? v : "";
}

int64_t hy_os_argc(void) {
    return g_argc;
}

const char *hy_os_argv(int64_t index) {
    if (index < 0 || index >= g_argc || !g_argv) return "";
    return g_argv[index] ? g_argv[index] : "";
}
