/*
 * eclipseglfw — library lifecycle, window flags, controller latch.
 *
 * Copyright (c) 2026 Shadow.
 * SPDX-License-Identifier: MIT
 *
 * One lock guards everything here. The state is a handful of booleans and the
 * operations are short reads and writes, so a single mutex costs less than the
 * atomics it would take to reason about each field separately — and nothing in
 * this file waits on anything else while holding it, so it cannot be part of a
 * deadlock.
 */

#include "eclipseglfw.h"

#include <pthread.h>

static const char *const kStatusText[] = {
    "success",
    "library not started",
    "invalid argument",
    "no surface bound",
    "event queue full",
    "platform error",
};

typedef struct EgWindowFlags {
    bool visible;
    bool hovered;
} EgWindowFlags;

static pthread_mutex_t g_state_lock = PTHREAD_MUTEX_INITIALIZER;
static bool g_started;
static bool g_gamepad_pending;
static EgWindowFlags g_window_flags;

const char *eg_status_string(EgStatus status) {
    /* Not called "index": POSIX declares index() in strings.h, and a local
     * shadowing it is a -Wshadow failure on some libcs and not others. */
    const size_t count = sizeof(kStatusText) / sizeof(kStatusText[0]);
    const int position = (int)status;

    if (position < 0 || (size_t)position >= count) {
        return "unknown status";
    }

    return kStatusText[position];
}

EgStatus eg_startup(void) {
    pthread_mutex_lock(&g_state_lock);
    g_started = true;
    pthread_mutex_unlock(&g_state_lock);

    return EG_OK;
}

bool eg_is_started(void) {
    bool started;

    pthread_mutex_lock(&g_state_lock);
    started = g_started;
    pthread_mutex_unlock(&g_state_lock);

    return started;
}

EgStatus eg_set_window_flag(int32_t flag, bool enabled) {
    EgStatus status = EG_OK;

    pthread_mutex_lock(&g_state_lock);

    if (!g_started) {
        status = EG_ERR_UNINITIALISED;
    } else {
        switch (flag) {
            case EG_WINDOW_VISIBLE:
                g_window_flags.visible = enabled;
                break;
            case EG_WINDOW_HOVERED:
                g_window_flags.hovered = enabled;
                break;
            default:
                status = EG_ERR_INVALID_ARGUMENT;
                break;
        }
    }

    pthread_mutex_unlock(&g_state_lock);

    return status;
}

bool eg_get_window_flag(int32_t flag) {
    bool value = false;

    pthread_mutex_lock(&g_state_lock);

    switch (flag) {
        case EG_WINDOW_VISIBLE:
            value = g_window_flags.visible;
            break;
        case EG_WINDOW_HOVERED:
            value = g_window_flags.hovered;
            break;
        default:
            break;
    }

    pthread_mutex_unlock(&g_state_lock);

    return value;
}

void eg_announce_gamepad(void) {
    pthread_mutex_lock(&g_state_lock);
    g_gamepad_pending = true;
    pthread_mutex_unlock(&g_state_lock);
}

bool eg_take_gamepad_announcement(void) {
    bool pending;

    pthread_mutex_lock(&g_state_lock);
    pending = g_gamepad_pending;
    g_gamepad_pending = false;
    pthread_mutex_unlock(&g_state_lock);

    return pending;
}
