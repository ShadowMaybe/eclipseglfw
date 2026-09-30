/*
 * eclipseglfw — the input event queue.
 *
 * Copyright (c) 2026 Shadow.
 * SPDX-License-Identifier: MIT
 *
 * A fixed ring, written by the launcher's UI thread and drained by whoever
 * polls. The choice to make here is what a full queue does, and it is not
 * "grow": this buffer sits between a thread that must never stall and a
 * consumer that may not be running at all, so a burst of input is allowed to
 * lose its tail while an unexpected allocation on the input path is not.
 * eg_post_event() reports EG_ERR_QUEUE_FULL and the caller decides — which,
 * for the launcher, is always to drop the oldest work and carry on.
 *
 * The lock is uncontended in the common case (one writer, one reader, both
 * fast) so pthread's mutex, which is a compare-and-swap when nobody else is
 * near, is the right size for it.
 */

#include "eclipseglfw.h"

#include <pthread.h>

enum { EG_EVENT_CAPACITY = 512 };

static pthread_mutex_t g_queue_lock = PTHREAD_MUTEX_INITIALIZER;
static EgEvent g_events[EG_EVENT_CAPACITY];
static size_t g_oldest;
static size_t g_count;

EgStatus eg_post_event(const EgEvent *event) {
    EgStatus status;

    if (event == NULL) {
        return EG_ERR_INVALID_ARGUMENT;
    }

    if (!eg_is_started()) {
        return EG_ERR_UNINITIALISED;
    }

    pthread_mutex_lock(&g_queue_lock);

    if (g_count == (size_t)EG_EVENT_CAPACITY) {
        status = EG_ERR_QUEUE_FULL;
    } else {
        /* Newest goes past the tail, not at a fixed slot: the window slides. */
        const size_t newest = (g_oldest + g_count) % (size_t)EG_EVENT_CAPACITY;
        g_events[newest] = *event;
        ++g_count;
        status = EG_OK;
    }

    pthread_mutex_unlock(&g_queue_lock);

    return status;
}

bool eg_next_event(EgEvent *out) {
    bool taken = false;

    if (out == NULL) {
        return false;
    }

    pthread_mutex_lock(&g_queue_lock);

    if (g_count > 0) {
        *out = g_events[g_oldest];
        g_oldest = (g_oldest + 1) % (size_t)EG_EVENT_CAPACITY;
        --g_count;
        taken = true;
    }

    pthread_mutex_unlock(&g_queue_lock);

    return taken;
}

void eg_clear_events(void) {
    pthread_mutex_lock(&g_queue_lock);
    g_oldest = 0;
    g_count = 0;
    pthread_mutex_unlock(&g_queue_lock);
}

size_t eg_pending_events(void) {
    size_t pending;

    pthread_mutex_lock(&g_queue_lock);
    pending = g_count;
    pthread_mutex_unlock(&g_queue_lock);

    return pending;
}
