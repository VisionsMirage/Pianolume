#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace pianolume
{

/** JUCE 標準部品（コンボボックス、スライダー、メニューなど）をデザイン案の見た目にそろえる。 */
class PianolumeLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PianolumeLookAndFeel();

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& font) override;

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                               bool isHighlighted, bool isDown) override;
    juce::Font getTextButtonFont (juce::TextButton& button, int buttonHeight) override;

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool isHighlighted, bool isDown) override;

    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox& box) override;
    juce::Font getComboBoxFont (juce::ComboBox& box) override;
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override;

    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override;
    juce::Font getPopupMenuFont() override;

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle style, juce::Slider& slider) override;
    juce::Label* createSliderTextBox (juce::Slider& slider) override;

    void fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor& editor) override;
    void drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& editor) override;

    void drawScrollbar (juce::Graphics& g, juce::ScrollBar& scrollbar, int x, int y, int width, int height,
                        bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                        bool isMouseOver, bool isMouseDown) override;
    int getDefaultScrollbarWidth() override { return 10; }
};

} // namespace pianolume
