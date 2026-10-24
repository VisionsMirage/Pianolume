#include "Icons.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace pianolume
{

namespace
{
    constexpr float viewBoxSize = 24.0f;

    Icon fromSvg (std::initializer_list<const char*> pathData, bool filled = false)
    {
        Icon icon;
        icon.filled = filled;

        for (const auto* data : pathData)
            icon.path.addPath (juce::Drawable::parseSVGPath (data));

        return icon;
    }
}

void Icon::draw (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour, float strokeWidth) const
{
    const auto scale = juce::jmin (area.getWidth(), area.getHeight()) / viewBoxSize;
    const auto transform = juce::AffineTransform::scale (scale)
                               .translated (area.getCentreX() - viewBoxSize * scale * 0.5f,
                                            area.getCentreY() - viewBoxSize * scale * 0.5f);

    g.setColour (colour);

    if (filled)
        g.fillPath (path, transform);
    else
        g.strokePath (path, juce::PathStrokeType (strokeWidth / scale, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded),
                      transform);
}

namespace Icons
{
    Icon folder()     { return fromSvg ({ "M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z" }); }

    Icon music()
    {
        auto icon = fromSvg ({ "M9 18V5l12-2v13" });
        icon.path.addEllipse (3.0f, 15.0f, 6.0f, 6.0f);
        icon.path.addEllipse (15.0f, 13.0f, 6.0f, 6.0f);
        return icon;
    }

    Icon layers()     { return fromSvg ({ "M12 3 3 8l9 5 9-5-9-5z", "m3 13 9 5 9-5" }); }

    Icon terminal()
    {
        auto icon = fromSvg ({ "m7 9 3 3-3 3", "M13 15h4" });
        icon.path.addRoundedRectangle (3.0f, 4.0f, 18.0f, 16.0f, 2.0f);
        return icon;
    }

    Icon projector()
    {
        auto icon = fromSvg ({ "M6 17v2M18 17v2M6 11h4" });
        icon.path.addRoundedRectangle (2.0f, 7.0f, 20.0f, 10.0f, 3.0f);
        icon.path.addEllipse (13.5f, 9.5f, 5.0f, 5.0f);
        return icon;
    }

    Icon settings()
    {
        auto icon = fromSvg ({ "M19.4 15a1.7 1.7 0 0 0 .3 1.8l.1.1a2 2 0 1 1-2.8 2.8l-.1-.1a1.7 1.7 0 0 0-1.8-.3 1.7 1.7 0 0 0-1 1.5V21a2 2 0 1 1-4 0v-.1a1.7 1.7 0 0 0-1.1-1.5 1.7 1.7 0 0 0-1.8.3l-.1.1a2 2 0 1 1-2.8-2.8l.1-.1a1.7 1.7 0 0 0 .3-1.8 1.7 1.7 0 0 0-1.5-1H3a2 2 0 1 1 0-4h.1a1.7 1.7 0 0 0 1.5-1.1 1.7 1.7 0 0 0-.3-1.8l-.1-.1a2 2 0 1 1 2.8-2.8l.1.1a1.7 1.7 0 0 0 1.8.3H9a1.7 1.7 0 0 0 1-1.5V3a2 2 0 1 1 4 0v.1a1.7 1.7 0 0 0 1 1.5 1.7 1.7 0 0 0 1.8-.3l.1-.1a2 2 0 1 1 2.8 2.8l-.1.1a1.7 1.7 0 0 0-.3 1.8V9a1.7 1.7 0 0 0 1.5 1H21a2 2 0 1 1 0 4h-.1a1.7 1.7 0 0 0-1.5 1z" });
        icon.path.addEllipse (9.0f, 9.0f, 6.0f, 6.0f);
        return icon;
    }

    Icon skipBack()   { return fromSvg ({ "M6 5v14", "M19 5 9 12l10 7z" }); }
    Icon play()       { return fromSvg ({ "M8 5v14l11-7z" }, true); }

    Icon pause()
    {
        Icon icon;
        icon.filled = true;
        icon.path.addRoundedRectangle (6.0f, 5.0f, 4.0f, 14.0f, 1.0f);
        icon.path.addRoundedRectangle (14.0f, 5.0f, 4.0f, 14.0f, 1.0f);
        return icon;
    }

    Icon loop()       { return fromSvg ({ "m17 2 4 4-4 4", "M3 11V9a3 3 0 0 1 3-3h15", "m7 22-4-4 4-4", "M21 13v2a3 3 0 0 1-3 3H3" }); }
    Icon close()      { return fromSvg ({ "M6 6l12 12M18 6 6 18" }); }
    Icon minus()      { return fromSvg ({ "M6 12h12" }); }
    Icon plus()       { return fromSvg ({ "M6 12h12M12 6v12" }); }
}

} // namespace pianolume
