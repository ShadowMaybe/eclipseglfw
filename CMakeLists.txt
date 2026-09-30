cmake_minimum_required(VERSION 3.22.1)

project(eclipseglfw
        VERSION 0.1.0
        LANGUAGES C
        DESCRIPTION "Eclipse Launcher window and input bridge")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Extensions on deliberately. Strict ISO mode hides POSIX behind __STRICT_ANSI__
# — pthread.h stops declaring half of itself there, and the Android headers
# assume they are allowed to offer what they offer. gnu11 keeps the device and
# host builds compiling the same source against the same declarations, and the
# host test script uses the same mode for the same reason.
set(CMAKE_C_EXTENSIONS ON)

# Warnings are errors. This is a library small enough to hold in one head and
# it runs on the input path, where a warning is much more likely to be a real
# bug than pedantry.
add_compile_options(-Wall -Wextra -Wshadow -Werror)

set(ECLIPSEGLFW_SOURCES
        src/eg_state.c
        src/eg_events.c
        src/eg_surface.c
        src/eg_jni.c)

add_library(eclipseglfw SHARED ${ECLIPSEGLFW_SOURCES})

target_include_directories(eclipseglfw
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# log is for the two lines that matter when something refuses to bind, and
# android carries ANativeWindow — the surface half has no way to exist without
# it. Nothing else is linked: this library takes what the launcher gives it
# and does not reach out of the process.
find_library(log-lib log)
find_library(android-lib android)

target_link_libraries(eclipseglfw
        PRIVATE ${log-lib} ${android-lib})
