package me.shadow.eclipselauncher.glfw;

import android.graphics.Bitmap;

/**
 * A cursor image the game asked for, kept as the pieces a bitmap cursor needs
 * to be drawn: the pixels, and the point within them the pointer tracks from.
 *
 * Kept deliberately dumb. Whether the image is ever blitted, and what happens
 * when it is not, belongs to whoever installs a {@link GlfwBridge.CursorHandler}
 * — this class only carries the data across the native boundary.
 */
public final class GlfwCursor {

    private final Bitmap bitmap;
    private final int hotspotX;
    private final int hotspotY;

    /**
     * @param bitmap   the cursor image, owned by the caller after construction
     * @param hotspotX horizontal offset of the tracking point, in pixels
     * @param hotspotY vertical offset of the tracking point, in pixels
     */
    public GlfwCursor(Bitmap bitmap, int hotspotX, int hotspotY) {
        if (bitmap == null) {
            throw new IllegalArgumentException("cursor bitmap must not be null");
        }
        this.bitmap = bitmap;
        this.hotspotX = hotspotX;
        this.hotspotY = hotspotY;
    }

    public Bitmap getBitmap() {
        return bitmap;
    }

    public int getHotspotX() {
        return hotspotX;
    }

    public int getHotspotY() {
        return hotspotY;
    }

    @Override
    public String toString() {
        return "GlfwCursor{" + bitmap.getWidth() + "x" + bitmap.getHeight()
                + " at " + hotspotX + "," + hotspotY + "}";
    }
}
