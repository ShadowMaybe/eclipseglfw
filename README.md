# eclipseglfw

The Eclipse launcher's own window, surface and input shim — the layer between
an Android `SurfaceView` and the windowing entry points Minecraft's runtime
binds against.

It is one of three components written for [Eclipse Launcher][launcher], the
others being [eclipseexec][exec] (native graphics bootstrap) and
[eclipsesdl][sdl].

[eclipse]: #eclipseglfw
[launcher]: https://github.com/ShadowMaybe/EclipseLauncher
[exec]: https://github.com/ShadowMaybe/eclipseexec
[sdl]: https://github.com/ShadowMaybe/eclipsesdl

## What it is for

Android has one window, a `SurfaceView`, and input events arriving on the UI
thread. A desktop window system has windows that are created, resized, focused
and iconified; input arriving as callbacks from a polling event loop; a
clipboard; a cursor with images and hotspots. The launcher sits in the middle
of that gap, and this library is what fills it.

It has two halves, and only one exists so far:

**The launcher half — built.** `me.shadow.eclipselauncher.glfw.GlfwBridge` is
where the two directions meet. The launcher hands over surfaces and posts the
player's pointer, key, text and scroll input; the class registers handlers for
grab state, pointer position, cursor selection, clipboard and controller
availability, and shares two direct buffers the controller thread writes into.

**The runtime half — not yet.** The windowing entry points the game's runtime
resolves (`glfwInit`, `glfwPollEvents`, and the rest) are the next layer, built
on top of what is here. They are held in their own header rather than this one
so the launcher-facing contract stays readable as what it is.

The surface between them is [include/eclipseglfw.h](include/eclipseglfw.h).
It states plainly what each half does and what it deliberately does not yet
do — the frame path counts frames rather than swapping them, for instance,
because the renderer binding that turns the count into a swap is the runtime
half's job.

## Layout

| Path | What it holds |
| --- | --- |
| `include/eclipseglfw.h` | The C contract: lifecycle, surfaces, flags, event queue |
| `src/eg_state.c` | Startup, window flags, the latched controller announcement |
| `src/eg_events.c` | The fixed-size input ring buffer |
| `src/eg_surface.c` | `ANativeWindow` ownership and the frame counter |
| `src/eg_jni.c` | `RegisterNatives` binding and the Java-facing entry points |
| `jni_bindings/` | The Android library module carrying the classes and the `.so` |
| `tests/` | Host tests for everything that needs no device |

## Building

Host tests need only a C compiler:

```sh
sh tests/run_tests.sh
```

The library itself builds through Gradle and the NDK, and CI is what
compiles it:

```sh
./gradlew assembleRelease
```

C is compiled with `-Wall -Wextra -Wshadow -Werror`. That is not a preference
list: this runs on the input path, where a warning is more likely to be a real
bug than pedantry, and a library this size is small enough that none of them
should be deferred.

## Why `RegisterNatives`

The native methods are bound by name from `JNI_OnLoad` rather than by exported
symbol. With exported symbols a signature typo surfaces as an
`UnsatisfiedLinkError` somewhere in the input path long after the library
loaded; with `RegisterNatives` it is one line in logcat during load that names
the class and says binding failed. The consequence is that `GlfwBridge` cannot
be renamed or shrunk out from under that binding — hence the keep rule in
`consumer-rules.pro`, which travels with the AAR to whoever includes it.

## Licence

MIT — see [LICENSE](LICENSE).

Copyright (c) 2026 Shadow.
