#ifdef WMA_ENABLE_ALSA

#include "wma/audio/backends/alsa/AlsaAudioDevice.hpp"

#include <algorithm>
#include <thread>
#include <utility>

#include <alsa/asoundlib.h>
#include <pthread.h>
#include <sched.h>

#include <ink/Inkogger.h>

namespace wma {

namespace {
    //! ALSA's native-endian 32-bit float. The mix callback produces f32, so
    //! this is the format that needs no conversion in the driver.
    constexpr snd_pcm_format_t kSampleFormat = SND_PCM_FORMAT_FLOAT;

    //! PCM to open when the caller does not name one. "default" follows the
    //! user's asoundrc, which is what routes correctly on a PulseAudio or
    //! PipeWire desktop instead of grabbing the raw card.
    constexpr const char* kDefaultDeviceName = "default";

    //! Periods of slack between the application and the hardware pointer.
    //! Three rather than the double-buffered minimum of two: the writer
    //! thread is ordinary-priority whenever setWriterRealtimePriority()
    //! below can't get SCHED_FIFO (the common case outside a container with
    //! CAP_SYS_NICE), so it has no scheduling guarantee against a CPU-heavy
    //! render thread stealing its core for longer than one period. One
    //! extra period of buffer absorbs a missed wakeup as added latency
    //! instead of an audible underrun.
    constexpr u32 kPeriodsOfLatency = 3;

    //! Best-effort real-time priority for the calling thread, so the kernel
    //! preempts CPU-bound work (a software rasterizer with no FPS cap, say)
    //! to let it refill the hardware buffer on schedule instead of starving
    //! it into an underrun. Silently a no-op without CAP_SYS_NICE or an
    //! rtprio limit -- the ordinary case in a container or unprivileged
    //! process -- which is why kPeriodsOfLatency above does not depend on
    //! this succeeding.
    void setWriterRealtimePriority() noexcept
    {
        sched_param param{};
        param.sched_priority = sched_get_priority_min(SCHED_FIFO) + 10;
        pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
    }

    //! Reserves the last CPU for this thread, so an unrelated CPU-bound
    //! thread elsewhere in the process (a software rasterizer with no FPS
    //! cap, say) has to be scheduled on a *different* core rather than
    //! merely being outranked on a shared one -- a guarantee priority alone
    //! doesn't give, and one that (unlike SCHED_FIFO above) does not need
    //! CAP_SYS_NICE. Skipped below two cores: there is nothing to reserve
    //! from, and restricting the only core would just add contention with
    //! nowhere else to run.
    //!
    //! Only isolates this thread, not the other side -- nothing here stops
    //! aura3d::JobSystem's worker pool from also landing on the reserved
    //! core, which on a fully-saturated worker count (every core requested)
    //! it will. The absolute guarantee needs the engine's own thread pool to
    //! leave this core out of its affinity too; this is the half wma can
    //! give unilaterally.
    void setWriterCoreAffinity() noexcept
    {
        const unsigned cores = std::thread::hardware_concurrency();
        if (cores < 2)
            return;

        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cores - 1, &cpuset);
        pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
    }
} // namespace

AlsaAudioDevice::~AlsaAudioDevice()
{
    AlsaAudioDevice::close();
}

WmaCode AlsaAudioDevice::open(const AudioDeviceConfig& config)
{
    if (!config.valid())
        return WmaCode::Error;

    if (_pcm)
        close();

    const char* deviceName = config.deviceName ? config.deviceName : kDefaultDeviceName;

    if (const int err = snd_pcm_open(&_pcm, deviceName, SND_PCM_STREAM_PLAYBACK, 0); err < 0)
    {
        //! Expected whenever there is no sound hardware -- a CI container, a
        //! headless server. The factory degrades to the next backend, so this
        //! is a warning rather than an error.
        INK_WARN << "[wma] ALSA could not open PCM '" << deviceName << "': " << snd_strerror(err);
        _pcm = nullptr;
        return WmaCode::Error;
    }

    //! Latency is expressed to ALSA in microseconds, from which it derives the
    //! period and buffer sizes itself. Deriving it from the requested period
    //! keeps AudioDeviceConfig::framesPerBuffer meaningful as the latency dial
    //! it is documented to be.
    const unsigned int latencyUs = static_cast<unsigned int>(
        (static_cast<u64>(config.framesPerBuffer) * kPeriodsOfLatency * 1'000'000ull)
        / config.sampleRate);

    unsigned int rate = config.sampleRate;

    if (const int err = snd_pcm_set_params(_pcm,
                                           kSampleFormat,
                                           SND_PCM_ACCESS_RW_INTERLEAVED,
                                           static_cast<unsigned int>(config.channelCount),
                                           rate,
                                           1 /* allow the driver to resample */,
                                           latencyUs);
        err < 0)
    {
        INK_WARN << "[wma] ALSA rejected the requested format on '" << deviceName
                 << "': " << snd_strerror(err);
        snd_pcm_close(_pcm);
        _pcm = nullptr;
        return WmaCode::Error;
    }

    _config = config;
    _config.deviceName = deviceName;

    //! What ALSA actually granted. The period size in particular is routinely
    //! not what was asked for, and it decides how much the writer thread mixes
    //! per iteration.
    snd_pcm_uframes_t bufferFrames = 0;
    snd_pcm_uframes_t periodFrames = 0;
    if (snd_pcm_get_params(_pcm, &bufferFrames, &periodFrames) == 0 && periodFrames > 0)
        _config.framesPerBuffer = static_cast<u32>(periodFrames);

    //! Rate is read back separately: snd_pcm_set_params takes the rate by value
    //! and, with resampling enabled, silently accepts one the hardware cannot
    //! do natively. A mixer that assumed the requested rate here would play
    //! every sound at the wrong pitch.
    {
        snd_pcm_hw_params_t* hwParams = nullptr;
        snd_pcm_hw_params_alloca(&hwParams);
        unsigned int actualRate = 0;
        if (snd_pcm_hw_params_current(_pcm, hwParams) == 0
            && snd_pcm_hw_params_get_rate(hwParams, &actualRate, nullptr) == 0
            && actualRate > 0)
        {
            _config.sampleRate = static_cast<u32>(actualRate);
        }
    }

    _scratch.assign(_config.samplesPerBuffer(), 0.0f);

    INK_INFO << "[wma] ALSA audio device opened on '" << deviceName << "': "
             << _config.sampleRate << " Hz, " << _config.channelCount << " ch, "
             << _config.framesPerBuffer << " frames/period";

    return WmaCode::Ok;
}

void AlsaAudioDevice::close() noexcept
{
    //! stop() joins the writer thread, so the active callback is no longer in
    //! use by the time it is dropped here.
    stop();
    _mixCallback.clear();

    if (_pcm)
    {
        snd_pcm_close(_pcm);
        _pcm = nullptr;
    }

    _scratch.clear();
    _scratch.shrink_to_fit();
}

WmaCode AlsaAudioDevice::start()
{
    if (!_pcm)
        return WmaCode::Error;

    if (_running.load(std::memory_order_acquire))
        return WmaCode::Ok;

    if (const int err = snd_pcm_prepare(_pcm); err < 0)
    {
        INK_WARN << "[wma] ALSA failed to prepare the PCM: " << snd_strerror(err);
        return WmaCode::Error;
    }

    _running.store(true, std::memory_order_release);
    _writer = std::jthread([this](std::stop_token token) { writerLoop(std::move(token)); });

    return WmaCode::Ok;
}

void AlsaAudioDevice::stop() noexcept
{
    if (!_writer.joinable())
    {
        _running.store(false, std::memory_order_release);
        return;
    }

    _writer.request_stop();

    //! Joined rather than interrupted: the thread can be parked inside
    //! snd_pcm_writei(), and ALSA's PCM handle is not safe to touch from here
    //! while it is. Waiting costs at most one period (tens of milliseconds) and
    //! is what makes the "callback is not running on return" postcondition
    //! true without racing the driver.
    _writer.join();

    //! Cleared only after the join, so isRunning() never reports false while a
    //! callback is still executing. Callers rely on that to decide when memory
    //! the mix callback reads can be freed; request_stop() is what ends the
    //! loop, so moving this later costs nothing.
    _running.store(false, std::memory_order_release);

    if (_pcm)
        snd_pcm_drop(_pcm);
}

bool AlsaAudioDevice::isRunning() const noexcept
{
    return _running.load(std::memory_order_acquire);
}

void AlsaAudioDevice::setMixCallback(AudioMixCallback callback)
{
    _mixCallback.store(std::move(callback));
}

const AudioDeviceConfig& AlsaAudioDevice::getConfig() const noexcept
{
    return _config;
}

AudioBackend AlsaAudioDevice::getBackendType() const noexcept
{
    return AudioBackend::Alsa;
}

void AlsaAudioDevice::writerLoop(std::stop_token stopToken)
{
    setWriterRealtimePriority();
    setWriterCoreAffinity();

    const snd_pcm_uframes_t periodFrames = _config.framesPerBuffer;

    while (!stopToken.stop_requested() && _running.load(std::memory_order_acquire))
    {
        const std::span<f32> block{_scratch.data(), _scratch.size()};

        //! Picks up a callback installed since the last period, and writes
        //! silence when none is installed -- the buffer is not pre-zeroed, and
        //! handing ALSA stale contents would replay the previous period.
        _mixCallback.invoke(block);

        snd_pcm_sframes_t written = snd_pcm_writei(_pcm, block.data(), periodFrames);

        if (written < 0)
        {
            //! -EPIPE is an underrun: the mix took longer than a period and the
            //! hardware ran dry. Recoverable, and normal under load, so the
            //! stream is restarted rather than abandoned. The `1` suppresses
            //! ALSA's own message -- a burst of underruns would otherwise
            //! flood the log with the very stalls it is reporting.
            written = snd_pcm_recover(_pcm, static_cast<int>(written), 1);

            if (written < 0)
            {
                INK_WARN << "[wma] ALSA write failed unrecoverably: "
                         << snd_strerror(static_cast<int>(written));
                break;
            }
        }
    }

    _running.store(false, std::memory_order_release);
}

} // namespace wma

#endif // WMA_ENABLE_ALSA
