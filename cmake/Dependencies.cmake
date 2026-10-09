if(DEFINED ENV{LOCAL_PREFIX})
    set(_WMA_SEARCH_PREFIX "$ENV{LOCAL_PREFIX}")
else()
    set(_WMA_SEARCH_PREFIX "/usr/local")
endif()

if(ANDROID)
    set(_WMA_PLATFORM "android")
elseif(EMSCRIPTEN)
    set(_WMA_PLATFORM "wasm")
elseif(APPLE)
    # Mirrors the linux branch below, for this project's own
    # macos-debug/macos-release/ios presets. iOS gets its own prefix rather than
    # sharing macOS's: an ink installed there is cross-compiled for
    # arm64-apple-ios and is not linkable into a macOS build (or vice versa), so
    # one prefix for both would silently offer the wrong slice.
    if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
        set(_WMA_PLATFORM "ios")
    elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(_WMA_PLATFORM "macos/debug")
    else()
        set(_WMA_PLATFORM "macos/release")
    endif()
else()
    # libink's linux presets install to linux/debug or linux/release
    # (single-config Ninja generator, so CMAKE_BUILD_TYPE is known here).
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(_WMA_PLATFORM "linux/debug")
    else()
        set(_WMA_PLATFORM "linux/release")
    endif()
endif()

# A per-platform install is only a default: an explicit -Dink_DIR wins, and a
# missing directory falls through to the normal package search.
set(_WMA_INK_DIR "${_WMA_SEARCH_PREFIX}/${_WMA_PLATFORM}/lib/cmake/ink")
if(NOT ink_DIR AND EXISTS "${_WMA_INK_DIR}")
    set(ink_DIR "${_WMA_INK_DIR}" CACHE PATH "Directory containing inkConfig.cmake")
endif()

unset(_WMA_INK_DIR)
unset(_WMA_SEARCH_PREFIX)
unset(_WMA_PLATFORM)

find_package(ink REQUIRED CONFIG)

# Windowing backend options. Defaults are OFF so a build explicitly opts into
# what it needs; Platform.cmake forces SDL3 ON for Android/WASM.
option(WMA_ENABLE_SDL     "Enable SDL3 backend"    OFF)
option(WMA_ENABLE_GLFW    "Enable GLFW backend"    OFF)
option(WMA_ENABLE_X11     "Enable X11 backend"     OFF)
option(WMA_ENABLE_WAYLAND "Enable Wayland backend" OFF)

# Audio backend options. A separate axis from the windowing backends above:
# GLFW/X11/Wayland have no audio API, and SDL3's audio and windowing halves are
# independently useful, so a build picks one from each list. There is no option
# for the null audio device -- it depends on nothing and is always compiled, so
# that a build with no audio backend at all still links and simply plays
# silence (unlike the windowing case, which can only throw).
option(WMA_ENABLE_ALSA "Enable ALSA native audio backend" OFF)

# The native desktop backends do not exist on Android/WASM; only SDL3 survives.
# ALSA is in this list for audio: Android routes through AAudio/OpenSL ES and
# the web through Web Audio, both of which SDL3 already covers.
if(ANDROID OR EMSCRIPTEN)
    foreach(_backend GLFW X11 WAYLAND ALSA)
        if(WMA_ENABLE_${_backend})
            message(WARNING
                "[wma] WMA_ENABLE_${_backend} is not supported on "
                "${CMAKE_SYSTEM_NAME} — forcing OFF (use SDL3)"
            )
        endif()
        set(WMA_ENABLE_${_backend} OFF CACHE BOOL "" FORCE)
    endforeach()
endif()

# X11/Wayland are Linux windowing protocols and ALSA is the Linux audio stack;
# on Windows only SDL3/GLFW exist, and audio goes through SDL3 (WASAPI
# underneath).
if(WIN32 AND NOT EMSCRIPTEN)
    foreach(_backend X11 WAYLAND ALSA)
        if(WMA_ENABLE_${_backend})
            message(WARNING
                "[wma] WMA_ENABLE_${_backend} is not supported on Windows — "
                "forcing OFF (use SDL3/GLFW)"
            )
        endif()
        set(WMA_ENABLE_${_backend} OFF CACHE BOOL "" FORCE)
    endforeach()
endif()

# Apple platforms: audio is CoreAudio, reached through SDL3. libasound has no
# port there at all.
if(APPLE)
    if(WMA_ENABLE_ALSA)
        message(WARNING
            "[wma] WMA_ENABLE_ALSA is not supported on Apple platforms — "
            "forcing OFF (use SDL3, which routes to CoreAudio)"
        )
    endif()
    set(WMA_ENABLE_ALSA OFF CACHE BOOL "" FORCE)
endif()

if(NOT WMA_ENABLE_SDL AND NOT WMA_ENABLE_GLFW AND NOT WMA_ENABLE_X11 AND NOT WMA_ENABLE_WAYLAND)
    message(WARNING
        "[wma] No backend enabled — the library will build but createWindowManager() "
        "will always throw. Enable at least one of WMA_ENABLE_SDL/GLFW/X11/WAYLAND."
    )
endif()

if(WMA_ENABLE_SDL)
    find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3)
endif()

if(WMA_ENABLE_GLFW)
    find_package(glfw3 REQUIRED)
endif()

# Static SDL3 and static GLFW both compile in the Wayland protocol tables
# (wp_fractional_scale_manager_v1_interface, ...), so an executable that pulls in
# both fails to link with duplicate symbols. The two are wayland-scanner output
# for the same protocol version and so identical in practice -- WMA_NEEDS_MULTIDEF_FIX
# tells CMakeLists.txt (where SDL3/glfw are actually linked to the wma target) to
# let the linker pick either copy instead of failing.
set(WMA_NEEDS_MULTIDEF_FIX FALSE)
if(WMA_ENABLE_SDL AND WMA_ENABLE_GLFW AND NOT TARGET SDL3::SDL3-shared AND TARGET glfw)
    get_target_property(_wma_glfw_type glfw TYPE)
    if(_wma_glfw_type STREQUAL "STATIC_LIBRARY" AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(WMA_NEEDS_MULTIDEF_FIX TRUE)
        message(STATUS
            "[wma] SDL3 and GLFW are both static: allowing the linker to pick either copy of "
            "the duplicate wp_*_interface Wayland protocol tables they both embed.")
    endif()
    unset(_wma_glfw_type)
endif()

# libasound ships a pkg-config file but no CMake config package, so this goes
# through PkgConfig the same way the Wayland client libraries below do.
if(WMA_ENABLE_ALSA)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(ALSA REQUIRED IMPORTED_TARGET alsa)
endif()

set(WMA_ENABLE_X11_GL     FALSE)
set(WMA_ENABLE_WAYLAND_GL FALSE)

if(WMA_ENABLE_X11)
    find_package(X11 REQUIRED)
    # Optional: GLX for OpenGL-on-X11. Provides the imported target OpenGL::GLX.
    find_package(OpenGL COMPONENTS OpenGL GLX)
    if(TARGET OpenGL::GLX)
        set(WMA_ENABLE_X11_GL TRUE)
    else()
        message(STATUS "[wma] GLX not found — OpenGL on the X11 backend is disabled.")
    endif()
endif()

if(WMA_ENABLE_WAYLAND)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WAYLAND REQUIRED IMPORTED_TARGET
        wayland-client
        wayland-cursor
        xkbcommon
    )

    # Optional: EGL + wayland-egl for OpenGL-on-Wayland.
    pkg_check_modules(WAYLAND_EGL IMPORTED_TARGET egl wayland-egl)
    if(TARGET PkgConfig::WAYLAND_EGL)
        set(WMA_ENABLE_WAYLAND_GL TRUE)
    else()
        message(STATUS "[wma] EGL/wayland-egl not found — OpenGL on the Wayland backend is disabled.")
    endif()

    # libdecor itself is GLFW's own runtime dlopen() dependency, not wma's -- this
    # only resolves the directory its plugin would live in, so GlfwNative.cpp can
    # tell "no plugin installed" (silently draws nothing) apart from "compositor
    # draws the frame itself" (this hint is irrelevant either way).
    if(WMA_ENABLE_GLFW)
        pkg_check_modules(LIBDECOR QUIET libdecor-0)
        if(LIBDECOR_FOUND)
            pkg_get_variable(WMA_LIBDECOR_LIBDIR libdecor-0 libdir)
            set(WMA_LIBDECOR_PLUGIN_DIR "${WMA_LIBDECOR_LIBDIR}/libdecor/plugins-1")
        endif()
    endif()

    find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)
    pkg_get_variable(WAYLAND_PROTOCOLS_DATADIR wayland-protocols pkgdatadir)

    set(WMA_WAYLAND_PROTO_DIR
        "${CMAKE_BINARY_DIR}/include/wma/backends/wayland/protocols"
        CACHE INTERNAL ""
    )
    file(MAKE_DIRECTORY "${WMA_WAYLAND_PROTO_DIR}")

    set(WMA_WAYLAND_GENERATED_SOURCES "" CACHE INTERNAL "")

    # Runs a wayland-scanner subcommand, aborting the configure on failure.
    function(wma_run_wayland_scanner mode xml_relpath out_name)
        execute_process(
            COMMAND "${WAYLAND_SCANNER}" "${mode}"
                "${WAYLAND_PROTOCOLS_DATADIR}/${xml_relpath}"
                "${WMA_WAYLAND_PROTO_DIR}/${out_name}"
            RESULT_VARIABLE _wl_scanner_result
        )
        if(_wl_scanner_result)
            message(FATAL_ERROR
                "[wma] wayland-scanner failed to generate ${out_name} "
                "(exit code: ${_wl_scanner_result})"
            )
        endif()
    endfunction()

    # Generates both the client-header (declarations, e.g. `xdg_wm_base_interface`)
    # and the private-code (the .c that actually *defines* those wl_interface
    # symbols) for <xml_relpath>, relative to the wayland-protocols pkgdatadir.
    # Without the private-code half, the Wayland backend fails to link on its own
    # — it previously only worked when built alongside SDL3, whose static lib
    # happens to vendor the same symbols.
    function(wma_generate_wayland_protocol xml_relpath header_name source_name)
        wma_run_wayland_scanner(client-header "${xml_relpath}" "${header_name}")
        wma_run_wayland_scanner(private-code  "${xml_relpath}" "${source_name}")
        set(WMA_WAYLAND_GENERATED_SOURCES
            ${WMA_WAYLAND_GENERATED_SOURCES} "${WMA_WAYLAND_PROTO_DIR}/${source_name}"
            CACHE INTERNAL ""
        )
    endfunction()

    wma_generate_wayland_protocol(
        stable/xdg-shell/xdg-shell.xml
        xdg-shell-client-protocol.h
        xdg-shell-protocol.c
    )
    wma_generate_wayland_protocol(
        unstable/xdg-decoration/xdg-decoration-unstable-v1.xml
        xdg-decoration-unstable-v1-client-protocol.h
        xdg-decoration-unstable-v1-protocol.c
    )
endif()
