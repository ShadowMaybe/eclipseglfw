#!/bin/sh
#
# eclipseglfw — build and run the host unit tests.
#
# Needs nothing but a C compiler: no Android SDK, no emulator, no device. The
# Android-only files (the JNI binding and the surface holder) are covered by
# the real NDK build, which this repository's CI runs as a separate job, so a
# green run here says nothing it has no business claiming.
#
# Usage: tests/run_tests.sh
set -eu

ROOT=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
OUT="$ROOT/tests/build"
CC=${CC:-cc}

mkdir -p "$OUT"

# -pthread in the flags rather than -lpthread at the end: the queue and the
# state lock call the real pthread functions, and on some libcs those are
# simply not present unless the compiler is told the program is threaded.
#
# gnu11 rather than c11. Strict ISO mode hides POSIX behind __STRICT_ANSI__,
# and pthread.h stops declaring half of itself there — the tests would fail to
# build for a reason that has nothing to do with what they test. The NDK build
# is configured the same way so the two compile the same source.
CFLAGS="-std=gnu11 -Wall -Wextra -Wshadow -Werror -pthread"

# shellcheck disable=SC2086  # $CFLAGS holds a list of flags and must word-split
"$CC" $CFLAGS -I"$ROOT/include" -o "$OUT/test_eclipseglfw" \
    "$ROOT/tests/test_eclipseglfw.c" \
    "$ROOT/src/eg_state.c" \
    "$ROOT/src/eg_events.c"

"$OUT/test_eclipseglfw"
