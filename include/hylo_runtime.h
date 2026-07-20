#ifndef HYLO_RUNTIME_H
#define HYLO_RUNTIME_H

#include "hylo/io.h"
#include <stddef.h>
#include <stdint.h>

typedef struct hy_msg hy_msg_t;
typedef struct hy_process hy_process_t;
typedef struct hy_coro hy_coro_t;
typedef struct hy_scheduler hy_scheduler_t;

typedef enum {
    HY_CORO_RUNNING = 0,
    HY_CORO_YIELDED,
    HY_CORO_DONE,
    HY_CORO_ERROR
} hy_coro_status_t;

typedef enum {
    HY_RESTART_CORO = 0,
    HY_RESTART_PROCESS,
    HY_RESTART_ALL
} hy_restart_policy_t;

typedef hy_coro_status_t (*hy_coro_fn)(hy_coro_t *coro, void *state);

struct hy_msg {
    int tag;
    int64_t value;
    void *payload;
    size_t payload_size;
    hy_process_t *from;
};

hy_scheduler_t *hy_scheduler_create(int worker_count);
void hy_scheduler_destroy(hy_scheduler_t *sched);
void hy_scheduler_add_process(hy_scheduler_t *sched, hy_process_t *proc);
void hy_scheduler_run(hy_scheduler_t *sched);

hy_process_t *hy_process_create(const char *name);
void hy_process_destroy(hy_process_t *proc);
hy_scheduler_t *hy_process_scheduler(hy_process_t *proc);
const char *hy_process_name(hy_process_t *proc);
void hy_process_set_receive_handler(hy_process_t *proc, hy_coro_fn handler, size_t state_size);

hy_coro_t *hy_coro_spawn(hy_process_t *proc, hy_coro_fn fn, void *init_state, size_t state_size);
hy_coro_status_t hy_yield(hy_coro_t *coro);
int hy_coro_id(hy_coro_t *coro);
hy_process_t *hy_coro_process(hy_coro_t *coro);
int hy_coro_step(hy_coro_t *coro);
void hy_coro_set_step(hy_coro_t *coro, int step);

void hy_send(hy_process_t *dst, int tag, int64_t value, void *payload, size_t payload_size);
int hy_try_recv(hy_process_t *self, hy_msg_t *out);
int hy_recv(hy_process_t *self, hy_msg_t *out);

hy_process_t *hy_supervisor_create(const char *name, hy_restart_policy_t policy);
void hy_supervisor_add_child(hy_process_t *supervisor, hy_process_t *child);

#endif
