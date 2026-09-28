#ifndef WMA_H
#define WMA_H

#include <memory>

#include <ink/InkAssert.h>
#include <ink/ink_base.hpp>

#include "audio/AudioTypes.hpp"
#include "audio/IAudioDevice.hpp"
#include "core/Types.hpp"
#include "core/WindowDetails.hpp"
#include "core/WindowFlags.hpp"
#include "exceptions/WMAException.hpp"
#include "input/keyboard/KeyAction.hpp"
#include "input/keyboard/KeyboardListener.hpp"
#include "input/keyboard/Keys.h"
#include "input/mouse/MouseAction.hpp"
#include "input/mouse/MouseListener.hpp"
#include "managers/IWindowManager.hpp"

//! NOTE: concrete backend headers (SdlWindowManager.hpp, …) are intentionally NOT
//! included here. They pull in backend SDK headers (SDL3, GLFW, Xlib, wayland) and
//! would force every consumer TU to have those on its include path. The factory
//! below is defined inside the compiled library where the private WMA_ENABLE_*
//! build macros and SDK headers are available. Use WMA_HAS_* for feature
//! detection; include a concrete backend header explicitly only if you need
//! backend-specific API.

//! WMA_HAS_*/WMA_VERSION_* arrive as compile definitions on wma::wma, since one
//! include tree serves every platform under a shared prefix. Including this
//! header without linking the target would otherwise read every backend as
//! absent, which is a wrong answer rather than a failure.
#if !defined(WMA_HAS_SDL)
#error "wma: link wma::wma -- WMA_HAS_*/WMA_VERSION_* come from its compile definitions"
#endif

#define WMA_MAJOR_VERSION WMA_VERSION_MAJOR
#define WMA_MINOR_VERSION WMA_VERSION_MINOR
#define WMA_PATCH_VERSION WMA_VERSION_PATCH
#define WMA_VERSION ((WMA_MAJOR_VERSION * 10000) + (WMA_MINOR_VERSION * 100) + WMA_PATCH_VERSION)
#define WMA_VERSION_STRING_FULL INK_STR(WMA_MAJOR_VERSION) "." INK_STR(WMA_MINOR_VERSION) "." INK_STR(WMA_PATCH_VERSION)

namespace wma
{

/**
 * @brief Create a window manager for the requested backend and graphics API.
 * @throws WMAException if the backend was not compiled into the library.
 *
 * Availability is decided by what the library was built with, not by the
 * consumer's TU — query isBackendAvailable() first, or use getDefaultBackend().
 */
[[nodiscard]] std::unique_ptr<IWindowManager>
createWindowManager(WindowBackend backend, const WindowDetails &windowDetails, GraphicsAPI graphicsAPI);

//! The preferred backend compiled into this build (SDL3 on WASM/Android).
[[nodiscard]] WindowBackend getDefaultBackend();

//! Whether @p backend was compiled into the library.
[[nodiscard]] bool isBackendAvailable(WindowBackend backend) noexcept;

/**
 * @brief Create an audio device for @p backend, without opening it.
 * @throws AudioException if the backend was not compiled into the library.
 *
 * The low-level entry point, mirroring createWindowManager(). Most callers
 * want openAudioDevice() instead, which also opens the device and degrades
 * to a working backend when the preferred one has no hardware behind it.
 */
[[nodiscard]] std::unique_ptr<IAudioDevice> createAudioDevice(AudioBackend backend);

//! The preferred audio backend compiled into this build: ALSA on desktop
//! Linux, SDL3 elsewhere, Null when the library was built with neither.
//! Declared ahead of openAudioDevice() because it supplies its default
//! argument, which may only name entities already declared.
[[nodiscard]] AudioBackend getDefaultAudioBackend() noexcept;

/**
 * @brief Create and open an audio device, degrading until one works.
 *
 * Tries @p preferred first, then the remaining compiled-in backends in
 * descending order of directness, ending at AudioBackend::Null — which
 * cannot fail — so this always returns a usable, already-open device and
 * never null. A machine with no sound hardware therefore yields a silent
 * device rather than an error the caller has to handle.
 *
 * The returned device is open but not started: install a mix callback with
 * IAudioDevice::setMixCallback(), then call IAudioDevice::start().
 *
 * @param config    Requested format. Read IAudioDevice::getConfig() back
 *                  afterwards for what was actually granted.
 * @param preferred Backend to try first; defaults to the platform's most
 *                  direct route.
 */
[[nodiscard]] std::unique_ptr<IAudioDevice> openAudioDevice(const AudioDeviceConfig &config,
                                                            AudioBackend preferred = getDefaultAudioBackend());

//! Whether @p backend was compiled into the library. Always true for
//! AudioBackend::Null.
[[nodiscard]] bool isAudioBackendAvailable(AudioBackend backend) noexcept;

//! Human-readable name for @p backend ("ALSA", "SDL3", "Null").
[[nodiscard]] const char *audioBackendName(AudioBackend backend) noexcept;

//! Human-readable build summary (version, backends, graphics APIs).
[[nodiscard]] const char *getLibraryInfo() noexcept;

} // namespace wma

#endif // WMA_H
