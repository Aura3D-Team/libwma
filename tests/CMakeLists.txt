# Deliberately framework-free: these assert on pure functions, and a
# dependency-free test binary keeps `ctest` runnable in the same containers
# that build the library, with nothing to fetch or vendor.
add_executable(wma_test_utf8 test_utf8.cpp)
target_link_libraries(wma_test_utf8 PRIVATE wma)
add_test(NAME wma_test_utf8 COMMAND wma_test_utf8)

# SDL's dummy video driver stands in for a display, so this runs headless.
add_executable(wma_test_content_scale test_content_scale.cpp)
target_link_libraries(wma_test_content_scale PRIVATE wma)
if(WMA_ENABLE_SDL)
    target_compile_definitions(wma_test_content_scale PRIVATE WMA_TEST_HAS_SDL)
endif()
add_test(NAME wma_test_content_scale COMMAND wma_test_content_scale)
set_tests_properties(wma_test_content_scale PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77)

# The clipboard on the backends that delegate it to the platform. SDL's dummy driver needs no display.
if(WMA_ENABLE_SDL)
    add_executable(wma_test_sdl_clipboard test_backend_clipboard.cpp)
    target_link_libraries(wma_test_sdl_clipboard PRIVATE wma)
    target_compile_definitions(wma_test_sdl_clipboard PRIVATE WMA_TEST_BACKEND_SDL)
    add_test(NAME wma_test_sdl_clipboard COMMAND wma_test_sdl_clipboard)
    set_tests_properties(wma_test_sdl_clipboard PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77)
endif()

if(WMA_ENABLE_SDL)
    add_executable(wma_test_cpu_pacing test_cpu_pacing.cpp)
    target_link_libraries(wma_test_cpu_pacing PRIVATE wma)
    add_test(NAME wma_test_cpu_pacing COMMAND wma_test_cpu_pacing)
    set_tests_properties(wma_test_cpu_pacing PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77)
endif()

# DynLib is private to the library (src/), so the test borrows its include path.
if(WMA_ENABLE_X11 OR WMA_ENABLE_WAYLAND OR WMA_ENABLE_ALSA)
    add_executable(wma_test_dynlib test_dynlib.cpp)
    target_link_libraries(wma_test_dynlib PRIVATE wma)
    target_include_directories(wma_test_dynlib PRIVATE "${PROJECT_SOURCE_DIR}/src")
    add_test(NAME wma_test_dynlib COMMAND wma_test_dynlib)
endif()

# Threaded, unlike the decoder tests: MixCallbackSlot exists to make
# setMixCallback() safe on a running device, and only two threads can show that.
# Worth running under -fsanitize=thread, which is what turns a missed
# synchronization back into a hard failure rather than a rare glitch.
add_executable(wma_test_mix_callback_slot test_mix_callback_slot.cpp)
target_link_libraries(wma_test_mix_callback_slot PRIVATE wma)
add_test(NAME wma_test_mix_callback_slot COMMAND wma_test_mix_callback_slot)

# Needs the Wayland backend's own header, which is deliberately outside
# <wma/wma.hpp>. Compiles a keymap with xkbcommon and drives the listener
# directly, so the translation is testable with no compositor and no seat.
if(WMA_ENABLE_WAYLAND)
    add_executable(wma_test_wayland_keymap test_wayland_keymap.cpp)
    target_link_libraries(wma_test_wayland_keymap PRIVATE wma PkgConfig::WAYLAND)
    add_test(NAME wma_test_wayland_keymap COMMAND wma_test_wayland_keymap)

    # A private compositor over a socket pair checks the actual xdg-shell
    # requests and configure events, without a desktop, GPU, or test framework.
    # Keep the server dependency confined to this optional test target.
    pkg_check_modules(WAYLAND_SERVER QUIET IMPORTED_TARGET wayland-server)
    if(TARGET PkgConfig::WAYLAND_SERVER)
        set(_wma_test_protocol_dir "${CMAKE_CURRENT_BINARY_DIR}/protocols")
        file(MAKE_DIRECTORY "${_wma_test_protocol_dir}")
        foreach(_protocol IN ITEMS "stable/xdg-shell/xdg-shell"
                                   "unstable/xdg-decoration/xdg-decoration-unstable-v1"
                                   "unstable/pointer-constraints/pointer-constraints-unstable-v1"
                                   "unstable/relative-pointer/relative-pointer-unstable-v1")
            get_filename_component(_name "${_protocol}" NAME)
            execute_process(
                COMMAND "${WAYLAND_SCANNER}" server-header
                    "${WAYLAND_PROTOCOLS_DATADIR}/${_protocol}.xml"
                    "${_wma_test_protocol_dir}/${_name}-server-protocol.h"
                COMMAND_ERROR_IS_FATAL ANY
            )
        endforeach()
        find_package(Threads REQUIRED)
        add_executable(wma_test_wayland_window_controls test_wayland_window_controls.cpp wayland_window_move.cpp)
        target_include_directories(wma_test_wayland_window_controls PRIVATE "${_wma_test_protocol_dir}")
        target_link_libraries(wma_test_wayland_window_controls PRIVATE
            wma PkgConfig::WAYLAND PkgConfig::WAYLAND_SERVER Threads::Threads)
        add_test(NAME wma_test_wayland_window_controls COMMAND wma_test_wayland_window_controls)
        set_tests_properties(wma_test_wayland_window_controls PROPERTIES TIMEOUT 15)
    else()
        message(STATUS "[wma] wayland-server not found; skipping headless window control tests.")
    endif()
endif()

# One X server serves all of them, and what they check (a window's pixels, the segments on the system) is disturbed by
# another test's window, so each takes the same lock: ctest -j never runs two together.
# These open windows, grab the pointer and inject input, so they only run against a
# disposable X server named by WMA_TEST_DISPLAY -- never the desktop's $DISPLAY.
# Without it they report skipped (77) rather than failing a headless build.
if(WMA_ENABLE_X11)
    add_executable(wma_test_x11_cpu_present test_x11_cpu_present.cpp)
    # Reads the framebuffer back with Xlib itself; wma binds libX11 at run time, so the test links it.
    target_link_libraries(wma_test_x11_cpu_present PRIVATE wma X11::X11)
    add_test(NAME wma_test_x11_cpu_present COMMAND wma_test_x11_cpu_present)
    set_tests_properties(wma_test_x11_cpu_present PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77 RESOURCE_LOCK wma_test_display)

    # xclip stands in for another application; without it only the checks that need no second client run.
    add_executable(wma_test_x11_clipboard test_x11_clipboard.cpp)
    target_link_libraries(wma_test_x11_clipboard PRIVATE wma)
    add_test(NAME wma_test_x11_clipboard COMMAND wma_test_x11_clipboard)
    set_tests_properties(wma_test_x11_clipboard PROPERTIES TIMEOUT 120 SKIP_RETURN_CODE 77 RESOURCE_LOCK wma_test_display)

    if(TARGET X11::Xtst)
        add_executable(wma_test_x11_mouse_capture test_x11_mouse_capture.cpp)
        target_link_libraries(wma_test_x11_mouse_capture PRIVATE wma X11::Xtst)
        add_test(NAME wma_test_x11_mouse_capture COMMAND wma_test_x11_mouse_capture)
        set_tests_properties(wma_test_x11_mouse_capture PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77 RESOURCE_LOCK wma_test_display)
    endif()
endif()

# The size and scale GlfwWindowManager keeps are the ones GLFW reported. Needs a display; WMA_TEST_DISPLAY names a
# disposable X server, and without it this reports skipped (77).
if(WMA_ENABLE_GLFW)
    add_executable(wma_test_glfw_clipboard test_backend_clipboard.cpp)
    target_link_libraries(wma_test_glfw_clipboard PRIVATE wma)
    target_compile_definitions(wma_test_glfw_clipboard PRIVATE WMA_TEST_BACKEND_GLFW)
    add_test(NAME wma_test_glfw_clipboard COMMAND wma_test_glfw_clipboard)
    set_tests_properties(wma_test_glfw_clipboard PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77 RESOURCE_LOCK wma_test_display)

    add_executable(wma_test_glfw_metrics test_glfw_metrics.cpp)
    target_link_libraries(wma_test_glfw_metrics PRIVATE wma glfw)
    add_test(NAME wma_test_glfw_metrics COMMAND wma_test_glfw_metrics)
    set_tests_properties(wma_test_glfw_metrics PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77 RESOURCE_LOCK wma_test_display)
endif()
