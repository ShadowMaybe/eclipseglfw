package me.shadow.eclipselauncher.glfw;

import android.view.Surface;

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.FloatBuffer;

/**
 * The launcher side of the window, surface and input bridge.
 *
 * <p>eclipseglfw is the launcher's own window system shim: Minecraft's runtime
 * binds against it to reach an Android surface, and the launcher feeds it the
 * input the player produces. This class is where those two directions meet.
 *
 * <h2>Direction of travel</h2>
 *
 * <ul>
 *   <li><b>Launcher&nbsp;&rarr;&nbsp;native</b> — a surface appears, moves or
 *       goes away, and the player's pointer, keys, text and scroll arrive as
 *       {@code post*} calls. These are the methods the launcher's platform
 *       backend drives.</li>
 *   <li><b>native&nbsp;&rarr;&nbsp;launcher</b> — the handler setters below
 *       register what the launcher wants to hear about: grab state, pointer
 *       position, cursor selection, clipboard and gamepad availability.</li>
 * </ul>
 *
 * <p>The shared {@link #gamepadButtons} and {@link #gamepadAxes} buffers are
 * allocated direct and native-ordered so the controller thread can write them
 * and the launcher can read them without a copy or a lock.
 *
 * <p>The class is the JNI binding surface: {@code JNI_OnLoad} resolves its
 * native methods by name, so renaming it or shrinking it away would fail at
 * game start rather than at build time. The keep rule that prevents that ships
 * in {@code consumer-rules.pro}.
 */
public final class GlfwBridge {

    /**
     * Window flags the launcher sets on the native side. The values are the
     * window attribute codes the runtime understands, not this library's own
     * invention: they are what the other end of the bridge compares against.
     */
    public static final int WINDOW_VISIBLE = 0x00020004;
    public static final int WINDOW_HOVERED = 0x0002000B;

    /** Controller state buffer capacities, in entries. */
    private static final int BUTTON_CAPACITY = GlfwGamepadLayout.BUTTON_COUNT;
    private static final int AXIS_CAPACITY = GlfwGamepadLayout.AXIS_COUNT;

    /* ------------------------------------------------------------------
     * Handler interfaces — what the launcher wants to be told about.
     * ------------------------------------------------------------------ */

    /** Pointer grab changed: the game took or released the mouse. */
    public interface GrabHandler {
        void onGrabChanged(boolean grabbed);
    }

    /** The runtime moved the pointer; both values are in window coordinates. */
    public interface PositionHandler {
        void onCursorPosition(double x, double y);
    }

    /** The runtime selected a cursor image. A {@code null} means the default. */
    public interface CursorHandler {
        void onCursorSelected(GlfwCursor cursor);
    }

    /** The native side finished coming up and can accept work. */
    public interface ReadyHandler {
        void onBridgeReady();
    }

    /**
     * Clipboard access. Deliberately the same two method names the launcher's
     * clipboard already answers to, so one class can serve this bridge and the
     * SDL bridge without declaring the same pair twice.
     */
    public interface ClipboardHandler {
        String getClipboardString();

        void setClipboardString(String text);
    }

    /** The runtime asked for exclusive use of a connected controller. */
    public interface GamepadHandler {
        void onGamepadAvailable();
    }

    /* ------------------------------------------------------------------
     * Shared state.
     * ------------------------------------------------------------------ */

    /**
     * Controller buttons, {@link GlfwGamepadLayout#BUTTON_COUNT} bytes,
     * written by the native controller thread and read by the launcher.
     */
    public static ByteBuffer gamepadButtons;

    /**
     * Controller axes, {@link GlfwGamepadLayout#AXIS_COUNT} floats, in the
     * same direction as {@link #gamepadButtons}.
     */
    public static FloatBuffer gamepadAxes;

    private static GrabHandler sGrabHandler;
    private static PositionHandler sPositionHandler;
    private static CursorHandler sCursorHandler;
    private static ReadyHandler sReadyHandler;
    private static ClipboardHandler sClipboardHandler;
    private static GamepadHandler sGamepadHandler;

    static {
        System.loadLibrary("eclipseglfw");

        gamepadButtons = ByteBuffer
                .allocateDirect(BUTTON_CAPACITY)
                .order(ByteOrder.nativeOrder());
        gamepadAxes = ByteBuffer
                .allocateDirect(AXIS_CAPACITY * 4)
                .order(ByteOrder.nativeOrder())
                .asFloatBuffer();

        nativeStartup();
    }

    private GlfwBridge() {
    }

    /* ------------------------------------------------------------------
     * Launcher → native: surfaces.
     * ------------------------------------------------------------------ */

    /**
     * Hand the native side a surface to draw into. The launcher calls this
     * when its surface view is created; the native side takes its own
     * reference, so the caller keeps ownership of its own.
     *
     * @param surface the surface backing the game view; must not be null
     */
    public static native void bindSurface(Surface surface);

    /**
     * Give up the surface. Safe to call when nothing is bound — the launcher's
     * teardown path can run twice, and a second release must not double-free.
     */
    public static native void releaseSurface();

    /**
     * Report that a frame has been produced. Kept separate from
     * {@link #bindSurface} because the surface outlives any number of frames.
     */
    public static native void presentFrame();

    /* ------------------------------------------------------------------
     * Launcher → native: input.
     * ------------------------------------------------------------------ */

    /**
     * Post an absolute pointer position, in window coordinates.
     *
     * @param x horizontal position
     * @param y vertical position
     */
    public static native void postCursorPosition(double x, double y);

    /**
     * Post a pointer button transition.
     *
     * @param button    button index, see the runtime's mouse button numbering
     * @param action    1 for press, 0 for release
     * @param modifiers modifier bit set held at the time
     */
    public static native void postPointerButton(int button, int action, int modifiers);

    /**
     * Post a key transition, in the numbering the platform's input layer uses.
     *
     * <p>The event is recorded exactly as the framework reported it. Turning
     * that into the numbering the game's runtime expects belongs to the
     * polling layer that dispatches events onward — doing it here would put a
     * table of translations between the two halves of the bridge, where
     * neither half could check it against the other.</p>
     *
     * @param androidKeyCode the platform keycode that changed
     * @param action         1 for press, 0 for release, 2 for repeat
     * @param modifiers      modifier bit set held at the time
     * @param fallbackChar   character the key produces, or {@code 0} if none
     * @return whether the transition was accepted and queued
     */
    public static native boolean postAndroidKey(int androidKeyCode, int action,
                                                int modifiers, char fallbackChar);

    /**
     * Post a run of text the player typed. Each code point in {@code text}
     * becomes its own character event, so composed input arrives in order.
     *
     * @param text      the text to post; null or empty is ignored
     * @param modifiers modifier bit set held at the time
     */
    public static native void postText(String text, int modifiers);

    /**
     * Post a scroll step.
     *
     * @param deltaX horizontal scroll, positive away from the player
     * @param deltaY vertical scroll, positive away from the player
     */
    public static native void postScroll(double deltaX, double deltaY);

    /* ------------------------------------------------------------------
     * Launcher → native: window flags and controllers.
     * ------------------------------------------------------------------ */

    /**
     * Set a window attribute the launcher owns the answer to, such as
     * {@link #WINDOW_VISIBLE} or {@link #WINDOW_HOVERED}.
     *
     * @param flag    attribute code
     * @param enabled the new value
     */
    public static native void setWindowFlag(int flag, boolean enabled);

    /** Announce that a controller appeared, so the runtime can claim it. */
    public static native void announceGamepad();

    /* ------------------------------------------------------------------
     * native → launcher: handler registration.
     * ------------------------------------------------------------------ */

    public static void setGrabHandler(GrabHandler handler) {
        sGrabHandler = handler;
    }

    public static void setPositionHandler(PositionHandler handler) {
        sPositionHandler = handler;
    }

    public static void setCursorHandler(CursorHandler handler) {
        sCursorHandler = handler;
    }

    public static void setReadyHandler(ReadyHandler handler) {
        sReadyHandler = handler;
    }

    public static void setClipboard(ClipboardHandler handler) {
        sClipboardHandler = handler;
    }

    public static void setGamepadHandler(GamepadHandler handler) {
        sGamepadHandler = handler;
    }

    /* ------------------------------------------------------------------
     * Entry points the native side calls back through. Package-private on
     * purpose: nothing outside this bridge and its native peer should be
     * synthesising these.
     * ------------------------------------------------------------------ */

    static void notifyGrabChanged(boolean grabbed) {
        GrabHandler handler = sGrabHandler;
        if (handler != null) {
            handler.onGrabChanged(grabbed);
        }
    }

    static void notifyCursorPosition(double x, double y) {
        PositionHandler handler = sPositionHandler;
        if (handler != null) {
            handler.onCursorPosition(x, y);
        }
    }

    static void notifyCursorSelected(GlfwCursor cursor) {
        CursorHandler handler = sCursorHandler;
        if (handler != null) {
            handler.onCursorSelected(cursor);
        }
    }

    static void notifyReady() {
        ReadyHandler handler = sReadyHandler;
        if (handler != null) {
            handler.onBridgeReady();
        }
    }

    static void notifyGamepadAvailable() {
        GamepadHandler handler = sGamepadHandler;
        if (handler != null) {
            handler.onGamepadAvailable();
        }
    }

    /**
     * Read the clipboard for the native side.
     *
     * @return the clipboard text, or null when no handler is registered
     */
    static String readClipboard() {
        ClipboardHandler handler = sClipboardHandler;
        return handler == null ? null : handler.getClipboardString();
    }

    /**
     * Hand clipboard text to the launcher.
     *
     * @param text the text the runtime put on the clipboard; may be null
     */
    static void writeClipboard(String text) {
        ClipboardHandler handler = sClipboardHandler;
        if (handler != null) {
            handler.setClipboardString(text == null ? "" : text);
        }
    }

    /** Starts the native side. Only called from this class's initializer. */
    private static native void nativeStartup();
}
