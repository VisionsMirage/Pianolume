#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <juce_events/juce_events.h>

namespace pianolume
{

/** 押されている鍵とペダルの状態。MIDIスレッドから書き込み、描画スレッドから読む。

    状態が変わるとメッセージスレッドへ変更通知を送るので、表示側はポーリングせずに即座に再描画できる。
*/
class KeyState : public juce::ChangeBroadcaster
{
public:
    void setNoteOn (int note, int velocity) noexcept
    {
        if (isValid (note))
        {
            velocities[(size_t) note].store ((std::uint8_t) (velocity > 0 ? velocity : 1), std::memory_order_relaxed);
            sendChangeMessage();
        }
    }

    void setNoteOff (int note) noexcept
    {
        if (isValid (note))
        {
            velocities[(size_t) note].store (0, std::memory_order_relaxed);
            sendChangeMessage();
        }
    }

    bool isDown (int note) const noexcept
    {
        return isValid (note) && velocities[(size_t) note].load (std::memory_order_relaxed) > 0;
    }

    int getVelocity (int note) const noexcept
    {
        return isValid (note) ? velocities[(size_t) note].load (std::memory_order_relaxed) : 0;
    }

    void setSustain (bool isOn) noexcept
    {
        sustain.store (isOn, std::memory_order_relaxed);
        sendChangeMessage();
    }

    bool isSustainOn() const noexcept       { return sustain.load (std::memory_order_relaxed); }

    void clear() noexcept
    {
        for (auto& v : velocities)
            v.store (0, std::memory_order_relaxed);

        sustain.store (false, std::memory_order_relaxed);
        sendChangeMessage();
    }

private:
    static constexpr bool isValid (int note) noexcept { return note >= 0 && note < 128; }

    std::array<std::atomic<std::uint8_t>, 128> velocities {};
    std::atomic<bool> sustain { false };
};

} // namespace pianolume
