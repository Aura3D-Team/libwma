# Deliberately framework-free: these assert on pure functions, and a
# dependency-free test binary keeps `ctest` runnable in the same containers
# that build the library, with nothing to fetch or vendor.
add_executable(wma_test_utf8 test_utf8.cpp)
target_link_libraries(wma_test_utf8 PRIVATE wma)
add_test(NAME wma_test_utf8 COMMAND wma_test_utf8)

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
                                   "unstable/xdg-decoration/xdg-decoration-unstable-v1")
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
