#include "SettingsPanel.h"
#include "Theme.h"

namespace pianolume
{

namespace
{
    constexpr int padding = 22;
    constexpr int labelWidth = 140;
    constexpr int rowHeight = 38;
    constexpr int rowGap = 12;
    constexpr int sectionGap = 22;
    constexpr int sectionHeaderHeight = 24;
    constexpr int footerHeight = 66;
}

SettingsPanel::SettingsPanel()
{
    for (auto* component : std::initializer_list<juce::Component*> {
             &midiBox, &keyboardBox, &lowestNoteBox, &detectButton,
             &screenKeysBox, &lookAheadSlider, &displayBox, &calibrateButton, &projectionGridToggle,
             &noteNamesToggle, &keyNamesBox,
             &synthToggle, &audioSettingsButton, &volumeSlider, &soundFontName, &loadSoundFontButton, &presetBox,
             &doneButton })
        addAndMakeVisible (component);

    midiBox.setTextWhenNoChoicesAvailable ("No MIDI devices");
    presetBox.setTextWhenNoChoicesAvailable ("No presets");

    // ID は NotesRenderer::KeyNames の値 + 1
    keyNamesBox.addItem ("Keys: none", 1);
    keyNamesBox.addItem ("Keys: white keys", 2);
    keyNamesBox.addItem ("Keys: black keys", 3);
    keyNamesBox.addItem ("Keys: C only", 4);

    for (auto* slider : { &volumeSlider, &lookAheadSlider })
    {
        slider->setSliderStyle (juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
    }

    volumeSlider.setTextValueSuffix (" dB");
    volumeSlider.setNumDecimalPlacesToDisplay (1);

    lookAheadSlider.setRange (1.0, 6.0, 0.1);
    lookAheadSlider.setTextValueSuffix (" s");
    lookAheadSlider.setDoubleClickReturnValue (true, 2.5);

    soundFontName.setFont (Theme::uiFont (13.0f));
    soundFontName.setColour (juce::Label::backgroundColourId, Theme::Colours::field);
    soundFontName.setBorderSize ({ 0, 12, 0, 12 });

    doneButton.setButtonText ("Done");

    setSize (660, 750);
}

void SettingsPanel::paint (juce::Graphics& g)
{
    using namespace Theme::Colours;

    g.fillAll (juce::Colour (0xff11141b));

    for (const auto& [title, y] : sectionHeaders)
    {
        g.setColour (textMuted);
        g.setFont (Theme::uiFont (11.0f, 600));
        g.drawText (title, padding, y, getWidth() - padding * 2, sectionHeaderHeight, juce::Justification::centredLeft, false);
    }

    for (const auto& row : rowLabels)
    {
        g.setColour (textSecondary);
        g.setFont (Theme::uiFont (13.0f));
        g.drawText (row.label, padding, row.y, labelWidth, rowHeight, juce::Justification::centredLeft, false);
    }

    g.setColour (divider);
    g.fillRect (0, getHeight() - footerHeight, getWidth(), 1);
}

void SettingsPanel::resized()
{
    sectionHeaders.clear();
    rowLabels.clear();

    int y = padding - 6;

    const auto section = [&] (const juce::String& title)
    {
        sectionHeaders.push_back ({ title, y });
        y += sectionHeaderHeight + 6;
    };

    const auto row = [&] (const juce::String& label)
    {
        rowLabels.push_back ({ label, y });
        const juce::Rectangle<int> bounds (padding + labelWidth + 16, y, getWidth() - padding * 2 - labelWidth - 16, rowHeight);
        y += rowHeight + rowGap;
        return bounds;
    };

    const auto splitRight = [] (juce::Rectangle<int>& bounds, int width)
    {
        auto right = bounds.removeFromRight (width);
        bounds.removeFromRight (10);
        return right;
    };

    section ("INPUT KEYBOARD");
    midiBox.setBounds (row ("MIDI Input"));

    {
        auto bounds = row ("Keyboard");
        const auto third = (bounds.getWidth() - 16) / 3;
        keyboardBox.setBounds (bounds.removeFromLeft (third));
        bounds.removeFromLeft (8);
        lowestNoteBox.setBounds (bounds.removeFromLeft (third));
        bounds.removeFromLeft (8);
        detectButton.setBounds (bounds);
    }

    y += sectionGap - rowGap;
    section ("DISPLAY");
    screenKeysBox.setBounds (row ("Screen Keys"));
    lookAheadSlider.setBounds (row ("Fall Time"));

    {
        auto bounds = row ("Projector");
        calibrateButton.setBounds (splitRight (bounds, 120));
        displayBox.setBounds (bounds);
    }

    {
        auto bounds = row ({});
        projectionGridToggle.setBounds (bounds);
    }

    {
        auto bounds = row ("Note Names");
        noteNamesToggle.setBounds (bounds.removeFromLeft (bounds.getWidth() / 2));
        keyNamesBox.setBounds (bounds);
    }

    y += sectionGap - rowGap;
    section ("SOUND");

    {
        auto bounds = row ("Internal Synth");
        audioSettingsButton.setBounds (splitRight (bounds, 140));
        synthToggle.setBounds (bounds);
    }

    volumeSlider.setBounds (row ("Volume"));

    {
        auto bounds = row ("SoundFont");
        loadSoundFontButton.setBounds (splitRight (bounds, 90));
        soundFontName.setBounds (bounds);
    }

    presetBox.setBounds (row ("Preset"));

    const auto footer = getLocalBounds().removeFromBottom (footerHeight).reduced (padding, 14);
    doneButton.setBounds (footer.withLeft (footer.getRight() - doneButton.getIdealWidth (38) - 12));
}

} // namespace pianolume
