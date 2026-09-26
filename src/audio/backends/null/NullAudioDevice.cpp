#include "wma/audio/backends/null/NullAudioDevice.hpp"

namespace wma
{

NullAudioDevice::~NullAudioDevice()
{
    NullAudioDevice::close();
}

WmaCode NullAudioDevice::open(const AudioDeviceConfig &config)
{
    if (!config.valid())
        return WmaCode::Error;

    //! Reported back verbatim: with no hardware to negotiate with, the request
    //! is always granted exactly.
    _config = config;
    _scratch.assign(_config.samplesPerBuffer(), 0.0f);
    _open = true;
    return WmaCode::Ok;
}

void NullAudioDevice::close() noexcept
{
    _running = false;
    _open = false;
    _scratch.clear();
    _scratch.shrink_to_fit();
}

WmaCode NullAudioDevice::start()
{
    if (!_open)
        return WmaCode::Error;

    _running = true;
    return WmaCode::Ok;
}

void NullAudioDevice::stop() noexcept
{
    //! No audio thread exists, so there is nothing to join: the postcondition
    //! that the callback is not executing on return holds trivially.
    _running = false;
}

bool NullAudioDevice::isRunning() const noexcept
{
    return _running;
}

void NullAudioDevice::setMixCallback(AudioMixCallback callback)
{
    _mixCallback = std::move(callback);
}

const AudioDeviceConfig &NullAudioDevice::getConfig() const noexcept
{
    return _config;
}

AudioBackend NullAudioDevice::getBackendType() const noexcept
{
    return AudioBackend::Null;
}

std::span<const f32> NullAudioDevice::renderFrames(u32 frames)
{
    if (!_running || !_mixCallback || frames == 0)
        return {};

    const usize samples = static_cast<usize>(frames) * _config.channelCount;
    if (_scratch.size() < samples)
        _scratch.resize(samples);

    const std::span<f32> block{_scratch.data(), samples};
    _mixCallback(block);
    return block;
}

} // namespace wma
