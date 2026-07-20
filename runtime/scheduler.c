#include "hylo_runtime.h"
#include <stdlib.h>
#include <string.h>

static char *hy_strdup(const char *s) {
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

#define MAILBOX_CAP 256
#define MAX_COROS 1024

typedef struct hy_mailbox {
    hy_msg_t msgs[MAILBOX_CAP];
    size_t head;
    size_t tail;
    size_t count;
} hy_mailbox_t;

struct hy_coro {
    int id;
    hy_coro_fn fn;
    void *state;
    size_t state_size;
    hy_coro_status_t status;
    int step;
    hy_process_t *proc;
    struct hy_coro *next;
};

struct hy_process {
    char *name;
    hy_scheduler_t *sched;
    hy_mailbox_t mailbox;
    hy_coro_t *coros;
    hy_coro_t *coro_tail;
    int next_coro_id;
    hy_coro_fn receive_handler;
    size_t receive_state_size;
    int is_supervisor;
    hy_restart_policy_t restart_policy;
    hy_process_t **children;
    size_t child_count;
    struct hy_process *next;
};

struct hy_scheduler {
    hy_process_t *processes;
    int worker_count;
    int running;
};

hy_scheduler_t *hy_scheduler_create(int worker_count) {
    hy_scheduler_t *s = (hy_scheduler_t *)calloc(1, sizeof(hy_scheduler_t));
    if (!s) return NULL;
    s->worker_count = worker_count > 0 ? worker_count : 1;
    return s;
}

void hy_scheduler_destroy(hy_scheduler_t *sched) {
    if (!sched) return;
    hy_process_t *p = sched->processes;
    while (p) {
        hy_process_t *next = p->next;
        hy_process_destroy(p);
        p = next;
    }
    free(sched);
}

void hy_scheduler_add_process(hy_scheduler_t *sched, hy_process_t *proc) {
    proc->sched = sched;
    proc->next = sched->processes;
    sched->processes = proc;
}

static int mailbox_push(hy_mailbox_t *mb, hy_msg_t msg) {
    if (mb->count >= MAILBOX_CAP) return 0;
    mb->msgs[mb->tail] = msg;
    mb->tail = (mb->tail + 1) % MAILBOX_CAP;
    mb->count++;
    return 1;
}

static int mailbox_pop(hy_mailbox_t *mb, hy_msg_t *out) {
    if (mb->count == 0) return 0;
    *out = mb->msgs[mb->head];
    mb->head = (mb->head + 1) % MAILBOX_CAP;
    mb->count--;
    return 1;
}

hy_process_t *hy_process_create(const char *name) {
    hy_process_t *p = (hy_process_t *)calloc(1, sizeof(hy_process_t));
    if (!p) return NULL;
    p->name = hy_strdup(name ? name : "process");
    p->next_coro_id = 1;
    return p;
}

void hy_process_destroy(hy_process_t *proc) {
    if (!proc) return;
    hy_coro_t *c = proc->coros;
    while (c) {
        hy_coro_t *next = c->next;
        free(c->state);
        free(c);
        c = next;
    }
    free(proc->children);
    free(proc->name);
    free(proc);
}

hy_scheduler_t *hy_process_scheduler(hy_process_t *proc) {
    return proc->sched;
}

const char *hy_process_name(hy_process_t *proc) {
    return proc ? proc->name : "";
}

void hy_process_set_receive_handler(hy_process_t *proc, hy_coro_fn handler, size_t state_size) {
    proc->receive_handler = handler;
    proc->receive_state_size = state_size;
}

hy_coro_t *hy_coro_spawn(hy_process_t *proc, hy_coro_fn fn, void *init_state, size_t state_size) {
    hy_coro_t *c = (hy_coro_t *)calloc(1, sizeof(hy_coro_t));
    if (!c) return NULL;
    c->id = proc->next_coro_id++;
    c->fn = fn;
    c->state_size = state_size;
    c->state = malloc(state_size);
    if (!c->state) {
        free(c);
        return NULL;
    }
    memcpy(c->state, init_state, state_size);
    c->status = HY_CORO_RUNNING;
    c->step = 0;
    c->proc = proc;
    if (!proc->coros) proc->coros = proc->coro_tail = c;
    else { proc->coro_tail->next = c; proc->coro_tail = c; }
    free(init_state);
    return c;
}

hy_coro_status_t hy_yield(hy_coro_t *coro) {
    coro->status = HY_CORO_YIELDED;
    return HY_CORO_YIELDED;
}

int hy_coro_id(hy_coro_t *coro) {
    return coro ? coro->id : -1;
}

hy_process_t *hy_coro_process(hy_coro_t *coro) {
    return coro ? coro->proc : NULL;
}

int hy_coro_step(hy_coro_t *coro) {
    return coro ? coro->step : 0;
}

void hy_coro_set_step(hy_coro_t *coro, int step) {
    if (coro) coro->step = step;
}

void hy_send(hy_process_t *dst, int tag, int64_t value, void *payload, size_t payload_size) {
    if (!dst) return;
    hy_msg_t msg = { tag, value, payload, payload_size, NULL };
    mailbox_push(&dst->mailbox, msg);
}

int hy_try_recv(hy_process_t *self, hy_msg_t *out) {
    if (!self || !out) return 0;
    return mailbox_pop(&self->mailbox, out);
}

int hy_recv(hy_process_t *self, hy_msg_t *out) {
    while (!hy_try_recv(self, out)) {
        /* cooperative wait */
    }
    return 1;
}

hy_process_t *hy_supervisor_create(const char *name, hy_restart_policy_t policy) {
    hy_process_t *p = hy_process_create(name);
    p->is_supervisor = 1;
    p->restart_policy = policy;
    return p;
}

void hy_supervisor_add_child(hy_process_t *supervisor, hy_process_t *child) {
    supervisor->child_count++;
    supervisor->children = (hy_process_t **)realloc(
        supervisor->children, supervisor->child_count * sizeof(hy_process_t *));
    supervisor->children[supervisor->child_count - 1] = child;
}

static int run_coroutine_round(hy_process_t *proc) {
    int progressed = 0;
    for (hy_coro_t *c = proc->coros; c; c = c->next) {
        if (c->status == HY_CORO_DONE || c->status == HY_CORO_ERROR) continue;
        hy_coro_status_t st = c->fn(c, c->state);
        c->status = st;
        if (st == HY_CORO_YIELDED) {
            c->status = HY_CORO_RUNNING;
            progressed = 1;
        } else if (st == HY_CORO_DONE) {
            progressed = 1;
        }
    }
    return progressed;
}

void hy_scheduler_run(hy_scheduler_t *sched) {
    if (!sched) return;
    sched->running = 1;
    int idle_rounds = 0;
    while (sched->running) {
        int any_active = 0;
        int any_progress = 0;
        for (hy_process_t *p = sched->processes; p; p = p->next) {
            for (hy_coro_t *c = p->coros; c; c = c->next) {
                if (c->status != HY_CORO_DONE && c->status != HY_CORO_ERROR) {
                    any_active = 1;
                    break;
                }
            }
            if (run_coroutine_round(p)) any_progress = 1;
        }
        if (!any_active) break;
        if (!any_progress) {
            idle_rounds++;
            if (idle_rounds > 10000) break;
        } else {
            idle_rounds = 0;
        }
    }
}
