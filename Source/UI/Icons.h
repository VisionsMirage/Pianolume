#pragma once

#include <juce_graphics/juce_graphics.h>

namespace pianolume
{

/** 24 x 24 の座標で描いたアイコン。filled でなければ線で描く。 */
struct Icon
{
    juce::Path path;
    bool filled = false;

    bool isEmpty() const { return path.isEmpty(); }

    /** area に収まるように拡大して描く。 */
    void draw (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour, float strokeWidth = 1.8f) const;
};

namespace Icons
{
    Icon folder();
    Icon music();
    Icon layers();
    Icon terminal();
    Icon projector();
    Icon settings();
    Icon skipBack();
    Icon play();
    Icon pause();
    Icon loop();
    Icon close();
    Icon minus();
    Icon plus();
}

} // namespace pianolume
