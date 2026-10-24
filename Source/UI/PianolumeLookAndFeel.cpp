#include "PianolumeLookAndFeel.h"
#include "Theme.h"

namespace pianolume
{

namespace
{
    constexpr float controlRadius = 10.0f;
}

PianolumeLookAndFeel::PianolumeLookAndFeel()
{
    using namespace Theme::Colours;

    setColourScheme ({ background, panel, control, border, text, textMuted, text, primary, text });

    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::DocumentWindow::backgroundColourId, background);
    setColour (juce::Label::textColourId, text);

    setColour (juce::ComboBox::backgroundColourId, field);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, juce::Colours::white.withAlpha (0.10f));
    setColour (juce::ComboBox::arrowColourId, textMuted);

    setColour (juce::PopupMenu::backgroundColourId, panel);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, controlActive);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);

    setColour (juce::TextButton::buttonColourId, controlRaised);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, text);

    setColour (juce::ToggleButton::textColourId, text);
    setColour (juce::ToggleButton::tickColourId, onPrimary);

    setColour (juce::Slider::trackColourId, text);
    setColour (juce::Slider::backgroundColourId, juce::Colours::white.withAlpha (0.10f));
    setColour (juce::Slider::thumbColourId, juce::Colours::white);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);

    setColour (juce::TextEditor::backgroundColourId, panel);
    setColour (juce::TextEditor::textColourId, juce::Colour (0xffb4bbcb));
    setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextEditor::highlightColourId, controlActive);

    setColour (juce::ScrollBar::thumbColourId, juce::Colours::white.withAlpha (0.18f));
    setColour (juce::ListBox::backgroundColourId, panel);
}

juce::Typeface::Ptr PianolumeLookAndFeel::getTypefaceForFont (const juce::Font& font)
{
    const auto name = font.getTypefaceName();

    if (name == juce::Font::getDefaultMonospacedFontName())
        return Theme::getMonoTypeface();

    if (name == juce::Font::getDefaultSansSerifFontName())
        return Theme::getTypeface (font.isBold() ? 600 : 400);

    return LookAndFeel_V4::getTypefaceForFont (font);
}

void PianolumeLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                                                 bool isHighlighted, bool isDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    auto fill = backgroundColour;

    if (isHighlighted)
        fill = fill.brighter (0.12f);

    if (isDown)
        fill = fill.darker (0.15f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, controlRadius);
    g.setColour (juce::Colours::white.withAlpha (0.14f));
    g.drawRoundedRectangle (bounds, controlRadius, 1.0f);
}

juce::Font PianolumeLookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return Theme::uiFont (13.0f, 500);
}

void PianolumeLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool isHighlighted, bool)
{
    using namespace Theme::Colours;

    const auto boxSize = 18.0f;
    const auto bounds = button.getLocalBounds().toFloat();
    const juce::Rectangle<float> box (bounds.getX() + 1.0f, bounds.getCentreY() - boxSize * 0.5f, boxSize, boxSize);

    if (button.getToggleState())
    {
        g.setColour (juce::Colour (0xff4ade80));
        g.fillRoundedRectangle (box, 5.0f);

        juce::Path tick;
        tick.startNewSubPath (box.getX() + 4.5f, box.getCentreY());
        tick.lineTo (box.getX() + 7.8f, box.getBottom() - 5.0f);
        tick.lineTo (box.getRight() - 4.0f, box.getY() + 5.0f);
        g.setColour (onPrimary);
        g.strokePath (tick, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else
    {
        g.setColour (isHighlighted ? controlActive : field);
        g.fillRoundedRectangle (box, 5.0f);
        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.drawRoundedRectangle (box, 5.0f, 1.0f);
    }

    g.setColour (button.findColour (juce::ToggleButton::textColourId));
    g.setFont (Theme::uiFont (13.0f));
    g.drawText (button.getButtonText(), bounds.withTrimmedLeft (boxSize + 10.0f), juce::Justification::centredLeft);
}

void PianolumeLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                         int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);

    g.setColour (box.findColour (juce::ComboBox::backgroundColourId).brighter (box.isMouseOver (true) ? 0.08f : 0.0f));
    g.fillRoundedRectangle (bounds, controlRadius);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (bounds, controlRadius, 1.0f);

    // 右端の下向き山形
    const auto centreX = (float) width - 18.0f;
    const auto centreY = (float) height * 0.5f;
    juce::Path chevron;
    chevron.startNewSubPath (centreX - 4.0f, centreY - 2.0f);
    chevron.lineTo (centreX, centreY + 2.0f);
    chevron.lineTo (centreX + 4.0f, centreY - 2.0f);

    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.strokePath (chevron, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font PianolumeLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return Theme::uiFont (13.0f);
}

void PianolumeLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 36, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void PianolumeLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
    g.setColour (Theme::Colours::border);
    g.drawRect (0, 0, width, height);
}

juce::Font PianolumeLookAndFeel::getPopupMenuFont()
{
    return Theme::uiFont (13.0f);
}

void PianolumeLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPos, float minSliderPos, float maxSliderPos,
                                             juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        return;
    }

    const auto trackHeight = 4.0f;
    const auto centreY = (float) y + (float) height * 0.5f;
    const juce::Rectangle<float> track ((float) x, centreY - trackHeight * 0.5f, (float) width, trackHeight);

    g.setColour (slider.findColour (juce::Slider::backgroundColourId));
    g.fillRoundedRectangle (track, trackHeight * 0.5f);

    g.setColour (slider.findColour (juce::Slider::trackColourId));
    g.fillRoundedRectangle (track.withRight (sliderPos), trackHeight * 0.5f);

    const auto thumbSize = 14.0f;
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.fillEllipse (juce::Rectangle<float> (thumbSize + 8.0f, thumbSize + 8.0f).withCentre ({ sliderPos, centreY }));
    g.setColour (slider.findColour (juce::Slider::thumbColourId));
    g.fillEllipse (juce::Rectangle<float> (thumbSize, thumbSize).withCentre ({ sliderPos, centreY }));
}

juce::Label* PianolumeLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (Theme::monoFont (13.0f));
    label->setJustificationType (juce::Justification::centredRight);
    return label;
}

void PianolumeLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int, int, juce::TextEditor& editor)
{
    g.fillAll (editor.findColour (juce::TextEditor::backgroundColourId));
}

void PianolumeLookAndFeel::drawTextEditorOutline (juce::Graphics&, int, int, juce::TextEditor&)
{
}

void PianolumeLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar& scrollbar, int x, int y, int width, int height,
                                          bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                                          bool isMouseOver, bool isMouseDown)
{
    juce::Rectangle<int> thumb;

    if (isScrollbarVertical)
        thumb = { x + 3, thumbStartPosition, width - 6, thumbSize };
    else
        thumb = { thumbStartPosition, y + 3, thumbSize, height - 6 };

    auto colour = scrollbar.findColour (juce::ScrollBar::thumbColourId);

    if (isMouseOver || isMouseDown)
        colour = colour.brighter (0.3f);

    g.setColour (colour);
    g.fillRoundedRectangle (thumb.toFloat(), 3.0f);
}

} // namespace pianolume
