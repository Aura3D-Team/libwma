cmake_minimum_required(VERSION 4.3.3)

project(wma
VERSION 0.5.4
LANGUAGES C CXX
)

# The GLFW Metal path needs AppKit, which means one Objective-C++ translation
# unit (src/platform/apple/AppleMetalLayer.mm). enable_language() must run in the
# top-level scope before any target that compiles a .mm, and OBJCXX is only a
# supported language on Apple toolchains -- hence the guard, and hence APPLE
# rather than a WMA_* option (cmake/Platform.cmake, which decides the options, is
# included further down and so runs too late for this).
if(APPLE)
    enable_language(OBJCXX)
    set(CMAKE_OBJCXX_STANDARD 23)
    set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)

option(WMA_BUILD_TESTS      "Build test suite"                OFF)
option(WMA_BUILD_EXAMPLES   "Build example applications"      OFF)
option(WMA_ENABLE_LTO       "Enable link-time optimization"   ON)
# -march=native produces non-portable binaries; keep OFF by default for a library
# that may be installed and shipped. Enable it for machine-local builds.
option(WMA_NATIVE_OPTIMIZE  "Enable native CPU optimization"  OFF)

include(cmake/PlatformSuffix.cmake)
wma_platform_suffix(WMA_PLATFORM_SUFFIX)
message(STATUS "wma: installing as wma_${WMA_PLATFORM_SUFFIX}")

include(cmake/Platform.cmake)
include(cmake/Compiler.cmake)
include(cmake/Dependencies.cmake)

################################################################################
# wma library
################################################################################

# Public headers ship both .hpp and .h (Keys.h is a .h) — glob both.
file(GLOB_RECURSE WMA_HEADERS
    CONFIGURE_DEPENDS
    ${PROJECT_SOURCE_DIR}/include/*.hpp
    ${PROJECT_SOURCE_DIR}/include/*.h
)

file(GLOB_RECURSE WMA_SOURCES
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
)

# Remove source files that belong to disabled backends so their
# implementation-specific headers and macros are never required.
#
# Both trees are swept: backends/<name>/ holds windowing backends and
# audio/backends/<name>/ holds audio ones. They are separate directories rather
# than one because the two are independent axes -- SDL3 appears in both and is
# gated by the single WMA_ENABLE_SDL (one dependency, two subsystems), while
# ALSA is audio-only and GLFW/X11/Wayland are windowing-only. audio/backends/null
# is deliberately not in this list: it has no dependencies and is always built.
foreach(_backend SDL GLFW X11 WAYLAND ALSA)
    if(NOT WMA_ENABLE_${_backend})
        string(TOLOWER "${_backend}" _lower)
        file(GLOB_RECURSE _disabled CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/src/backends/${_lower}/*.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/audio/backends/${_lower}/*.cpp"
        )
        if(_disabled)
            list(REMOVE_ITEM WMA_SOURCES ${_disabled})
        endif()
    endif()
endforeach()

# Metal support: whether this build can honour GraphicsAPI::Metal at all. Apple
# platforms only, and only through a backend that has a route to a CAMetalLayer
# (SDL3 anywhere, GLFW on macOS). Exposed to consumers as WMA_HAS_METAL, so a
# renderer can feature-detect instead of duplicating this reasoning against
# __APPLE__ and the backend list.
set(WMA_HAS_METAL FALSE)
if(APPLE AND (WMA_ENABLE_SDL OR WMA_ENABLE_GLFW))
    set(WMA_HAS_METAL TRUE)

    # The AppKit half of the GLFW route -- the library's only Objective-C++, and
    # the reason the root CMakeLists calls enable_language(OBJCXX) for APPLE.
    # Compiled unconditionally on Apple rather than only for WMA_ENABLE_GLFW: the
    # file is a handful of lines, and its non-macOS branch is already a stub, so
    # keeping it out of the GLFW conditional means one fewer way for an
    # SDL-only-plus-GLFW-later build to fail to link.
    list(APPEND WMA_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/apple/AppleMetalLayer.mm")
endif()

add_library(wma STATIC)
add_library(wma::wma ALIAS wma)

#! One flat lib directory holds every platform's build, so the ABI tag is in
#! the filename: libwma_linux_x86_64.a, libwma_wasm32.a, ...
set_target_properties(wma PROPERTIES OUTPUT_NAME "wma_${WMA_PLATFORM_SUFFIX}")

target_sources(wma
    PRIVATE
        ${WMA_SOURCES}

    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS
            ${PROJECT_SOURCE_DIR}/include
        FILES
            ${WMA_HEADERS}
)

target_include_directories(wma
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    # Helpers shared between backends; never installed.
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

# Which backends this build contains. PUBLIC compile definitions rather than a
# generated header: every platform shares one include tree under the install
# prefix, so a header could not carry per-platform answers. All are defined,
# 0 or 1, preserving the `#if WMA_HAS_X` spelling the old BuildConfig.hpp used.
function(_wma_define_flag name value)
    if(value)
        target_compile_definitions(wma PUBLIC ${name}=1)
    else()
        target_compile_definitions(wma PUBLIC ${name}=0)
    endif()
endfunction()

_wma_define_flag(WMA_HAS_GLFW    "${WMA_ENABLE_GLFW}")
_wma_define_flag(WMA_HAS_SDL     "${WMA_ENABLE_SDL}")
_wma_define_flag(WMA_HAS_X11     "${WMA_ENABLE_X11}")
_wma_define_flag(WMA_HAS_WAYLAND "${WMA_ENABLE_WAYLAND}")
_wma_define_flag(WMA_HAS_ALSA    "${WMA_ENABLE_ALSA}")
# Resolved above; unlike the flags around it this depends on the target platform,
# not just on an option.
_wma_define_flag(WMA_HAS_METAL   "${WMA_HAS_METAL}")

target_compile_definitions(wma PUBLIC
    WMA_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    WMA_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    WMA_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

target_compile_features(wma
    PUBLIC
        cxx_std_23
)

target_compile_options(wma PRIVATE
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:
        $<$<CONFIG:Debug>:
            -Wall
            -Wextra
            -g
            -O0
        >

        $<$<CONFIG:Release>:-O3>

        $<$<AND:$<CONFIG:Release>,$<BOOL:${WMA_NATIVE_OPTIMIZE}>>:
            -march=native
            -mtune=native
        >
    >

    $<$<CXX_COMPILER_ID:MSVC>:
        /W4
        $<$<CONFIG:Debug>:/Od>
        $<$<CONFIG:Release>:/O2>
    >
)

# MSVC has no -march=native/-mtune=native equivalent, so WMA_NATIVE_OPTIMIZE
# is a no-op there — flag it instead of silently ignoring the request.
if(WMA_NATIVE_OPTIMIZE AND MSVC)
    message(STATUS "[wma] WMA_NATIVE_OPTIMIZE has no effect on MSVC (no -march=native equivalent); ignoring.")
endif()

# ThreadPool/WorkerThread live in ink::ink again as of ink 0.3.0, so there is
# no separate threading target to link -- on Emscripten or anywhere else.
target_link_libraries(wma PUBLIC ink::ink)

if(WMA_ENABLE_SDL)
    target_compile_definitions(wma PRIVATE WMA_ENABLE_SDL)
    target_link_libraries(wma PRIVATE SDL3::SDL3)
endif()

if(WMA_HAS_METAL)
    # QuartzCore for CAMetalLayer, Foundation for the Objective-C runtime types
    # AppleMetalLayer.mm touches. Metal itself is deliberately NOT linked: wma
    # creates the layer but never a MTLDevice — the renderer sets that, and it is
    # the renderer that links Metal.
    #
    # PUBLIC because wma is a static library: these symbols have to resolve at the
    # final link of whatever executable consumes it, not here.
    target_link_libraries(wma PUBLIC
        "-framework QuartzCore"
        "-framework Foundation")

    # AppKit hosts the layer in an NSView on macOS. iOS reaches its layer through
    # SDL's UIView-backed metal view instead, so nothing there needs UIKit from
    # this library.
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        target_link_libraries(wma PUBLIC "-framework AppKit")
    endif()
endif()

if(WMA_ENABLE_GLFW)
    target_compile_definitions(wma PRIVATE WMA_ENABLE_GLFW)
    target_link_libraries(wma PRIVATE glfw)
endif()

# See WMA_NEEDS_MULTIDEF_FIX in cmake/Dependencies.cmake. PUBLIC: this has to reach
# the final link of whatever executable consumes wma, the same as the Metal
# frameworks above.
if(WMA_NEEDS_MULTIDEF_FIX)
    target_link_options(wma PUBLIC "-Wl,--allow-multiple-definition")
endif()

if(WMA_ENABLE_ALSA)
    target_compile_definitions(wma PRIVATE WMA_ENABLE_ALSA)
    target_link_libraries(wma PRIVATE PkgConfig::ALSA)
endif()

if(WMA_ENABLE_X11)
    target_compile_definitions(wma PRIVATE WMA_ENABLE_X11)
    target_link_libraries(wma PRIVATE X11::X11)

    # OpenGL on X11 is done through GLX. Optional: if libGL/GLX is unavailable the
    # backend still builds and simply reports OpenGL as unsupported at runtime.
    if(WMA_ENABLE_X11_GL)
        target_link_libraries(wma PRIVATE OpenGL::GLX)
        target_compile_definitions(wma PRIVATE WMA_X11_HAS_GL)
    endif()
endif()

if(WMA_ENABLE_WAYLAND)
    target_compile_definitions(wma PRIVATE WMA_ENABLE_WAYLAND)
    target_link_libraries(wma PRIVATE PkgConfig::WAYLAND)

    # Generated protocol marshalling code (defines xdg_wm_base_interface & co.,
    # which the client headers only declare) — see cmake/Dependencies.cmake.
    target_sources(wma PRIVATE ${WMA_WAYLAND_GENERATED_SOURCES})

    # OpenGL on Wayland is done through EGL + wayland-egl (optional, as above).
    if(WMA_ENABLE_WAYLAND_GL)
        target_link_libraries(wma PRIVATE PkgConfig::WAYLAND_EGL)
        target_compile_definitions(wma PRIVATE WMA_WAYLAND_HAS_GL)
    endif()

    # See WMA_LIBDECOR_PLUGIN_DIR in cmake/Dependencies.cmake. GLFW-only: it feeds
    # GlfwNative.cpp's decision about the GLFW_WAYLAND_LIBDECOR hint.
    if(WMA_ENABLE_GLFW AND DEFINED WMA_LIBDECOR_PLUGIN_DIR)
        target_compile_definitions(wma PRIVATE
            WMA_LIBDECOR_PLUGIN_DIR="${WMA_LIBDECOR_PLUGIN_DIR}")
    endif()

    # Expose the build-time generated protocol headers to the compiler and
    # install them alongside the other public headers.
    target_include_directories(wma PUBLIC
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/include>
    )
    install(
        DIRECTORY "${WMA_WAYLAND_PROTO_DIR}/"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/wma/backends/wayland/protocols"
        FILES_MATCHING PATTERN "*.h"
    )
endif()

################################################################################
# Docs
################################################################################

# Not a build input: lists docs/ in the IDE project tree.
file(GLOB WMA_DOC_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/docs/*.md")
add_custom_target(wma_docs SOURCES ${WMA_DOC_FILES} README.md CHANGELOG.md)

if(WMA_BUILD_TESTS)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
        #! Must precede add_subdirectory: add_test() only registers with CTest
        #! once testing is enabled, and otherwise builds the binaries while
        #! leaving `ctest` reporting no tests at all.
        enable_testing()
        add_subdirectory(tests)
    else()
        message(WARNING "[wma] WMA_BUILD_TESTS=ON but tests/ directory is absent — skipping.")
    endif()
endif()

if(WMA_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

include(cmake/Install.cmake)
