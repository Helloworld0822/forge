#include "mod_registry.h"
#include <stdio.h>

static const HyloStdFn IO_FNS[] = {
    {"print", "hy_print"},
    {"eprint", "hy_eprint"},
    {"eprintln", "hy_eprintln"},
};

static const HyloStdFn STRING_FNS[] = {
    {"str_len", "hy_str_len"},
    {"str_concat", "hy_str_concat"},
    {"str_eq", "hy_str_eq"},
    {"str_sub", "hy_str_sub"},
    {"str_contains", "hy_str_contains"},
    {"str_trim", "hy_str_trim"},
};

static const HyloStdFn MATH_FNS[] = {
    {"abs_i", "hy_abs_i"},
    {"abs_f", "hy_abs_f"},
    {"min_i", "hy_min_i"},
    {"max_i", "hy_max_i"},
    {"clamp_i", "hy_clamp_i"},
    {"pow_i", "hy_pow_i"},
};

static const HyloStdFn TIME_FNS[] = {
    {"time_now_ms", "hy_time_now_ms"},
    {"sleep_ms", "hy_sleep_ms"},
};

static const HyloStdFn FS_FNS[] = {
    {"fs_read", "hy_fs_read"},
    {"fs_write", "hy_fs_write"},
    {"fs_exists", "hy_fs_exists"},
    {"fs_remove", "hy_fs_remove"},
};

static const HyloStdFn OS_FNS[] = {
    {"os_exit", "hy_os_exit"},
    {"os_getenv", "hy_os_getenv"},
    {"os_argc", "hy_os_argc"},
    {"os_argv", "hy_os_argv"},
};

static const HyloStdFn TCP_FNS[] = {
    {"tcp_listen", "hy_tcp_listen"},
    {"tcp_accept", "hy_tcp_accept"},
    {"tcp_connect", "hy_tcp_connect"},
    {"tcp_send", "hy_tcp_send"},
    {"tcp_recv", "hy_tcp_recv"},
    {"tcp_close", "hy_tcp_close"},
    {"net_init", "hy_net_init"},
};

static const HyloStdFn UDP_FNS[] = {
    {"udp_bind", "hy_udp_bind"},
    {"udp_send", "hy_udp_send"},
    {"udp_recv", "hy_udp_recv"},
    {"udp_peer", "hy_udp_peer"},
    {"udp_close", "hy_udp_close"},
};

static const HyloStdFn HTTP_FNS[] = {
    {"http_get", "hy_http_get"},
    {"http_post", "hy_http_post"},
    {"http_listen", "hy_http_listen"},
    {"http_accept", "hy_http_accept"},
    {"http_req_method", "hy_http_req_method"},
    {"http_req_path", "hy_http_req_path"},
    {"http_req_body", "hy_http_req_body"},
    {"http_respond", "hy_http_respond"},
    {"http_close", "hy_http_close"},
    {"http_server_close", "hy_http_server_close"},
};

static const HyloStdFn JSON_FNS[] = {
    {"json_get_string", "hy_json_get_string"},
    {"json_get_int", "hy_json_get_int"},
    {"json_stringify_str", "hy_json_stringify_str"},
    {"json_stringify_int", "hy_json_stringify_int"},
};

static const HyloModule MODULES[] = {
    { .name = { "io", 2 }, .header = "hylo/io.h", .fns = IO_FNS, .fn_count = 3 },
    { .name = { "strings", 7 }, .header = "hylo/string.h", .fns = STRING_FNS, .fn_count = 6 },
    { .name = { "math", 4 }, .header = "hylo/math.h", .fns = MATH_FNS, .fn_count = 6 },
    { .name = { "time", 4 }, .header = "hylo/time.h", .fns = TIME_FNS, .fn_count = 2 },
    { .name = { "fs", 2 }, .header = "hylo/fs.h", .fns = FS_FNS, .fn_count = 4 },
    { .name = { "os", 2 }, .header = "hylo/os.h", .fns = OS_FNS, .fn_count = 4 },
    { .name = { "tcp", 3 }, .header = "hylo/tcp.h", .fns = TCP_FNS, .fn_count = 7 },
    { .name = { "udp", 3 }, .header = "hylo/udp.h", .fns = UDP_FNS, .fn_count = 5 },
    { .name = { "http", 4 }, .header = "hylo/http.h", .fns = HTTP_FNS, .fn_count = 10 },
    { .name = { "json", 4 }, .header = "hylo/json.h", .fns = JSON_FNS, .fn_count = 4 },
};

const HyloModule *hylo_std_module(HyloStr name) {
    for (size_t i = 0; i < sizeof(MODULES) / sizeof(MODULES[0]); i++) {
        if (hylo_str_eq(MODULES[i].name, name)) return &MODULES[i];
    }
    return NULL;
}

const char *hylo_std_c_name(HyloStr hy_name, HyloStr *imports, size_t import_count) {
    for (size_t i = 0; i < import_count; i++) {
        const HyloModule *mod = hylo_std_module(imports[i]);
        if (!mod) continue;
        for (size_t j = 0; j < mod->fn_count; j++) {
            HyloStr fn = hylo_str(mod->fns[j].hy_name);
            if (hylo_str_eq(fn, hy_name)) return mod->fns[j].c_name;
        }
    }
    return NULL;
}

const char *hylo_std_header(HyloStr module) {
    const HyloModule *mod = hylo_std_module(module);
    return mod ? mod->header : NULL;
}

int hylo_import_is_stdlib(HyloStr name) {
    return hylo_std_module(name) != NULL;
}

void hylo_lib_mangle(char *out, size_t cap, HyloStr lib, HyloStr fn) {
    snprintf(out, cap, "hylib_%.*s_%.*s", (int)lib.len, lib.data, (int)fn.len, fn.data);
}
