#ifndef WMA_AUDIO_BACKENDS_SDL_AUDIO_DEVICE_HPP
#define WMA_AUDIO_BACKENDS_SDL_AUDIO_DEVICE_HPP

#include <vector>

#include "wma/audio/IAudioDevice.hpp"
#include "wma/audio/MixCallbackSlot.hpp"

struct SDL_AudioStream;

namespace wma
{

/**
 * @brief Audio output through SDL3's audio stream API.
 *
 * The portable backend, and the only one that exists on Android, WASM and
 * Apple platforms — SDL3 resolves internally to AAudio/OpenSL ES, Web Audio
 * and CoreAudio, so writing separate native backends for those targets
 * would only duplicate work SDL already does. On desktop Linux it works
 * too, but AlsaAudioDevice is preferred there for latency.
 *
 * SDL pulls rather than pushes: it owns the audio thread and calls back
 * asking for more data, which this class satisfies by invoking the mix
 * callback and handing the result to SDL_PutAudioStreamData().
 *
 * @note SDL's audio subsystem is initialized through SDL_InitSubSystem, not
 *       SDL_Init, so a device coexists with an SdlWindowManager without
 *       either tearing the other's subsystem down. The one ordering
 *       constraint is at shutdown: the window manager calls SDL_Quit() when
 *       its last window dies, which quits *every* subsystem — so destroy
 *       audio devices before the last window, not after.
 */
class SDLAudioDevice final : public IAudioDevice
{
  public:
    SDLAudioDevice() = default;
    ~SDLAudioDevice() override;

    SDLAudioDevice(const SDLAudioDevice &) = delete;
    SDLAudioDevice &operator=(const SDLAudioDevice &) = delete;

    [[nodiscard]] WmaCode open(const AudioDeviceConfig &config) override;
    void close() noexcept override;

    /**
     * @brief Resume the SDL stream's device.
     *
     * @note On Emscripten the browser refuses to start audio until the page
     *       has seen a user gesture (click, key, touch). SDL reports success
     *       here regardless and the context stays suspended until that
     *       happens, so a web build should call this again from an input
     *       handler — repeat calls on an already-running device are
     *       harmless.
     */
    [[nodiscard]] WmaCode start() override;

    void stop() noexcept override;
    [[nodiscard]] bool isRunning() const noexcept override;
    void setMixCallback(AudioMixCallback callback) override;
    [[nodiscard]] const AudioDeviceConfig &getConfig() const noexcept override;
    [[nodiscard]] AudioBackend getBackendType() const noexcept override;

  private:
    //! SDL's "give me more audio" callback. Trampolines to fillStream() on
    //! the instance carried through SDL's userdata pointer.
    static void streamCallback(void *userdata, SDL_AudioStream *stream, int additionalAmount, int totalAmount);

    void fillStream(SDL_AudioStream *stream, int additionalBytes);

    SDL_AudioStream *_stream = nullptr;
    AudioDeviceConfig _config{};
    MixCallbackSlot _mixCallback{};
    //! Reused across callbacks: allocating on the audio thread is exactly
    //! the kind of unbounded pause that produces an audible dropout.
    std::vector<f32> _scratch{};
    bool _ownsSubsystem = false;
    bool _running = false;
};

} // namespace wma

#endif // WMA_AUDIO_BACKENDS_SDL_AUDIO_DEVICE_HPP
