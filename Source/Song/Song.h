#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace pianolume
{

/** パートの役割。表示色と、自動演奏するかどうかが決まる。 */
enum class PartRole : std::uint8_t
{
    rightHand,
    leftHand,
    accompaniment,
    mute
};

inline bool isHandRole (PartRole role) noexcept
{
    return role == PartRole::rightHand || role == PartRole::leftHand;
}

struct SongNote
{
    double start = 0.0;
    double end = 0.0;
    int note = 0;
    int velocity = 0;
    int part = 0;
};

struct SongEvent
{
    double time = 0.0;
    juce::MidiMessage message;
    int part = 0;
};

/** 落下エリアに引く小節線・拍線。 */
struct GridLine
{
    double time = 0.0;
    bool isMeasure = false;
    int measureNumber = 0;
};

/** MIDIファイルの「トラック × チャンネル」の組。 */
struct SongPart
{
    juce::String name;
    juce::String instrument;
    int track = 0;
    int channel = 1;
    int noteCount = 0;
    int lowestNote = 127;
    int highestNote = 0;
    PartRole defaultRole = PartRole::accompaniment;
};

/** 読み込んだMIDIファイル。時刻はすべて秒（テンポ変更を反映済み）。 */
struct Song
{
    static std::shared_ptr<Song> load (const juce::File& file, juce::String& errorMessage);

    juce::String title;
    juce::File file;

    std::vector<SongNote> notes;    // 開始時刻順
    std::vector<SongEvent> events;  // 再生用。時刻順、同時刻ならノートオフが先
    std::vector<SongPart> parts;
    std::vector<GridLine> gridLines; // 時刻順

    double duration = 0.0;
    double maxNoteLength = 0.0;
};

} // namespace pianolume
