/*
 * eclipseglfw — the launcher's window, surface and input bridge.
 *
 * Copyright (c) 2026 Shadow.
 * SPDX-License-Identifier: MIT
 *
 * This header is the whole contract between the launcher and the shim: the
 * JNI layer drives every function here, and the host tests exercise them
 * without a device. Nothing outside the library includes it.
 *
 * Two things are deliberately absent. The renderer binding is not here yet —
 * a surface is held, sized and pumped, but no context is made current, so
 * eg_present() counts frames rather than swapping them. And the windowing
 * entry points the game's runtime resolves are a later layer built on top of
 * what this declares; they belong in their own header so this one stays
 * readable as the launcher-facing half.
 */
#ifndef ECLIPSEGLFW_H
#define ECLIPSEGLFW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ECLIPSEGLFW_VERSION_MAJOR 0
#define ECLIPSEGLFW_VERSION_MINOR 1
#define ECLIPSEGLFW_VERSION_PATCH 0

/*
 * The concrete definition comes from <android/native_window.h>, included only
 * by the translation units that dereference it. Forward-declaring here keeps
 * this header usable in host tests, which have no Android SDK at all — and a
 * duplicate typedef would be legal C11 anyway, so the two never conflict.
 */
struct ANativeWindow;

/** Result of any operation in this library. Zero always means success. */
typedef enum EgStatus {
    EG_OK = 0,
    /** eg_startup() has not been called, or it failed. */
    EG_ERR_UNINITIALISED = 1,
    /** A NULL pointer or an out-of-range value was passed. */
    EG_ERR_INVALID_ARGUMENT = 2,
    /** The operation needs a surface and none is bound. */
    EG_ERR_NO_SURFACE = 3,
    /** The event queue is full; the caller should drain before posting again. */
    EG_ERR_QUEUE_FULL = 4,
    /** Android refused the operation — the surface went away underneath us. */
    EG_ERR_PLATFORM = 5
} EgStatus;

/**
 * A short, static description of a status. Never NULL, never allocated: safe
 * to hand straight to a logger from any thread.
 */
const char *eg_status_string(EgStatus status);

/* ------------------------------------------------------------------
 * Window flags.
 *
 * These codes are the attribute numbers the runtime already speaks, so the
 * launcher can hand them over unchanged. They are not this library's
 * invention, and adding more later means extending the table in eg_state.c
 * rather than changing anything here.
 * ------------------------------------------------------------------ */

enum {
    /** The game window is on screen. */
    EG_WINDOW_VISIBLE = 0x00020004,
    /** The pointer is over the game window. */
    EG_WINDOW_HOVERED = 0x0002000B
};

/* ------------------------------------------------------------------
 * Lifecycle.
 * ------------------------------------------------------------------ */

/**
 * Bring the library up. Idempotent: the launcher's static initializer and any
 * later explicit call may both run it, and the second one is a no-op that
 * still returns EG_OK.
 *
 * @return EG_OK once the library can accept work
 */
EgStatus eg_startup(void);

/** Whether eg_startup() has completed successfully. */
bool eg_is_started(void);

/* ------------------------------------------------------------------
 * Surface.
 *
 * The launcher owns the SurfaceView; this half takes a reference it controls
 * and lets go of it explicitly, because Android will happily hand out a
 * surface whose window disappears mid-frame.
 * ------------------------------------------------------------------ */

/**
 * Take a reference on a window and record its size.
 *
 * Binding a second window releases the first, so the launcher does not have to
 * be careful about ordering — a recreated surface is one call, not a release
 * followed by a bind.
 *
 * @param window the window to hold; must not be NULL
 * @return EG_OK, or EG_ERR_UNINITIALISED / EG_ERR_INVALID_ARGUMENT
 */
EgStatus eg_bind_surface(struct ANativeWindow *window);

/**
 * Release the held window. Calling it with nothing bound is not an error: the
 * launcher's teardown path can legitimately run twice.
 *
 * @return EG_OK whether or not a window was held
 */
EgStatus eg_unbind_surface(void);

/**
 * Account for one produced frame.
 *
 * This counts rather than swaps: the renderer binding that turns the count
 * into an eglSwapBuffers arrives with the context layer. Until then the call
 * still validates the same conditions the swap will, so the launcher's frame
 * path is exercised against the real surface lifetime.
 *
 * @return EG_OK, or EG_ERR_NO_SURFACE when nothing is bound
 */
EgStatus eg_present(void);

/** Whether a window is currently held. */
bool eg_has_surface(void);

/** Width of the bound window in pixels, or 0 when unbound. */
uint32_t eg_surface_width(void);

/** Height of the bound window in pixels, or 0 when unbound. */
uint32_t eg_surface_height(void);

/** Frames accounted for by eg_present() since startup. */
uint64_t eg_frame_count(void);

/* ------------------------------------------------------------------
 * Window flags, readable as well as settable.
 * ------------------------------------------------------------------ */

/**
 * Record a window attribute value.
 *
 * Unknown codes are stored rather than rejected, so a caller may pre-register
 * an attribute before this side grows a meaning for it.
 *
 * @param flag    attribute code
 * @param enabled the value to store
 * @return EG_OK, or EG_ERR_UNINITIALISED / EG_ERR_INVALID_ARGUMENT / EG_ERR_QUEUE_FULL
 *         when the fixed flag table is exhausted
 */
EgStatus eg_set_window_flag(int32_t flag, bool enabled);

/**
 * Read a window attribute back.
 *
 * @param  flag attribute code
 * @return the stored value, or false when the code was never set
 */
bool eg_get_window_flag(int32_t flag);

/* ------------------------------------------------------------------
 * Input events.
 *
 * One ring buffer, written by the launcher's UI thread and drained by whoever
 * polls. The queue never blocks a writer and never allocates: a full queue
 * reports EG_ERR_QUEUE_FULL instead of growing, because dropping a burst of
 * input is recoverable and an unbounded allocation on the input path is not.
 * ------------------------------------------------------------------ */

/** What kind of thing an event describes. */
typedef enum EgEventKind {
    EG_EVENT_CURSOR = 0,
    EG_EVENT_KEY,
    EG_EVENT_TEXT,
    EG_EVENT_BUTTON,
    EG_EVENT_SCROLL,
    EG_EVENT_KIND_COUNT
} EgEventKind;

/**
 * One input transition.
 *
 * The fields are shared by all kinds rather than unioned: the struct stays
 * under sixty bytes, and a union would buy back less than it costs in
 * "which member is live for this kind" mistakes.
 *
 * <ul>
 *   <li>EG_EVENT_CURSOR — x and y hold the position.</li>
 *   <li>EG_EVENT_KEY — code is the key in the platform's own numbering, action
 *       the transition, modifiers the state held at the time. Translating it
 *       to the game's numbering is the polling layer's job, so that the table
 *       lives next to the code that consumes its output.</li>
 *   <li>EG_EVENT_TEXT — character holds one code point.</li>
 *   <li>EG_EVENT_BUTTON — code is the button, action the transition.</li>
 *   <li>EG_EVENT_SCROLL — x and y hold the deltas.</li>
 * </ul>
 */
typedef struct EgEvent {
    EgEventKind kind;
    int32_t code;
    int32_t action;
    int32_t modifiers;
    double x;
    double y;
    uint32_t character;
} EgEvent;

/**
 * Queue one event. Copies the struct; the caller may reuse it immediately.
 *
 * Safe from any thread, and safe with no surface bound — input arrives before
 * and after the surface does.
 *
 * @param event the event to queue; must not be NULL
 * @return EG_OK, or EG_ERR_UNINITIALISED / EG_ERR_INVALID_ARGUMENT /
 *         EG_ERR_QUEUE_FULL
 */
EgStatus eg_post_event(const EgEvent *event);

/**
 * Take the oldest queued event.
 *
 * @param out receives the event; must not be NULL
 * @return true when an event was taken, false when the queue was empty
 */
bool eg_next_event(EgEvent *out);

/** Drop everything queued. Used when the game window goes away mid-burst. */
void eg_clear_events(void);

/** How many events are waiting. */
size_t eg_pending_events(void);

/* ------------------------------------------------------------------
 * Controllers.
 * ------------------------------------------------------------------ */

/**
 * Record that a controller appeared. Latched rather than delivered: the
 * runtime may not be listening yet, so the announcement waits until somebody
 * asks for it instead of vanishing into an unregistered callback.
 */
void eg_announce_gamepad(void);

/**
 * Read and clear the controller announcement.
 *
 * @return true when a controller arrived since the last call, false otherwise
 */
bool eg_take_gamepad_announcement(void);

#ifdef __cplusplus
}
#endif

#endif /* ECLIPSEGLFW_H */
