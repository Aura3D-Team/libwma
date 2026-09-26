#include "wma/wma.hpp"

//! This TU is compiled INTO the library, where the private WMA_ENABLE_* backend
//! macros and the backend SDK headers (alsa/asoundlib.h, SDL3) are visible.
//! Keeping the factory here rather than inline in a public header is what lets a
//! consumer pick an audio backend without ALSA or SDL3 on its own include path.

#include "wma/audio/backends/null/NullAudioDevice.hpp"

#ifdef WMA_ENABLE_ALSA
#include "wma/audio/backends/alsa/AlsaAudioDevice.hpp"
#endif
#ifdef WMA_ENABLE_SDL
#include "wma/audio/backends/sdl/SDLAudioDevice.hpp"
#endif

#include <array>

#include <ink/Inkogger.h>

namespace wma
{

std::unique_ptr<IAudioDevice> createAudioDevice(AudioBackend backend)
{
    switch (backend)
    {
#ifdef WMA_ENABLE_ALSA
    case AudioBackend::Alsa:
        return std::make_unique<AlsaAudioDevice>();
#endif
#ifdef WMA_ENABLE_SDL
    case AudioBackend::Sdl3:
        return std::make_unique<SDLAudioDevice>();
#endif
    case AudioBackend::Null:
        //! Never gated: it depends on nothing, and it is what guarantees
        //! openAudioDevice() always has something left to fall back to.
        return std::make_unique<NullAudioDevice>();
    default:
        break;
    }
    throw AudioException("Requested audio backend is not available or not compiled in");
}

AudioBackend getDefaultAudioBackend() noexcept
{
#if defined(WMA_ENABLE_ALSA)
    //! Native Linux: one hop fewer than SDL3's route to the same hardware.
    return AudioBackend::Alsa;
#elif defined(WMA_ENABLE_SDL)
    //! The only backend on Android/WASM/Apple, where SDL3 already *is* the
    //! platform-native path (AAudio/OpenSL ES, Web Audio, CoreAudio).
    return AudioBackend::Sdl3;
#else
    return AudioBackend::Null;
#endif
}

bool isAudioBackendAvailable(AudioBackend backend) noexcept
{
    return backend == AudioBackend::Null || (backend == AudioBackend::Alsa && WMA_HAS_ALSA) ||
           (backend == AudioBackend::Sdl3 && WMA_HAS_SDL);
}

const char *audioBackendName(AudioBackend backend) noexcept
{
    switch (backend)
    {
    case AudioBackend::Alsa:
        return "ALSA";
    case AudioBackend::Sdl3:
        return "SDL3";
    case AudioBackend::Null:
        return "Null";
    }
    return "Unknown";
}

std::unique_ptr<IAudioDevice> openAudioDevice(const AudioDeviceConfig &config, AudioBackend preferred)
{
    //! Preference first, then the rest in descending order of directness. Null
    //! anchors the chain: it cannot fail to open, so this never returns null.
    const std::array<AudioBackend, 4> chain{
        preferred,
        AudioBackend::Alsa,
        AudioBackend::Sdl3,
        AudioBackend::Null,
    };

    for (usize i = 0; i < chain.size(); ++i)
    {
        const AudioBackend backend = chain[i];

        //! Skip a backend the preference already covered, and any the build
        //! left out.
        if (i > 0 && backend == preferred)
            continue;
        if (!isAudioBackendAvailable(backend))
            continue;

        std::unique_ptr<IAudioDevice> device = createAudioDevice(backend);

        if (device->open(config) == WmaCode::Ok)
        {
            if (backend != preferred)
            {
                INK_WARN << "[wma] audio backend " << audioBackendName(preferred) << " unavailable; using "
                         << audioBackendName(backend) << " instead";
            }
            return device;
        }
    }

    //! Unreachable in practice: NullAudioDevice::open() only fails on a config
    //! that is invalid on its face, which every branch above would reject too.
    throw AudioException("No audio backend could open a device with the requested configuration");
}

} // namespace wma
