cmake_minimum_required(VERSION 4.3.3)

project(wma
VERSION 0.3.1
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

add_subdirectory(src)

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
