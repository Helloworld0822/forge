#ifndef FORGE_WORK_QUEUE_H
#define FORGE_WORK_QUEUE_H

#include "forge/thread.h"
#include <stddef.h>

typedef struct fr_coro fr_coro_t;

typedef struct fr_run_node {
    fr_coro_t *coro;
    struct fr_run_node *next;
} fr_run_node_t;

typedef struct {
    fr_mutex_t *lock;
    fr_run_node_t *head;
    fr_run_node_t *tail;
    size_t count;
} fr_run_queue_t;

void fr_run_queue_init(fr_run_queue_t *q);
void fr_run_queue_destroy(fr_run_queue_t *q);
void fr_run_queue_push(fr_run_queue_t *q, fr_coro_t *coro);
fr_coro_t *fr_run_queue_pop(fr_run_queue_t *q);
fr_coro_t *fr_run_queue_steal(fr_run_queue_t *victim, fr_run_queue_t *thief);

#endif
