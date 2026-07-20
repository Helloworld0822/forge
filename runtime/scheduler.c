#include "forge_runtime.h"
#include <stdlib.h>
#include <string.h>

static char *fr_strdup(const char *s) {
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

#define MAILBOX_CAP 256
#define MAX_COROS 1024

typedef struct fr_mailbox {
    fr_msg_t msgs[MAILBOX_CAP];
    size_t head;
    size_t tail;
    size_t count;
} fr_mailbox_t;

struct fr_coro {
    int id;
    fr_coro_fn fn;
    void *state;
    size_t state_size;
    fr_coro_status_t status;
    int step;
    fr_process_t *proc;
    struct fr_coro *next;
};

struct fr_process {
    char *name;
    fr_scheduler_t *sched;
    fr_mailbox_t mailbox;
    fr_coro_t *coros;
    fr_coro_t *coro_tail;
    int next_coro_id;
    fr_coro_fn receive_handler;
    size_t receive_state_size;
    int is_supervisor;
    fr_restart_policy_t restart_policy;
    fr_process_t **children;
    size_t child_count;
    struct fr_process *next;
};

struct fr_scheduler {
    fr_process_t *processes;
    int worker_count;
    int running;
};

fr_scheduler_t *fr_scheduler_create(int worker_count) {
    fr_scheduler_t *s = (fr_scheduler_t *)calloc(1, sizeof(fr_scheduler_t));
    if (!s) return NULL;
    s->worker_count = worker_count > 0 ? worker_count : 1;
    return s;
}

void fr_scheduler_destroy(fr_scheduler_t *sched) {
    if (!sched) return;
    fr_process_t *p = sched->processes;
    while (p) {
        fr_process_t *next = p->next;
        fr_process_destroy(p);
        p = next;
    }
    free(sched);
}

void fr_scheduler_add_process(fr_scheduler_t *sched, fr_process_t *proc) {
    proc->sched = sched;
    proc->next = sched->processes;
    sched->processes = proc;
}

static int mailbox_push(fr_mailbox_t *mb, fr_msg_t msg) {
    if (mb->count >= MAILBOX_CAP) return 0;
    mb->msgs[mb->tail] = msg;
    mb->tail = (mb->tail + 1) % MAILBOX_CAP;
    mb->count++;
    return 1;
}

static int mailbox_pop(fr_mailbox_t *mb, fr_msg_t *out) {
    if (mb->count == 0) return 0;
    *out = mb->msgs[mb->head];
    mb->head = (mb->head + 1) % MAILBOX_CAP;
    mb->count--;
    return 1;
}

fr_process_t *fr_process_create(const char *name) {
    fr_process_t *p = (fr_process_t *)calloc(1, sizeof(fr_process_t));
    if (!p) return NULL;
    p->name = fr_strdup(name ? name : "process");
    p->next_coro_id = 1;
    return p;
}

void fr_process_destroy(fr_process_t *proc) {
    if (!proc) return;
    fr_coro_t *c = proc->coros;
    while (c) {
        fr_coro_t *next = c->next;
        free(c->state);
        free(c);
        c = next;
    }
    free(proc->children);
    free(proc->name);
    free(proc);
}

fr_scheduler_t *fr_process_scheduler(fr_process_t *proc) {
    return proc->sched;
}

const char *fr_process_name(fr_process_t *proc) {
    return proc ? proc->name : "";
}

void fr_process_set_receive_handler(fr_process_t *proc, fr_coro_fn handler, size_t state_size) {
    proc->receive_handler = handler;
    proc->receive_state_size = state_size;
}

fr_coro_t *fr_coro_spawn(fr_process_t *proc, fr_coro_fn fn, void *init_state, size_t state_size) {
    fr_coro_t *c = (fr_coro_t *)calloc(1, sizeof(fr_coro_t));
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
    c->status = FR_CORO_RUNNING;
    c->step = 0;
    c->proc = proc;
    if (!proc->coros) proc->coros = proc->coro_tail = c;
    else { proc->coro_tail->next = c; proc->coro_tail = c; }
    free(init_state);
    return c;
}

fr_coro_status_t fr_yield(fr_coro_t *coro) {
    coro->status = FR_CORO_YIELDED;
    return FR_CORO_YIELDED;
}

int fr_coro_id(fr_coro_t *coro) {
    return coro ? coro->id : -1;
}

fr_process_t *fr_coro_process(fr_coro_t *coro) {
    return coro ? coro->proc : NULL;
}

int fr_coro_step(fr_coro_t *coro) {
    return coro ? coro->step : 0;
}

void fr_coro_set_step(fr_coro_t *coro, int step) {
    if (coro) coro->step = step;
}

void fr_send(fr_process_t *dst, int tag, int64_t value, void *payload, size_t payload_size) {
    if (!dst) return;
    fr_msg_t msg = { tag, value, payload, payload_size, NULL };
    mailbox_push(&dst->mailbox, msg);
}

int fr_try_recv(fr_process_t *self, fr_msg_t *out) {
    if (!self || !out) return 0;
    return mailbox_pop(&self->mailbox, out);
}

int fr_recv(fr_process_t *self, fr_msg_t *out) {
    while (!fr_try_recv(self, out)) {
        /* cooperative wait */
    }
    return 1;
}

fr_process_t *fr_supervisor_create(const char *name, fr_restart_policy_t policy) {
    fr_process_t *p = fr_process_create(name);
    p->is_supervisor = 1;
    p->restart_policy = policy;
    return p;
}

void fr_supervisor_add_child(fr_process_t *supervisor, fr_process_t *child) {
    supervisor->child_count++;
    supervisor->children = (fr_process_t **)realloc(
        supervisor->children, supervisor->child_count * sizeof(fr_process_t *));
    supervisor->children[supervisor->child_count - 1] = child;
}

static int run_coroutine_round(fr_process_t *proc) {
    int progressed = 0;
    for (fr_coro_t *c = proc->coros; c; c = c->next) {
        if (c->status == FR_CORO_DONE || c->status == FR_CORO_ERROR) continue;
        fr_coro_status_t st = c->fn(c, c->state);
        c->status = st;
        if (st == FR_CORO_YIELDED) {
            c->status = FR_CORO_RUNNING;
            progressed = 1;
        } else if (st == FR_CORO_DONE) {
            progressed = 1;
        }
    }
    return progressed;
}

void fr_scheduler_run(fr_scheduler_t *sched) {
    if (!sched) return;
    sched->running = 1;
    int idle_rounds = 0;
    while (sched->running) {
        int any_active = 0;
        int any_progress = 0;
        for (fr_process_t *p = sched->processes; p; p = p->next) {
            for (fr_coro_t *c = p->coros; c; c = c->next) {
                if (c->status != FR_CORO_DONE && c->status != FR_CORO_ERROR) {
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
