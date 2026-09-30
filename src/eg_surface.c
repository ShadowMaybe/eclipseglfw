/*
 * eclipseglfw — surface ownership.
 *
 * Copyright (c) 2026 Shadow.
 * SPDX-License-Identifier: MIT
 *
 * The launcher owns the SurfaceView; this half takes a reference it controls
 * and lets go of it explicitly. That explicitness is the whole point: Android
 * will happily hand out a window that disappears underneath a frame in
 * flight, so nothing here assumes a surface outlives the call that used it.
 *
 * Binding is deliberately forgiving. A second bind replaces the first, and an
 * unbind with nothing bound succeeds — the launcher's teardown path can run
 * twice, and making it careful would be asking the wrong half to be careful.
 *
 * Presenting counts frames rather than swapping them. The renderer binding
 * that turns the count into eglSwapBuffers belongs with the context layer;
 * until it lands, this still validates the same conditions the swap will, so
 * the frame path is already exercised against real surface lifetime.
 */

#include "eclipseglfw.h"

#include <android/native_window.h>
#include <pthread.h>

static pthread_mutex_t g_surface_lock = PTHREAD_MUTEX_INITIALIZER;
static struct ANativeWindow *g_window;
static uint32_t g_width;
static uint32_t g_height;
static uint64_t g_frames;

/* Android reports 0 or a negative value for a window that has not settled;
 * the callers below want a dimension they can pass to a renderer. */
static uint32_t measured_dimension(int32_t value) {
    return value > 0 ? (uint32_t)value : 0;
}

EgStatus eg_bind_surface(struct ANativeWindow *window) {
    int32_t width;
    int32_t height;

    if (window == NULL) {
        return EG_ERR_INVALID_ARGUMENT;
    }

    if (!eg_is_started()) {
        return EG_ERR_UNINITIALISED;
    }

    /* Measure before taking the lock: these calls into the buffer queue can
     * block, and holding the lock across them would stall a present. */
    width = ANativeWindow_getWidth(window);
    height = ANativeWindow_getHeight(window);

    pthread_mutex_lock(&g_surface_lock);

    if (g_window != NULL) {
        ANativeWindow_release(g_window);
    }
    ANativeWindow_acquire(window);
    g_window = window;
    g_width = measured_dimension(width);
    g_height = measured_dimension(height);

    pthread_mutex_unlock(&g_surface_lock);

    return EG_OK;
}

EgStatus eg_unbind_surface(void) {
    pthread_mutex_lock(&g_surface_lock);

    if (g_window != NULL) {
        ANativeWindow_release(g_window);
        g_window = NULL;
    }
    g_width = 0;
    g_height = 0;

    pthread_mutex_unlock(&g_surface_lock);

    return EG_OK;
}

EgStatus eg_present(void) {
    EgStatus status;

    if (!eg_is_started()) {
        return EG_ERR_UNINITIALISED;
    }

    pthread_mutex_lock(&g_surface_lock);

    if (g_window == NULL) {
        status = EG_ERR_NO_SURFACE;
    } else {
        ++g_frames;
        status = EG_OK;
    }

    pthread_mutex_unlock(&g_surface_lock);

    return status;
}

bool eg_has_surface(void) {
    bool bound;

    pthread_mutex_lock(&g_surface_lock);
    bound = g_window != NULL;
    pthread_mutex_unlock(&g_surface_lock);

    return bound;
}

uint32_t eg_surface_width(void) {
    uint32_t width;

    pthread_mutex_lock(&g_surface_lock);
    width = g_width;
    pthread_mutex_unlock(&g_surface_lock);

    return width;
}

uint32_t eg_surface_height(void) {
    uint32_t height;

    pthread_mutex_lock(&g_surface_lock);
    height = g_height;
    pthread_mutex_unlock(&g_surface_lock);

    return height;
}

uint64_t eg_frame_count(void) {
    uint64_t frames;

    pthread_mutex_lock(&g_surface_lock);
    frames = g_frames;
    pthread_mutex_unlock(&g_surface_lock);

    return frames;
}
