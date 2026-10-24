#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace pianolume
{

/** 内蔵音源のチャンネル割り当て。0〜15 は曲のチャンネルをそのまま使い、16 以降は演奏用の音色で鳴らす。 */
namespace SynthChannels
{
    constexpr int numSongChannels = 16;
    constexpr int live = 16;       // MIDIキーボードの演奏
    constexpr int songHands = 17;  // 曲の右手・左手パートの自動演奏
    constexpr int total = 32;      // FluidSynth のチャンネル数（16 の倍数）
}

/** オーディオブロック内で発音する MIDI イベント。チャンネルは FluidSynth のチャンネル番号（0 始まり、16 以上も可）。 */
struct ScheduledMidiEvent
{
    int samplePosition = 0;
    int order = 0;
    juce::MidiMessage message;
    int synthChannel = 0;
    bool allowProgramChange = false;
};

/** オーディオスレッドで確保が起きないよう、事前に確保した容量を超えるイベントは捨てる。 */
inline void appendEvent (std::vector<ScheduledMidiEvent>& events, int samplePosition,
                         const juce::MidiMessage& message, int synthChannel, bool allowProgramChange)
{
    if (events.size() < events.capacity())
        events.push_back ({ samplePosition, (int) events.size(), message, synthChannel, allowProgramChange });
}

} // namespace pianolume
