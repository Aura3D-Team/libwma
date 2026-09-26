/**
 * @file test_mix_callback_slot.cpp
 * @brief Covers wma::MixCallbackSlot, the handover that lets setMixCallback()
 *        be called on a running device.
 *
 * The single-threaded cases below are cheap sanity checks; the point of this
 * file is the stress test, which reproduces the exact shape of the race the
 * slot exists to remove -- one thread replacing the callback while another
 * invokes it -- and is meant to be run under ThreadSanitizer.
 */
#include "wma/audio/MixCallbackSlot.hpp"

#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

namespace
{

int g_failures = 0;

void expect(bool condition, const char *name)
{
    if (condition)
    {
        std::printf("ok   %s\n", name);
        return;
    }
    std::printf("FAIL %s\n", name);
    ++g_failures;
}

//! A callback that stamps every sample with @p marker, so the audio side can
//! tell which generation of callback produced a buffer.
[[nodiscard]] wma::AudioMixCallback stamping(f32 marker)
{
    return [marker](std::span<f32> out)
    {
        for (f32 &sample : out)
            sample = marker;
    };
}

void testSilenceWithNoCallback()
{
    wma::MixCallbackSlot slot;
    std::vector<f32> buffer(8, 1234.0f);

    slot.invoke(std::span<f32>{buffer});

    bool allZero = true;
    for (const f32 sample : buffer)
        allZero = allZero && (sample == 0.0f);

    //! Not merely "left alone": an unfilled buffer replays the previous period.
    expect(allZero, "no callback installed writes silence");
}

void testStoreTakesEffect()
{
    wma::MixCallbackSlot slot;
    std::vector<f32> buffer(8, 0.0f);

    slot.store(stamping(7.0f));
    slot.invoke(std::span<f32>{buffer});

    expect(buffer[0] == 7.0f && buffer[7] == 7.0f, "stored callback is picked up");
}

void testReplaceAndClear()
{
    wma::MixCallbackSlot slot;
    std::vector<f32> buffer(8, 0.0f);

    slot.store(stamping(1.0f));
    slot.invoke(std::span<f32>{buffer});
    slot.store(stamping(2.0f));
    slot.invoke(std::span<f32>{buffer});

    expect(buffer[0] == 2.0f, "replacement callback supersedes the first");

    slot.clear();
    slot.invoke(std::span<f32>{buffer});

    expect(buffer[0] == 0.0f, "clear() returns the slot to silence");
}

/**
 * @brief Hammers store() against invoke() from two threads.
 *
 * Every callback fills the whole buffer with one marker, so a buffer holding
 * two different values would mean a swap happened mid-invocation. Markers are
 * issued in increasing order, so a marker that goes backwards would mean a
 * stale callback was resurrected. Neither can happen if the handover is
 * correct; both are silent corruption if it is not.
 *
 * Under ThreadSanitizer this also fails outright on the underlying data race,
 * which is the failure the original unsynchronized member would have produced.
 */
void testConcurrentReplacement()
{
    constexpr int kGenerations = 20000;

    wma::MixCallbackSlot slot;
    std::atomic<bool> done{false};
    std::atomic<bool> torn{false};
    std::atomic<bool> wentBackwards{false};
    std::atomic<int> highestSeen{0};

    std::thread audioThread(
        [&]
        {
            std::vector<f32> buffer(64, 0.0f);
            f32 previous = 0.0f;

            while (!done.load(std::memory_order_relaxed))
            {
                slot.invoke(std::span<f32>{buffer});

                const f32 first = buffer[0];
                for (const f32 sample : buffer)
                {
                    if (sample != first)
                        torn.store(true, std::memory_order_relaxed);
                }

                if (first < previous)
                    wentBackwards.store(true, std::memory_order_relaxed);
                previous = first;

                highestSeen.store(static_cast<int>(first), std::memory_order_relaxed);
            }
        });

    for (int generation = 1; generation <= kGenerations; ++generation)
        slot.store(stamping(static_cast<f32>(generation)));

    done.store(true, std::memory_order_relaxed);
    audioThread.join();

    expect(!torn.load(), "no buffer is filled by two callbacks at once");
    expect(!wentBackwards.load(), "no superseded callback is resurrected");

    //! Progress: try_lock may lose a round, but not every round for 20k stores.
    expect(highestSeen.load() > 0, "swaps actually reach the audio thread");
}

} // namespace

int main()
{
    testSilenceWithNoCallback();
    testStoreTakesEffect();
    testReplaceAndClear();
    testConcurrentReplacement();

    if (g_failures > 0)
    {
        std::printf("\n%d failure(s)\n", g_failures);
        return 1;
    }

    std::printf("\nall passed\n");
    return 0;
}
