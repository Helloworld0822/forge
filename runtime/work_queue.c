#include "work_queue.h"
#include <stdlib.h>

void fr_run_queue_init(fr_run_queue_t *q) {
    pthread_mutex_init(&q->lock, NULL);
    q->head = q->tail = NULL;
    q->count = 0;
}

void fr_run_queue_destroy(fr_run_queue_t *q) {
    pthread_mutex_lock(&q->lock);
    fr_run_node_t *n = q->head;
    while (n) {
        fr_run_node_t *next = n->next;
        free(n);
        n = next;
    }
    q->head = q->tail = NULL;
    q->count = 0;
    pthread_mutex_unlock(&q->lock);
    pthread_mutex_destroy(&q->lock);
}

void fr_run_queue_push(fr_run_queue_t *q, fr_coro_t *coro) {
    fr_run_node_t *node = (fr_run_node_t *)malloc(sizeof(fr_run_node_t));
    if (!node) return;
    node->coro = coro;
    node->next = NULL;
    pthread_mutex_lock(&q->lock);
    if (q->tail) q->tail->next = node;
    else q->head = node;
    q->tail = node;
    q->count++;
    pthread_mutex_unlock(&q->lock);
}

static fr_coro_t *pop_locked(fr_run_queue_t *q) {
    if (!q->head) return NULL;
    fr_run_node_t *node = q->head;
    q->head = node->next;
    if (!q->head) q->tail = NULL;
    q->count--;
    fr_coro_t *coro = node->coro;
    free(node);
    return coro;
}

fr_coro_t *fr_run_queue_pop(fr_run_queue_t *q) {
    pthread_mutex_lock(&q->lock);
    fr_coro_t *coro = pop_locked(q);
    pthread_mutex_unlock(&q->lock);
    return coro;
}

fr_coro_t *fr_run_queue_steal(fr_run_queue_t *victim, fr_run_queue_t *thief) {
    (void)thief;
    pthread_mutex_lock(&victim->lock);
    fr_coro_t *coro = pop_locked(victim);
    pthread_mutex_unlock(&victim->lock);
    return coro;
}
