#ifndef WMA_AUDIO_MIX_CALLBACK_SLOT_HPP
#define WMA_AUDIO_MIX_CALLBACK_SLOT_HPP

#include <algorithm>
#include <atomic>
#include <mutex>
#include <span>

#include "AudioTypes.hpp"

namespace wma
{

/**
 * @brief Carries the mix callback across the control/audio thread boundary.
 *
 * Backends that own an audio thread (AlsaAudioDevice, SDLAudioDevice) hold
 * one of these instead of a bare AudioMixCallback. Without it,
 * setMixCallback() on a running device is a data race: it writes the
 * callback while the audio thread is reading and invoking it, and destroys
 * the old one's captured state out from under a thread that may be inside
 * it.
 *
 * Two properties matter, and an ordinary mutex delivers neither:
 *
 * - The audio thread never waits. It takes the lock with try_lock and, if
 *   the control thread happens to hold it, simply keeps using the callback
 *   it already has and picks the new one up a period later. A missed swap
 *   is invisible; a blocked audio thread is an audible dropout.
 *
 * - The audio thread never frees. Releasing a callback's captured state is
 *   unbounded work (it may free, or run arbitrary destructors), so the
 *   replaced callback is parked in a retired slot and reclaimed by the next
 *   store() on the control thread.
 *
 * NullAudioDevice deliberately does not use this: it has no audio thread,
 * so its callback is only ever touched by its caller.
 */
class MixCallbackSlot
{
  public:
    MixCallbackSlot() = default;

    MixCallbackSlot(const MixCallbackSlot &) = delete;
    MixCallbackSlot &operator=(const MixCallbackSlot &) = delete;

    /**
     * @brief Publishes @p callback, from any thread but the audio thread.
     *
     * Takes effect at the start of one of the next few periods, not
     * instantly: the audio thread performs the swap itself. Safe to call
     * while the device is running.
     */
    void store(AudioMixCallback callback)
    {
        const std::lock_guard lock(_mutex);

        //! Reclaim what the audio thread retired. This is the only place
        //! the previous callback is destroyed, and it is on this thread.
        _retired = {};

        _pending = std::move(callback);
        _hasPending.store(true, std::memory_order_release);
    }

    /**
     * @brief Fills @p output on the audio thread, swapping in a pending
     *        callback first if one is waiting and the lock is free.
     *
     * Writes silence when no callback is installed -- a normal state for a
     * game that has loaded no audio yet. Leaving @p output untouched would
     * replay whatever the previous period held.
     */
    void invoke(std::span<f32> output)
    {
        if (_hasPending.load(std::memory_order_acquire)) [[unlikely]]
        {
            std::unique_lock lock(_mutex, std::try_to_lock);
            if (lock.owns_lock())
            {
                //! _retired is empty here: store() clears it under this
                //! same lock before it ever sets _hasPending.
                _retired = std::move(_active);
                _active = std::move(_pending);
                _pending = {};
                _hasPending.store(false, std::memory_order_release);
            }
        }

        if (_active)
            _active(output);
        else
            std::ranges::fill(output, 0.0f);
    }

    /**
     * @brief Drops every callback held.
     *
     * @warning Only valid once the audio thread is known to have stopped --
     *          after a join, or after the backend guarantees its callback
     *          has finished. This touches the active slot, which is
     *          otherwise the audio thread's alone.
     */
    void clear() noexcept
    {
        const std::lock_guard lock(_mutex);
        _active = {};
        _pending = {};
        _retired = {};
        _hasPending.store(false, std::memory_order_release);
    }

  private:
    //! Guards _pending and _retired. Uncontended in the steady state: it is
    //! taken only when a swap is actually outstanding.
    std::mutex _mutex;

    //! Audio thread only, except under clear().
    AudioMixCallback _active{};

    AudioMixCallback _pending{};
    AudioMixCallback _retired{};
    std::atomic<bool> _hasPending{false};
};

} // namespace wma

#endif // WMA_AUDIO_MIX_CALLBACK_SLOT_HPP
