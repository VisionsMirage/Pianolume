#include "ToolButton.h"
#include "Theme.h"

namespace pianolume
{

namespace
{
    constexpr float iconTextGap = 8.0f;
    constexpr float horizontalPadding = 14.0f;
}

ToolButton::ToolButton (const juce::String& name, Variant variantToUse, Icon iconToUse)
    : juce::Button (name),
      variant (variantToUse),
      icon (std::move (iconToUse))
{
    // 名前は読み上げ用のタイトルにだけ使い、表示する文字は setButtonText で明示的に設定する
    setTitle (name);
    setButtonText ({});

    setWantsKeyboardFocus (false);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void ToolButton::setIcon (Icon newIcon)
{
    icon = std::move (newIcon);
    repaint();
}

juce::Font ToolButton::getTextFont() const
{
    switch (variant)
    {
        case Variant::loopMarker: return Theme::monoFont (12.0f);
        case Variant::loop:       return Theme::uiFont (12.0f, 600);
        case Variant::primary:    return Theme::uiFont (13.0f, 600);
        default:                  return Theme::uiFont (13.0f, 500);
    }
}

int ToolButton::getIdealWidth (int height) const
{
    const auto label = getButtonText();

    if (label.isEmpty())
        return height;

    auto width = juce::GlyphArrangement::getStringWidth (getTextFont(), label) + horizontalPadding * 2.0f;

    if (! icon.isEmpty())
        width += iconSize + iconTextGap;

    return juce::roundToInt (std::ceil (width));
}

void ToolButton::paintButton (juce::Graphics& g, bool isHighlighted, bool isDown)
{
    using namespace Theme::Colours;

    auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    const auto isToggled = getToggleState();

    juce::Colour fill = juce::Colours::transparentBlack;
    juce::Colour outline = juce::Colours::transparentBlack;
    juce::Colour foreground = textSecondary;

    switch (variant)
    {
        case Variant::secondary:
            fill = isToggled ? controlActive : control;
            outline = isToggled ? borderStrong : border;
            foreground = isToggled ? juce::Colours::white : textSecondary;
            break;

        case Variant::ghost:
            fill = isHighlighted ? juce::Colours::white.withAlpha (0.06f) : juce::Colours::transparentBlack;
            foreground = textSecondary;
            break;

        case Variant::primary:
        case Variant::primaryCircle:
            fill = primary;
            foreground = onPrimary;
            break;

        case Variant::loop:
            fill = isToggled ? loop.withAlpha (0.18f) : control;
            outline = isToggled ? loop.withAlpha (0.7f) : juce::Colours::white.withAlpha (0.10f);
            foreground = isToggled ? loopText : textSecondary;
            break;

        case Variant::loopMarker:
            fill = loop.withAlpha (0.08f);
            outline = loop.withAlpha (0.35f);
            foreground = loopText;
            break;
    }

    if (isHighlighted && variant != Variant::ghost)
        fill = fill.brighter (0.12f);

    if (isDown)
        fill = fill.darker (0.15f);

    if (! isEnabled())
        foreground = foreground.withMultipliedAlpha (0.4f);

    if (variant == Variant::primaryCircle)
    {
        const auto circle = bounds.withSizeKeepingCentre (juce::jmin (bounds.getWidth(), bounds.getHeight()),
                                                          juce::jmin (bounds.getWidth(), bounds.getHeight()));
        g.setColour (fill);
        g.fillEllipse (circle);
    }
    else
    {
        g.setColour (fill);
        g.fillRoundedRectangle (bounds, cornerRadius);

        if (! outline.isTransparent())
        {
            g.setColour (outline);
            g.drawRoundedRectangle (bounds, cornerRadius, 1.0f);
        }
    }

    // アイコンと文字を中央にまとめて置く
    const auto label = getButtonText();
    const auto font = getTextFont();
    const auto textWidth = label.isEmpty() ? 0.0f : juce::GlyphArrangement::getStringWidth (font, label);
    const auto hasIcon = ! icon.isEmpty();
    const auto contentWidth = (hasIcon ? iconSize : 0.0f) + (hasIcon && label.isNotEmpty() ? iconTextGap : 0.0f) + textWidth;
    auto x = bounds.getCentreX() - contentWidth * 0.5f;

    if (hasIcon)
    {
        icon.draw (g, { x, bounds.getCentreY() - iconSize * 0.5f, iconSize, iconSize }, foreground);
        x += iconSize + iconTextGap;
    }

    if (label.isNotEmpty())
    {
        g.setColour (foreground);
        g.setFont (font);
        g.drawText (label, juce::Rectangle<float> (x, bounds.getY(), textWidth + 1.0f, bounds.getHeight()),
                    juce::Justification::centredLeft, false);
    }
}

} // namespace pianolume
