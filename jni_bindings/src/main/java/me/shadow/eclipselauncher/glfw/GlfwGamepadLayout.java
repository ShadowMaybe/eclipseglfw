package me.shadow.eclipselauncher.glfw;

/**
 * Controller button and axis positions as the game expects to read them out of
 * {@link GlfwBridge#gamepadButtons} and {@link GlfwBridge#gamepadAxes}.
 *
 * The numbering follows the standard gamepad layout the runtime already
 * produces, so a mapping layer upstream stays the only thing that knows how a
 * particular controller is wired.
 */
public final class GlfwGamepadLayout {

    private GlfwGamepadLayout() {
    }

    /* Buttons. */
    public static final int BUTTON_A = 0;
    public static final int BUTTON_B = 1;
    public static final int BUTTON_X = 2;
    public static final int BUTTON_Y = 3;
    public static final int BUTTON_LEFT_BUMPER = 4;
    public static final int BUTTON_RIGHT_BUMPER = 5;
    public static final int BUTTON_BACK = 6;
    public static final int BUTTON_START = 7;
    public static final int BUTTON_GUIDE = 8;
    public static final int BUTTON_LEFT_THUMB = 9;
    public static final int BUTTON_RIGHT_THUMB = 10;
    public static final int BUTTON_DPAD_UP = 11;
    public static final int BUTTON_DPAD_RIGHT = 12;
    public static final int BUTTON_DPAD_DOWN = 13;
    public static final int BUTTON_DPAD_LEFT = 14;

    /** Highest valid button index; the buffer must hold one more than this. */
    public static final int BUTTON_COUNT = 15;

    /* Axes. */
    public static final int AXIS_LEFT_X = 0;
    public static final int AXIS_LEFT_Y = 1;
    public static final int AXIS_RIGHT_X = 2;
    public static final int AXIS_RIGHT_Y = 3;
    public static final int AXIS_LEFT_TRIGGER = 4;
    public static final int AXIS_RIGHT_TRIGGER = 5;

    /** Highest valid axis index; the buffer must hold one more than this. */
    public static final int AXIS_COUNT = 6;

    /** Value of a pressed button in the button buffer. */
    public static final int PRESS = 1;
    /** Value of a released button in the button buffer. */
    public static final int RELEASE = 0;
}
