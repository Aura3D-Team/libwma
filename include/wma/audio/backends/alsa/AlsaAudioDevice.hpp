#ifndef WMA_AUDIO_BACKENDS_ALSA_AUDIO_DEVICE_HPP
#define WMA_AUDIO_BACKENDS_ALSA_AUDIO_DEVICE_HPP

#include <atomic>
#include <thread>
#include <vector>

#include "wma/audio/IAudioDevice.hpp"
#include "wma/audio/MixCallbackSlot.hpp"

//! Opaque in this header on purpose: <alsa/asoundlib.h> is a large C header
//! that would land in every consumer TU including wma/audio headers. Only the
//! .cpp sees it.
// ALSA owns this tag; the forward declaration must match its C ABI.
// NOLINTNEXTLINE(bugprone-reserved-identifier)
typedef struct _snd_pcm snd_pcm_t;

namespace wma
{

/**
 * @brief Native Linux audio output through libasound (ALSA).
 *
 * The low-latency desktop-Linux path: it drives the kernel PCM interface
 * directly instead of going through a sound server's client library. A
 * machine running PulseAudio or PipeWire still works — both register an
 * ALSA-compatible "default" PCM — the samples simply take one hop fewer
 * than they would through those servers' own APIs.
 *
 * Unlike SDL3, ALSA does not own a callback thread. This class runs its
 * own: a loop that pulls one period from the mix callback and blocks in
 * snd_pcm_writei() until the hardware has room for it, which is what paces
 * playback.
 *
 * Compiled only when WMA_ENABLE_ALSA is on, which the build forces off
 * everywhere ALSA does not exist (Android, WASM, Windows, Apple).
 */
class AlsaAudioDevice final : public IAudioDevice
{
  public:
    AlsaAudioDevice() = default;
    ~AlsaAudioDevice() override;

    AlsaAudioDevice(const AlsaAudioDevice &) = delete;
    AlsaAudioDevice &operator=(const AlsaAudioDevice &) = delete;

    /**
     * @brief Open the PCM named by @p config.deviceName, or "default".
     *
     * ALSA negotiates: the rate and period size it grants may differ from
     * the request, and getConfig() reports what it settled on.
     *
     * @note Samples are handed over as 32-bit float, which most cards do
     *       not accept natively — the conversion is ALSA's to do. "default"
     *       and any "plughw:X,Y" name include the plug layer that performs
     *       it, but a raw "hw:X,Y" does not and will fail to open with
     *       "Sample format not available" on such a card. Prefer plughw
     *       over hw unless you know the device takes float directly.
     */
    [[nodiscard]] WmaCode open(const AudioDeviceConfig &config) override;

    void close() noexcept override;
    [[nodiscard]] WmaCode start() override;
    void stop() noexcept override;
    [[nodiscard]] bool isRunning() const noexcept override;
    void setMixCallback(AudioMixCallback callback) override;
    [[nodiscard]] const AudioDeviceConfig &getConfig() const noexcept override;
    [[nodiscard]] AudioBackend getBackendType() const noexcept override;

  private:
    //! Body of the writer thread: mix a period, write it, recover from
    //! underruns, repeat until asked to stop.
    void writerLoop(const std::stop_token &stopToken);

    snd_pcm_t *_pcm = nullptr;
    AudioDeviceConfig _config{};
    MixCallbackSlot _mixCallback{};
    std::vector<f32> _scratch{};
    std::jthread _writer{};
    std::atomic<bool> _running{false};
};

} // namespace wma

#endif // WMA_AUDIO_BACKENDS_ALSA_AUDIO_DEVICE_HPP
