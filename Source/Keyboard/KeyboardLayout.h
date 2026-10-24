#pragma once

#include <juce_core/juce_core.h>

namespace pianolume
{

/** 鍵盤の範囲（鍵数と最低音）と、白鍵幅を 1.0 とした正規化座標での鍵の位置を表す。 */
class KeyboardLayout
{
public:
    KeyboardLayout (int numKeys = 37, int lowestNote = 48);

    /** 25 / 37 / 49 / 61 / 76 / 88 鍵のプリセットを返す。 */
    static KeyboardLayout fromPreset (int numKeys);
    static juce::Array<int> getPresetKeyCounts();

    int getNumKeys() const noexcept       { return numKeys; }
    int getLowestNote() const noexcept    { return lowestNote; }
    int getHighestNote() const noexcept   { return lowestNote + numKeys - 1; }
    bool contains (int note) const noexcept { return note >= lowestNote && note <= getHighestNote(); }

    int getNumWhiteKeys() const;

    /** 鍵の左端と右端（白鍵幅 = 1.0）。 */
    juce::Range<float> getKeyRange (int note) const;

    static bool isBlackKey (int note) noexcept;

    /** 白鍵幅に対する黒鍵幅の比率。 */
    static constexpr float blackKeyWidth = 0.6f;
    /** 白鍵の長さに対する黒鍵の長さの比率。 */
    static constexpr float blackKeyLength = 0.62f;

private:
    int countWhiteKeysBelow (int note) const;

    int numKeys;
    int lowestNote;
};

} // namespace pianolume
