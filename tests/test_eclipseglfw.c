/*
 * eclipseglfw — host unit tests for the parts that need no device.
 *
 * Copyright (c) 2026 Shadow.
 * SPDX-License-Identifier: MIT
 *
 * The surface holder and the JNI binding are Android's to prove, and the NDK
 * build covers them. What is ours and testable anywhere is the state machine
 * behind them: whether the library knows it started, whether a flag round
 * trips, whether the queue keeps its order and knows when it is full, and
 * whether a controller announcement is delivered exactly once.
 *
 * State is process-global, so the order of the calls below is the test: the
 * two cases that must run before startup are first, and everything after
 * assumes the library is up.
 */

#include "eclipseglfw.h"

#include <assert.h>
#include <stdio.h>

/* A freshly prepared event with every field written, so no test can pass by
 * reading a zero that happened to be left over from the last one. */
static EgEvent make_event(EgEventKind kind, int32_t code) {
    EgEvent event;

    event.kind = kind;
    event.code = code;
    event.action = 1;
    event.modifiers = 0;
    event.x = 12.5;
    event.y = -3.25;
    event.character = (uint32_t)code;

    return event;
}

static void test_status_strings_are_stable(void) {
    assert(eg_status_string(EG_OK) != NULL);
    assert(eg_status_string(EG_ERR_UNINITIALISED) != NULL);
    assert(eg_status_string(EG_ERR_QUEUE_FULL) != NULL);

    /* Out-of-range must answer rather than index off the end of the table. */
    assert(eg_status_string((EgStatus)999) != NULL);
    assert(eg_status_string((EgStatus)-1) != NULL);
}

static void test_work_is_refused_before_startup(void) {
    const EgEvent event = make_event(EG_EVENT_KEY, 7);

    assert(!eg_is_started());

    /* The library must refuse rather than quietly accept work it would then
     * have no initialised state to store. */
    assert(eg_set_window_flag(EG_WINDOW_VISIBLE, true) ==
           EG_ERR_UNINITIALISED);
    assert(eg_post_event(&event) == EG_ERR_UNINITIALISED);
    assert(eg_present() == EG_ERR_UNINITIALISED);

    /* Untouched by any of the refusals above. */
    assert(!eg_get_window_flag(EG_WINDOW_VISIBLE));
    assert(eg_pending_events() == 0);
}

static void test_startup_is_idempotent(void) {
    assert(eg_is_started());
    assert(eg_startup() == EG_OK);
    assert(eg_startup() == EG_OK);
    assert(eg_is_started());
}

static void test_window_flags_round_trip(void) {
    assert(!eg_get_window_flag(EG_WINDOW_VISIBLE));
    assert(!eg_get_window_flag(EG_WINDOW_HOVERED));

    assert(eg_set_window_flag(EG_WINDOW_VISIBLE, true) == EG_OK);
    assert(eg_get_window_flag(EG_WINDOW_VISIBLE));
    assert(!eg_get_window_flag(EG_WINDOW_HOVERED));

    assert(eg_set_window_flag(EG_WINDOW_HOVERED, true) == EG_OK);
    assert(eg_get_window_flag(EG_WINDOW_HOVERED));

    assert(eg_set_window_flag(EG_WINDOW_VISIBLE, false) == EG_OK);
    assert(!eg_get_window_flag(EG_WINDOW_VISIBLE));
    assert(eg_get_window_flag(EG_WINDOW_HOVERED));

    /* A code we have no meaning for is refused, not stored as a default. */
    assert(eg_set_window_flag(0x0BADF00D, true) == EG_ERR_INVALID_ARGUMENT);
    assert(!eg_get_window_flag(0x0BADF00D));
}

static void test_queue_keeps_order(void) {
    const int expected = 32;
    EgEvent incoming;
    EgEvent outgoing;
    int i;

    eg_clear_events();
    assert(eg_pending_events() == 0);

    for (i = 0; i < expected; ++i) {
        incoming = make_event(EG_EVENT_KEY, i);
        assert(eg_post_event(&incoming) == EG_OK);
    }

    assert(eg_pending_events() == (size_t)expected);

    for (i = 0; i < expected; ++i) {
        assert(eg_next_event(&outgoing));
        assert(outgoing.code == i);
        assert(outgoing.kind == EG_EVENT_KEY);
    }

    assert(!eg_next_event(&outgoing));
    assert(eg_pending_events() == 0);
}

static void test_queue_wraps_without_losing_order(void) {
    EgEvent incoming;
    EgEvent outgoing;
    int i;

    eg_clear_events();

    /* Drain far past the point where the ring has wrapped at least once, so
     * the arithmetic that slides the window is what is under test rather
     * than the first lap around it. */
    for (i = 0; i < 3000; ++i) {
        incoming = make_event(EG_EVENT_SCROLL, i);
        assert(eg_post_event(&incoming) == EG_OK);

        assert(eg_next_event(&outgoing));
        assert(outgoing.code == i);
    }

    assert(eg_pending_events() == 0);
}

static void test_queue_reports_full_instead_of_growing(void) {
    EgEvent incoming = make_event(EG_EVENT_BUTTON, 0);
    EgEvent outgoing;
    int accepted = 0;

    eg_clear_events();

    /* The capacity is deliberately not duplicated here: filling until it
     * refuses, then confirming it still refuses, tests the behaviour the
     * header promises without pinning the number this file would have to
     * remember. */
    while (eg_post_event(&incoming) == EG_OK) {
        ++accepted;
    }

    assert(accepted > 0);
    assert(eg_post_event(&incoming) == EG_ERR_QUEUE_FULL);
    assert(eg_pending_events() == (size_t)accepted);

    eg_clear_events();
    assert(eg_pending_events() == 0);

    /* Still usable after a full queue — the failure must not wedge it. */
    assert(eg_post_event(&incoming) == EG_OK);
    assert(eg_next_event(&outgoing));
    assert(eg_post_event(NULL) == EG_ERR_INVALID_ARGUMENT);
    assert(!eg_next_event(NULL));
}

static void test_controller_announcement_is_delivered_once(void) {
    assert(!eg_take_gamepad_announcement());

    eg_announce_gamepad();
    assert(eg_take_gamepad_announcement());

    /* Latched, not edge-triggered: a second reader must not see it twice. */
    assert(!eg_take_gamepad_announcement());

    eg_announce_gamepad();
    eg_announce_gamepad();
    assert(eg_take_gamepad_announcement());
    assert(!eg_take_gamepad_announcement());
}

int main(void) {
    test_status_strings_are_stable();
    test_work_is_refused_before_startup();
    test_startup_is_idempotent();
    test_window_flags_round_trip();
    test_queue_keeps_order();
    test_queue_wraps_without_losing_order();
    test_queue_reports_full_instead_of_growing();
    test_controller_announcement_is_delivered_once();

    printf("eclipseglfw: all host tests passed\n");
    return 0;
}
