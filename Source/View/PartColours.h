#pragma once

#include <juce_graphics/juce_graphics.h>
#include "Song/Song.h"

namespace pianolume::PartColours
{

/** パートの役割ごとの色。黒鍵のノートは少し暗くして白鍵と区別する。 */
inline juce::Colour forRole (PartRole role, bool isBlackKey = false)
{
    switch (role)
    {
        case PartRole::rightHand:     return isBlackKey ? juce::Colour (0xff16a34a) : juce::Colour (0xff4ade80);
        case PartRole::leftHand:      return isBlackKey ? juce::Colour (0xff2563eb) : juce::Colour (0xff60a5fa);
        case PartRole::accompaniment: return isBlackKey ? juce::Colour (0xff6b7280) : juce::Colour (0xff9ca3af);
        case PartRole::mute:          break;
    }

    return juce::Colours::transparentBlack;
}

/** ガイドの無い鍵を押したとき（再生中のミス）。 */
inline const juce::Colour mistake { 0xffef4444 };

/** 曲を再生していないときに押した鍵。 */
inline const juce::Colour freePlay { 0xfffbbf24 };

} // namespace pianolume::PartColours
