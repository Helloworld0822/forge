#include "forge_runtime.h"
#include "work_queue.h"
#include "forge/event.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sched.h>
#include <unistd.h>

static char *fr_strdup(const char *s) {
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

#define MAILBOX_CAP 256

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
    int on_queue;
    fr_process_t *proc;
    int await_fd;
    uint32_t await_events;
    int await_ready;
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
    int worker_id;
    pthread_mutex_t lock;
    pthread_cond_t msg_cond;
    struct fr_process *next;
};

struct fr_scheduler {
    fr_process_t *processes;
    int worker_count;
    int running;
    int done;
    pthread_t *workers;
    fr_run_queue_t *worker_queues;
    fr_event_loop_t *event_loop;
    pthread_mutex_t lock;
    pthread_cond_t idle_cond;
};

static _Thread_local fr_coro_t *tls_current_coro = NULL;

fr_coro_t *fr_coro_current(void) {
    return tls_current_coro;
}

static int default_worker_count(int n) {
    if (n > 0) return n;
    long cpus = sysconf(_SC_NPROCESSORS_ONLN);
    return cpus > 0 ? (int)cpus : 4;
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

static void enqueue_coro(fr_scheduler_t *sched, fr_coro_t *coro) {
    if (!coro || coro->on_queue) return;
    if (coro->status == FR_CORO_DONE || coro->status == FR_CORO_ERROR) return;
    coro->on_queue = 1;
    int wid = coro->proc->worker_id % sched->worker_count;
    fr_run_queue_push(&sched->worker_queues[wid], coro);
    pthread_cond_broadcast(&sched->idle_cond);
}

static void event_resume_cb(fr_event_loop_t *loop, int fd, uint32_t events, void *userdata) {
    (void)loop;
    (void)fd;
    (void)events;
    fr_coro_t *coro = (fr_coro_t *)userdata;
    if (!coro) return;
    coro->await_ready = 1;
    coro->status = FR_CORO_RUNNING;
    if (coro->proc && coro->proc->sched) {
        enqueue_coro(coro->proc->sched, coro);
    }
}

static int run_coro_step(fr_coro_t *c) {
    tls_current_coro = c;
    fr_coro_status_t st = c->fn(c, c->state);
    tls_current_coro = NULL;
    c->status = st;
    if (st == FR_CORO_YIELDED) {
        c->status = FR_CORO_RUNNING;
        return 1;
    }
    if (st == FR_CORO_WAITING_RECV || st == FR_CORO_WAITING_IO) return 0;
    return st == FR_CORO_DONE || st == FR_CORO_ERROR ? 1 : 1;
}

static int process_has_active(fr_process_t *p) {
    for (fr_coro_t *c = p->coros; c; c = c->next) {
        if (c->status != FR_CORO_DONE && c->status != FR_CORO_ERROR) return 1;
    }
    return 0;
}

static int sched_any_active(fr_scheduler_t *sched) {
    for (fr_process_t *p = sched->processes; p; p = p->next) {
        if (process_has_active(p)) return 1;
    }
    return 0;
}

static void scan_enqueue_runnable(fr_scheduler_t *sched) {
    for (fr_process_t *p = sched->processes; p; p = p->next) {
        pthread_mutex_lock(&p->lock);
        for (fr_coro_t *c = p->coros; c; c = c->next) {
            if (c->status == FR_CORO_RUNNING) {
                enqueue_coro(sched, c);
            }
        }
        pthread_mutex_unlock(&p->lock);
    }
}

typedef struct {
    fr_scheduler_t *sched;
    int wid;
} fr_worker_arg_t;

static void *worker_main(void *arg) {
    fr_worker_arg_t *wa = (fr_worker_arg_t *)arg;
    fr_scheduler_t *sched = wa->sched;
    int wid = wa->wid;
    free(wa);

    while (sched->running) {
        fr_coro_t *coro = fr_run_queue_pop(&sched->worker_queues[wid]);
        if (!coro) {
            for (int i = 0; i < sched->worker_count; i++) {
                if (i == wid) continue;
                coro = fr_run_queue_steal(&sched->worker_queues[i], &sched->worker_queues[wid]);
                if (coro) break;
            }
        }
        if (!coro) {
            if (!sched_any_active(sched)) {
                sched->done = 1;
                break;
            }
            sched_yield();
            continue;
        }

        coro->on_queue = 0;
        run_coro_step(coro);
        if (coro->status == FR_CORO_RUNNING) {
            enqueue_coro(sched, coro);
        }
    }
    return NULL;
}

fr_scheduler_t *fr_scheduler_create(int worker_count) {
    fr_scheduler_t *s = (fr_scheduler_t *)calloc(1, sizeof(fr_scheduler_t));
    if (!s) return NULL;
    s->worker_count = default_worker_count(worker_count);
    s->event_loop = fr_event_loop_create();
    if (s->event_loop) fr_event_loop_set_cb(s->event_loop, event_resume_cb);
    pthread_mutex_init(&s->lock, NULL);
    pthread_cond_init(&s->idle_cond, NULL);
    s->worker_queues = (fr_run_queue_t *)calloc((size_t)s->worker_count, sizeof(fr_run_queue_t));
    s->workers = (pthread_t *)calloc((size_t)s->worker_count, sizeof(pthread_t));
    if (!s->worker_queues || !s->workers) {
        fr_scheduler_destroy(s);
        return NULL;
    }
    for (int i = 0; i < s->worker_count; i++) {
        fr_run_queue_init(&s->worker_queues[i]);
    }
    return s;
}

void fr_scheduler_destroy(fr_scheduler_t *sched) {
    if (!sched) return;
    sched->running = 0;
    pthread_cond_broadcast(&sched->idle_cond);
    if (sched->workers) {
        for (int i = 0; i < sched->worker_count; i++) {
            if (sched->workers[i]) pthread_join(sched->workers[i], NULL);
        }
    }
    if (sched->worker_queues) {
        for (int i = 0; i < sched->worker_count; i++) {
            fr_run_queue_destroy(&sched->worker_queues[i]);
        }
    }
    fr_process_t *p = sched->processes;
    while (p) {
        fr_process_t *next = p->next;
        fr_process_destroy(p);
        p = next;
    }
    fr_event_loop_destroy(sched->event_loop);
    pthread_mutex_destroy(&sched->lock);
    pthread_cond_destroy(&sched->idle_cond);
    free(sched->worker_queues);
    free(sched->workers);
    free(sched);
}

void fr_scheduler_add_process(fr_scheduler_t *sched, fr_process_t *proc) {
    static int next_worker = 0;
    proc->sched = sched;
    proc->worker_id = next_worker++ % sched->worker_count;
    pthread_mutex_lock(&sched->lock);
    proc->next = sched->processes;
    sched->processes = proc;
    pthread_mutex_unlock(&sched->lock);
}

fr_event_loop_t *fr_scheduler_event_loop(fr_scheduler_t *sched) {
    return sched ? sched->event_loop : NULL;
}

int fr_scheduler_worker_count(fr_scheduler_t *sched) {
    return sched ? sched->worker_count : 0;
}

void fr_scheduler_run(fr_scheduler_t *sched) {
    if (!sched) return;
    sched->running = 1;
    sched->done = 0;
    scan_enqueue_runnable(sched);
    for (int i = 0; i < sched->worker_count; i++) {
        fr_worker_arg_t *wa = (fr_worker_arg_t *)malloc(sizeof(fr_worker_arg_t));
        wa->sched = sched;
        wa->wid = i;
        pthread_create(&sched->workers[i], NULL, worker_main, wa);
    }
    pthread_mutex_lock(&sched->lock);
    while (!sched->done) {
        fr_event_loop_poll(sched->event_loop, 1);
        if (!sched_any_active(sched)) sched->done = 1;
    }
    pthread_mutex_unlock(&sched->lock);
    sched->running = 0;
    pthread_cond_broadcast(&sched->idle_cond);
    for (int i = 0; i < sched->worker_count; i++) {
        pthread_join(sched->workers[i], NULL);
        sched->workers[i] = 0;
    }
}

fr_process_t *fr_process_create(const char *name) {
    fr_process_t *p = (fr_process_t *)calloc(1, sizeof(fr_process_t));
    if (!p) return NULL;
    p->name = fr_strdup(name ? name : "process");
    p->next_coro_id = 1;
    pthread_mutex_init(&p->lock, NULL);
    pthread_cond_init(&p->msg_cond, NULL);
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
    for (size_t i = 0; i < proc->mailbox.count; i++) {
        fr_msg_t *m = &proc->mailbox.msgs[(proc->mailbox.head + i) % MAILBOX_CAP];
        if (m->owns_payload && m->payload) free(m->payload);
    }
    pthread_mutex_destroy(&proc->lock);
    pthread_cond_destroy(&proc->msg_cond);
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
    c->await_fd = -1;
    if (!proc->coros) proc->coros = proc->coro_tail = c;
    else { proc->coro_tail->next = c; proc->coro_tail = c; }
    free(init_state);
    if (proc->sched) enqueue_coro(proc->sched, c);
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
    fr_msg_t msg = { tag, value, payload, payload_size, payload ? 1 : 0, NULL };
    pthread_mutex_lock(&dst->lock);
    mailbox_push(&dst->mailbox, msg);
    pthread_cond_broadcast(&dst->msg_cond);
    pthread_mutex_unlock(&dst->lock);
    if (dst->sched) pthread_cond_broadcast(&dst->sched->idle_cond);
}

int fr_try_recv(fr_process_t *self, fr_msg_t *out) {
    if (!self || !out) return 0;
    pthread_mutex_lock(&self->lock);
    int ok = mailbox_pop(&self->mailbox, out);
    pthread_mutex_unlock(&self->lock);
    return ok;
}

int fr_recv(fr_process_t *self, fr_msg_t *out) {
    if (!self || !out) return 0;
    pthread_mutex_lock(&self->lock);
    while (!mailbox_pop(&self->mailbox, out)) {
        pthread_cond_wait(&self->msg_cond, &self->lock);
    }
    pthread_mutex_unlock(&self->lock);
    return 1;
}

void fr_msg_free_payload(fr_msg_t *msg) {
    if (msg && msg->owns_payload && msg->payload) {
        free(msg->payload);
        msg->payload = NULL;
        msg->owns_payload = 0;
    }
}

int64_t fr_await_fd(fr_coro_t *coro, int64_t fd, uint32_t events) {
    if (!coro) return 1;
    if (coro->await_ready && coro->await_fd == (int)fd) {
        coro->await_ready = 0;
        coro->await_fd = -1;
        fr_event_loop_del(coro->proc->sched->event_loop, (int)fd);
        return 1;
    }
    coro->await_fd = (int)fd;
    coro->await_events = events;
    coro->await_ready = 0;
    coro->status = FR_CORO_WAITING_IO;
    fr_event_loop_add(coro->proc->sched->event_loop, (int)fd, events | EPOLLONESHOT, coro);
    return 0;
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

int fr_event_poll(fr_scheduler_t *sched, int timeout_ms) {
    if (!sched || !sched->event_loop) return -1;
    return fr_event_loop_poll(sched->event_loop, timeout_ms);
}

int64_t fr_event_add_read(fr_scheduler_t *sched, int64_t fd) {
    if (!sched || !sched->event_loop || fd < 0) return -1;
    if (fr_event_loop_add(sched->event_loop, (int)fd, EPOLLIN | EPOLLET, NULL) < 0) return -1;
    return fd;
}
