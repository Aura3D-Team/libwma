#ifndef WMA_AUDIO_BACKENDS_NULL_AUDIO_DEVICE_HPP
#define WMA_AUDIO_BACKENDS_NULL_AUDIO_DEVICE_HPP

#include <vector>

#include "wma/audio/IAudioDevice.hpp"

namespace wma
{

/**
 * @brief Audio device that accepts everything and plays nothing.
 *
 * Opens unconditionally and reports back exactly the configuration it was
 * asked for, so code above it behaves identically to a real device — it
 * simply never pulls the mix callback and produces no sound.
 *
 * This is the audio equivalent of IWindowManager's inert TouchListener: it
 * exists so that "no audio hardware" is an ordinary runtime state instead of
 * an error path every caller has to branch on. Two uses in practice:
 * headless CI containers, which have neither ALSA device nodes nor a sound
 * server, and unit tests that need to drive a mixer deterministically
 * without a real clock running underneath it.
 *
 * Always compiled in — it depends on nothing — which is what lets
 * createAudioDevice() guarantee it can always return *some* device.
 */
class NullAudioDevice final : public IAudioDevice
{
  public:
    NullAudioDevice() = default;
    ~NullAudioDevice() override;

    NullAudioDevice(const NullAudioDevice &) = delete;
    NullAudioDevice &operator=(const NullAudioDevice &) = delete;

    [[nodiscard]] WmaCode open(const AudioDeviceConfig &config) override;
    void close() noexcept override;
    [[nodiscard]] WmaCode start() override;
    void stop() noexcept override;
    [[nodiscard]] bool isRunning() const noexcept override;
    void setMixCallback(AudioMixCallback callback) override;
    [[nodiscard]] const AudioDeviceConfig &getConfig() const noexcept override;
    [[nodiscard]] AudioBackend getBackendType() const noexcept override;

    /**
     * @brief Pull @p frames worth of samples by hand, as a device would.
     *
     * Test-only entry point: the whole purpose of this backend is that no
     * audio thread exists, so this is how a test advances the mixer and
     * inspects what it produced. Returns the buffer the callback filled, or
     * an empty span when no callback is installed or the device is stopped.
     *
     * The returned span points at storage owned by this device and stays
     * valid until the next call.
     */
    [[nodiscard]] std::span<const f32> renderFrames(u32 frames);

  private:
    AudioDeviceConfig _config{};
    AudioMixCallback _mixCallback{};
    std::vector<f32> _scratch{};
    bool _open = false;
    bool _running = false;
};

} // namespace wma

#endif // WMA_AUDIO_BACKENDS_NULL_AUDIO_DEVICE_HPP
