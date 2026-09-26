#ifdef WMA_ENABLE_SDL

#include "wma/audio/backends/sdl/SDLAudioDevice.hpp"

#include <algorithm>
#include <utility>

#include <SDL3/SDL.h>

#include <ink/Inkogger.h>

namespace wma
{

SDLAudioDevice::~SDLAudioDevice()
{
    SDLAudioDevice::close();
}

WmaCode SDLAudioDevice::open(const AudioDeviceConfig &config)
{
    if (!config.valid())
        return WmaCode::Error;

    if (_stream)
        close();

    //! Subsystem-scoped rather than SDL_Init: audio must come up (and go down)
    //! without disturbing the video subsystem a window manager may already own.
    //! SDL reference-counts this internally, so several devices are fine.
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        INK_WARN << "[wma] SDL audio subsystem failed to initialize: " << SDL_GetError();
        return WmaCode::Error;
    }
    _ownsSubsystem = true;

    const SDL_AudioSpec spec{
        .format = SDL_AUDIO_F32,
        .channels = static_cast<int>(config.channelCount),
        .freq = static_cast<int>(config.sampleRate),
    };

    _stream =
        SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &SDLAudioDevice::streamCallback, this);
    if (!_stream)
    {
        INK_WARN << "[wma] SDL could not open the default playback device: " << SDL_GetError();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        _ownsSubsystem = false;
        return WmaCode::Error;
    }

    //! SDL converts transparently between the stream's format and whatever the
    //! hardware actually runs at, so the requested spec is what reaches the mix
    //! callback -- unlike ALSA, this never comes back changed. The one value
    //! worth reading back is the device's real period size, since SDL sizes its
    //! callbacks from that rather than from framesPerBuffer.
    _config = config;

    SDL_AudioSpec deviceSpec{};
    int deviceFrames = 0;
    if (SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(_stream), &deviceSpec, &deviceFrames) && deviceFrames > 0)
    {
        _config.framesPerBuffer = static_cast<u32>(deviceFrames);
    }

    //! Four periods of headroom: SDL asks for whatever it needs, which can
    //! exceed one period after a stall, and growing the buffer inside the
    //! callback would mean allocating on the audio thread.
    _scratch.assign(_config.samplesPerBuffer() * 4u, 0.0f);

    INK_INFO << "[wma] SDL audio device opened: " << _config.sampleRate << " Hz, " << _config.channelCount << " ch, "
             << _config.framesPerBuffer << " frames/period";

    return WmaCode::Ok;
}

void SDLAudioDevice::close() noexcept
{
    if (_stream)
    {
        //! Destroying the stream unbinds it from the device and guarantees the
        //! callback has finished, which is what makes touching _mixCallback and
        //! _scratch below safe.
        SDL_DestroyAudioStream(_stream);
        _stream = nullptr;
    }

    _running = false;
    _mixCallback.clear();

    if (_ownsSubsystem)
    {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        _ownsSubsystem = false;
    }

    _scratch.clear();
    _scratch.shrink_to_fit();
}

WmaCode SDLAudioDevice::start()
{
    if (!_stream)
        return WmaCode::Error;

    if (!SDL_ResumeAudioStreamDevice(_stream))
    {
        INK_WARN << "[wma] SDL failed to resume the audio device: " << SDL_GetError();
        return WmaCode::Error;
    }

    _running = true;
    return WmaCode::Ok;
}

void SDLAudioDevice::stop() noexcept
{
    if (!_stream)
        return;

    //! SDL guarantees the callback is not running once this returns, which is
    //! the postcondition IAudioDevice::stop() promises.
    SDL_PauseAudioStreamDevice(_stream);
    _running = false;
}

bool SDLAudioDevice::isRunning() const noexcept
{
    return _running;
}

void SDLAudioDevice::setMixCallback(AudioMixCallback callback)
{
    _mixCallback.store(std::move(callback));
}

const AudioDeviceConfig &SDLAudioDevice::getConfig() const noexcept
{
    return _config;
}

AudioBackend SDLAudioDevice::getBackendType() const noexcept
{
    return AudioBackend::Sdl3;
}

void SDLAudioDevice::streamCallback(void *userdata, SDL_AudioStream *stream, int additionalAmount, int /*totalAmount*/)
{
    if (auto *self = static_cast<SDLAudioDevice *>(userdata))
        self->fillStream(stream, additionalAmount);
}

void SDLAudioDevice::fillStream(SDL_AudioStream *stream, int additionalBytes)
{
    if (additionalBytes <= 0)
        return;

    usize samplesWanted = static_cast<usize>(additionalBytes) / sizeof(f32);
    if (samplesWanted == 0)
        return;

    /*
     * Clamped, never grown. open() sizes _scratch to four periods, so a larger
     * request means SDL is recovering from a stall -- and resizing here would
     * allocate on the audio thread, turning a momentary shortfall into an
     * unbounded pause. Feeding what fits leaves SDL to ask again for the rest.
     */
    if (samplesWanted > _scratch.size()) [[unlikely]]
        samplesWanted = _scratch.size();

    const std::span<f32> block{_scratch.data(), samplesWanted};

    //! Picks up a callback installed since the last period, and writes silence
    //! when none is installed -- a normal state for a game that has loaded no
    //! audio yet.
    _mixCallback.invoke(block);

    SDL_PutAudioStreamData(stream, block.data(), static_cast<int>(samplesWanted * sizeof(f32)));
}

} // namespace wma

#endif // WMA_ENABLE_SDL
