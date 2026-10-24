#include "KeyboardLayout.h"

namespace pianolume
{

namespace
{
    struct Preset
    {
        int numKeys;
        int lowestNote;
    };

    // 一般的な製品の音域。オクターブシフトがある機種は鍵盤検出で合わせる
    constexpr Preset presets[] = {
        { 25, 48 }, // C3 - C5
        { 37, 48 }, // C3 - C6（iRig Keys 37 Pro の初期状態）
        { 49, 36 }, // C2 - C6
        { 61, 36 }, // C2 - C7
        { 76, 28 }, // E1 - G7
        { 88, 21 }, // A0 - C8
    };
}

KeyboardLayout::KeyboardLayout (int numKeysToUse, int lowestNoteToUse)
    : numKeys (juce::jlimit (1, 128, numKeysToUse)),
      lowestNote (juce::jlimit (0, 128 - numKeys, lowestNoteToUse))
{
}

KeyboardLayout KeyboardLayout::fromPreset (int numKeysToUse)
{
    for (const auto& preset : presets)
        if (preset.numKeys == numKeysToUse)
            return { preset.numKeys, preset.lowestNote };

    jassertfalse;
    return {};
}

juce::Array<int> KeyboardLayout::getPresetKeyCounts()
{
    juce::Array<int> counts;

    for (const auto& preset : presets)
        counts.add (preset.numKeys);

    return counts;
}

int KeyboardLayout::getNumWhiteKeys() const
{
    return countWhiteKeysBelow (getHighestNote() + 1);
}

juce::Range<float> KeyboardLayout::getKeyRange (int note) const
{
    const auto whiteIndex = (float) countWhiteKeysBelow (note);

    // 黒鍵は左右の白鍵の境界を中心に置く
    if (isBlackKey (note))
        return { whiteIndex - blackKeyWidth * 0.5f, whiteIndex + blackKeyWidth * 0.5f };

    return { whiteIndex, whiteIndex + 1.0f };
}

bool KeyboardLayout::isBlackKey (int note) noexcept
{
    switch (note % 12)
    {
        case 1: case 3: case 6: case 8: case 10:
            return true;
        default:
            return false;
    }
}

int KeyboardLayout::countWhiteKeysBelow (int note) const
{
    int count = 0;

    for (int n = lowestNote; n < note; ++n)
        if (! isBlackKey (n))
            ++count;

    return count;
}

} // namespace pianolume
