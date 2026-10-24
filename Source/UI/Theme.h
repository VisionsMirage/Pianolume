#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace pianolume::Theme
{

/** 画面全体の配色（デザイン案「Midnight Stage」）。 */
namespace Colours
{
    inline const juce::Colour background     { 0xff0a0c11 };
    inline const juce::Colour panel          { 0xff0e1117 };
    inline const juce::Colour control        { 0xff161a22 };
    inline const juce::Colour controlRaised  { 0xff222838 };
    inline const juce::Colour controlActive  { 0xff232938 };
    inline const juce::Colour field          { 0xff181c25 };
    inline const juce::Colour overlay        { 0xf011141b };
    inline const juce::Colour border         { 0x14ffffff };
    inline const juce::Colour borderStrong   { 0x59ffffff };
    inline const juce::Colour divider        { 0x0fffffff };

    inline const juce::Colour text           { 0xffe7e9ee };
    inline const juce::Colour textSecondary  { 0xffc7ccd8 };
    inline const juce::Colour textMuted      { 0xff9aa3b5 };
    inline const juce::Colour textFaint      { 0xff7d8597 };
    inline const juce::Colour primary        { 0xfff2f4f8 };
    inline const juce::Colour onPrimary      { 0xff0a0c11 };

    inline const juce::Colour loop           { 0xfffbbf24 };
    inline const juce::Colour loopText       { 0xfffcd34d };

    inline const juce::Colour stage          { 0xff07090d };
    inline const juce::Colour felt           { 0xff5a1717 };
    inline const juce::Colour feltEdge       { 0xff7a2323 };
}

/** 本文用（Inter）。height は CSS の font-size と同じ意味。weight は 400 / 500 / 600 / 700。 */
juce::Font uiFont (float height, int weight = 400);

/** 時間表示・ログ用（JetBrains Mono）。 */
juce::Font monoFont (float height);

juce::Typeface::Ptr getTypeface (int weight);
juce::Typeface::Ptr getMonoTypeface();

} // namespace pianolume::Theme
