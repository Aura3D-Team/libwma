#ifndef WMA_AUDIO_AUDIO_TYPES_HPP
#define WMA_AUDIO_AUDIO_TYPES_HPP

#include <span>

#include "../core/Types.hpp"

namespace wma
{

/**
 * @brief Fills @p output with interleaved 32-bit float samples.
 *
 * Invoked on the backend's audio thread (SDL3's callback thread, or the
 * dedicated writer thread AlsaAudioDevice owns) — *not* the thread that
 * called start(). Everything the callback touches is shared across that
 * boundary and must be synchronized accordingly.
 *
 * The span is exactly `frames * channelCount` elements. Implementations
 * must write every element: the buffer is not pre-zeroed, so a callback
 * that returns early leaves whatever the previous period held and the
 * device plays it back as a repeated fragment.
 *
 * Samples are nominally in [-1, 1]; the backend does not clamp, so values
 * outside that range reach the hardware as clipping.
 *
 * @warning Runs under a hard real-time deadline. Blocking, allocating, or
 *          taking a lock the game thread can hold for long stretches shows
 *          up directly as an audible dropout.
 */
using AudioMixCallback = move_only_function<void(std::span<f32> output)>;

/**
 * @brief What an IAudioDevice is asked to open, and what it actually got.
 *
 * Passed to IAudioDevice::open() as a request. Backends negotiate with the
 * hardware and may land on different values, so the granted configuration
 * is read back through IAudioDevice::getConfig() rather than assumed to
 * match — a mixer that hardcodes the requested rate will play back at the
 * wrong pitch on a device that refused it.
 */
struct AudioDeviceConfig
{
    /// Frames per second. 48 kHz is the native rate of essentially all
    /// modern hardware; 44.1 kHz costs a resample in the driver.
    u32 sampleRate = 48000;

    /// Interleaved channels per frame. 1 = mono, 2 = stereo.
    u16 channelCount = 2;

    /**
     * @brief Frames the device asks for per callback.
     *
     * The latency/robustness dial: 1024 frames at 48 kHz is ~21 ms, low
     * enough for gameplay SFX and forgiving enough not to underrun on a
     * loaded machine. Backends treat it as a hint — SDL3 in particular
     * chooses its own period size and simply calls back for whatever it
     * needs.
     */
    u32 framesPerBuffer = 1024;

    /**
     * @brief Backend-specific device name, or nullptr for the system default.
     *
     * Only AlsaAudioDevice reads this (an ALSA PCM name such as "default",
     * "hw:0,0" or "pipewire"). Ignored by every other backend, which always
     * open the platform's default playback device.
     */
    const char *deviceName = nullptr;

    [[nodiscard]] constexpr bool valid() const noexcept
    {
        return sampleRate > 0 && channelCount > 0 && framesPerBuffer > 0;
    }

    //! Elements per callback buffer: frames times interleaved channels.
    [[nodiscard]] constexpr usize samplesPerBuffer() const noexcept
    {
        return static_cast<usize>(framesPerBuffer) * channelCount;
    }
};

} // namespace wma

#endif // WMA_AUDIO_AUDIO_TYPES_HPP
