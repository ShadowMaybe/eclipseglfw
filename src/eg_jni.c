/*
 * eclipseglfw — the JNI binding.
 *
 * Copyright (c) 2026 Shadow.
 * SPDX-License-Identifier: MIT
 *
 * Native methods are bound by name through RegisterNatives rather than by
 * exported symbol. The difference matters at three in the morning: with
 * exported symbols a signature typo is an UnsatisfiedLinkError thrown from
 * somewhere in the input path long after the library loaded, and with
 * RegisterNatives it is one line in logcat during JNI_OnLoad that names the
 * class and says binding failed.
 *
 * Everything here is one direction — Java calling in. The other direction,
 * this library calling back into the launcher's handlers, arrives with the
 * windowing layer that has something to report; there is no half of the
 * bridge here that would sit waiting for it.
 */

#include <jni.h>

#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

#include "eclipseglfw.h"

#define ECLIPSE_LOG_TAG "EclipseGLFW"

#define ECLIPSE_LOGE(...) \
    __android_log_print(ANDROID_LOG_ERROR, ECLIPSE_LOG_TAG, __VA_ARGS__)

/* The class whose native methods we bind, and the reason consumer-rules.pro
 * keeps that class from being renamed out from under us. */
#define ECLIPSE_BRIDGE_CLASS "me/shadow/eclipselauncher/glfw/GlfwBridge"

/* ------------------------------------------------------------------
 * Java → native.
 * ------------------------------------------------------------------ */

static void bridge_startup(JNIEnv *env, jclass clazz) {
    const EgStatus status = eg_startup();

    (void)env;
    (void)clazz;

    if (status != EG_OK) {
        ECLIPSE_LOGE("eclipseglfw failed to start: %s", eg_status_string(status));
    }
}

/*
 * Borrowed from the Surface, not the window: ANativeWindow_fromSurface hands
 * back a reference we own, eg_bind_surface takes its own, and the one we were
 * given goes back immediately. Getting this wrong leaks a buffer queue on
 * every surface recreation, which on Android is every rotation.
 */
static void bridge_bind_surface(JNIEnv *env, jclass clazz, jobject surface) {
    struct ANativeWindow *window;

    (void)clazz;

    if (surface == NULL) {
        ECLIPSE_LOGE("bindSurface received no surface");
        return;
    }

    window = ANativeWindow_fromSurface(env, surface);
    if (window == NULL) {
        ECLIPSE_LOGE("the framework refused to give us a window");
        return;
    }

    if (eg_bind_surface(window) != EG_OK) {
        ECLIPSE_LOGE("the surface could not be bound");
    }

    ANativeWindow_release(window);
}

static void bridge_release_surface(JNIEnv *env, jclass clazz) {
    (void)env;
    (void)clazz;

    /* Always succeeds by design: teardown may legitimately run twice. */
    eg_unbind_surface();
}

/*
 * A present with no surface is the normal state during teardown, not a
 * fault worth logging — the launcher keeps pumping frames while it winds the
 * game down. Anything else is.
 */
static void bridge_present_frame(JNIEnv *env, jclass clazz) {
    const EgStatus status = eg_present();

    (void)env;
    (void)clazz;

    if (status != EG_OK && status != EG_ERR_NO_SURFACE) {
        ECLIPSE_LOGE("presenting a frame failed: %s", eg_status_string(status));
    }
}

static void bridge_post_cursor_position(JNIEnv *env, jclass clazz,
                                        jdouble x, jdouble y) {
    EgEvent event;

    (void)env;
    (void)clazz;

    event.kind = EG_EVENT_CURSOR;
    event.code = 0;
    event.action = 0;
    event.modifiers = 0;
    event.x = x;
    event.y = y;
    event.character = 0;

    eg_post_event(&event);
}

static void bridge_post_pointer_button(JNIEnv *env, jclass clazz, jint button,
                                       jint action, jint modifiers) {
    EgEvent event;

    (void)env;
    (void)clazz;

    event.kind = EG_EVENT_BUTTON;
    event.code = button;
    event.action = action;
    event.modifiers = modifiers;
    event.x = 0;
    event.y = 0;
    event.character = 0;

    eg_post_event(&event);
}

/*
 * Posted in the platform's own numbering, exactly as the caller reported it.
 * Translating it to the game's numbering belongs to the polling layer, so
 * that the table of translations sits next to the code reading its output
 * rather than in between the two halves of the bridge.
 */
static jboolean bridge_post_android_key(JNIEnv *env, jclass clazz,
                                        jint platform_key, jint action,
                                        jint modifiers, jchar fallback_char) {
    EgEvent event;

    (void)env;
    (void)clazz;

    event.kind = EG_EVENT_KEY;
    event.code = platform_key;
    event.action = action;
    event.modifiers = modifiers;
    event.x = 0;
    event.y = 0;
    event.character = (uint32_t)fallback_char;

    return eg_post_event(&event) == EG_OK ? JNI_TRUE : JNI_FALSE;
}

/*
 * The runtime takes text, not events, so each code point becomes its own
 * event. Java strings are UTF-16, which means the pair of a surrogate
 * sequence has to be folded back into the single code point the game
 * expects — posting the halves separately would show up as two replacement
 * characters in a chat window and nowhere else, which is the worst kind of
 * bug to find late.
 */
static void bridge_post_text(JNIEnv *env, jclass clazz, jstring text,
                             jint modifiers) {
    const jchar *units;
    jsize length;
    jsize cursor;

    (void)clazz;

    if (text == NULL) {
        return;
    }

    length = (*env)->GetStringLength(env, text);
    if (length <= 0) {
        return;
    }

    units = (*env)->GetStringChars(env, text, NULL);
    if (units == NULL) {
        /* The JVM has already thrown OutOfMemoryError; nothing to add. */
        return;
    }

    for (cursor = 0; cursor < length; ++cursor) {
        uint32_t codepoint = (uint32_t)units[cursor];
        EgEvent event;

        if (codepoint >= 0xD800u && codepoint <= 0xDBFFu &&
            (cursor + 1) < length) {
            const uint32_t low = (uint32_t)units[cursor + 1];
            if (low >= 0xDC00u && low <= 0xDFFFu) {
                codepoint = 0x10000u + ((codepoint - 0xD800u) << 10) +
                            (low - 0xDC00u);
                ++cursor;
            }
        }

        event.kind = EG_EVENT_TEXT;
        event.code = 0;
        event.action = 0;
        event.modifiers = modifiers;
        event.x = 0;
        event.y = 0;
        event.character = codepoint;

        /* Stop at the first refusal rather than hammering a queue that is
         * already full; the tail of a dropped burst will not be missed. */
        if (eg_post_event(&event) != EG_OK) {
            break;
        }
    }

    (*env)->ReleaseStringChars(env, text, units);
}

static void bridge_post_scroll(JNIEnv *env, jclass clazz, jdouble delta_x,
                               jdouble delta_y) {
    EgEvent event;

    (void)env;
    (void)clazz;

    event.kind = EG_EVENT_SCROLL;
    event.code = 0;
    event.action = 0;
    event.modifiers = 0;
    event.x = delta_x;
    event.y = delta_y;
    event.character = 0;

    eg_post_event(&event);
}

static void bridge_set_window_flag(JNIEnv *env, jclass clazz, jint flag,
                                   jboolean enabled) {
    const EgStatus status =
        eg_set_window_flag((int32_t)flag, enabled == JNI_TRUE);

    (void)env;
    (void)clazz;

    if (status != EG_OK) {
        ECLIPSE_LOGE("window flag 0x%x refused: %s", (unsigned int)flag,
                     eg_status_string(status));
    }
}

static void bridge_announce_gamepad(JNIEnv *env, jclass clazz) {
    (void)env;
    (void)clazz;

    eg_announce_gamepad();
}

/* ------------------------------------------------------------------
 * Binding table and library entry points.
 * ------------------------------------------------------------------ */

static const JNINativeMethod kBridgeMethods[] = {
    {"nativeStartup", "()V", (void *)bridge_startup},
    {"bindSurface", "(Landroid/view/Surface;)V", (void *)bridge_bind_surface},
    {"releaseSurface", "()V", (void *)bridge_release_surface},
    {"presentFrame", "()V", (void *)bridge_present_frame},
    {"postCursorPosition", "(DD)V", (void *)bridge_post_cursor_position},
    {"postPointerButton", "(III)V", (void *)bridge_post_pointer_button},
    {"postAndroidKey", "(IIIC)Z", (void *)bridge_post_android_key},
    {"postText", "(Ljava/lang/String;I)V", (void *)bridge_post_text},
    {"postScroll", "(DD)V", (void *)bridge_post_scroll},
    {"setWindowFlag", "(IZ)V", (void *)bridge_set_window_flag},
    {"announceGamepad", "()V", (void *)bridge_announce_gamepad},
};

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env = NULL;
    jclass bridge;
    jint bound;

    (void)reserved;

    if ((*vm)->GetEnv(vm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
        ECLIPSE_LOGE("eclipseglfw could not obtain a JNI environment");
        return JNI_ERR;
    }

    /* FindClass from JNI_OnLoad runs on the thread that called
     * System.loadLibrary, so the application class loader is the one in
     * context and the bridge is findable by its full name. */
    bridge = (*env)->FindClass(env, ECLIPSE_BRIDGE_CLASS);
    if (bridge == NULL) {
        ECLIPSE_LOGE("eclipseglfw could not find %s", ECLIPSE_BRIDGE_CLASS);
        return JNI_ERR;
    }

    bound = (*env)->RegisterNatives(
        env, bridge, kBridgeMethods,
        (jint)(sizeof(kBridgeMethods) / sizeof(kBridgeMethods[0])));

    if (bound != 0) {
        ECLIPSE_LOGE("eclipseglfw could not bind %d native method(s); check "
                     "the signatures against GlfwBridge",
                     (int)(sizeof(kBridgeMethods) / sizeof(kBridgeMethods[0])));
        (*env)->DeleteLocalRef(env, bridge);
        return JNI_ERR;
    }

    (*env)->DeleteLocalRef(env, bridge);

    return JNI_VERSION_1_6;
}

/*
 * The library is not unloaded while anything can still use it, but a surface
 * held at this point would never be released by anyone else. Letting go here
 * costs one call and closes the only path where the reference count could
 * outlive the process's interest in it.
 */
JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved) {
    (void)vm;
    (void)reserved;

    eg_unbind_surface();
}
