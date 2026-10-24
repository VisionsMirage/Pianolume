#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "ToolButton.h"

namespace pianolume
{

/** 設定ダイアログの中身。部品の配置だけを受け持ち、動作は MainComponent が結び付ける。 */
class SettingsPanel : public juce::Component
{
public:
    SettingsPanel();

    void paint (juce::Graphics& g) override;
    void resized() override;

    // 入力鍵盤（実際のキーボード）
    juce::ComboBox midiBox, keyboardBox, lowestNoteBox;
    juce::TextButton detectButton { "Detect" };

    // 表示
    juce::ComboBox screenKeysBox;
    juce::Slider lookAheadSlider;
    juce::ComboBox displayBox;
    juce::TextButton calibrateButton { "Calibrate..." };
    juce::ToggleButton projectionGridToggle { "Show bar lines on projection" };
    juce::ToggleButton noteNamesToggle { "On falling notes" };
    juce::ComboBox keyNamesBox;

    // 音
    juce::ToggleButton synthToggle { "On" };
    juce::TextButton audioSettingsButton { "Audio Device..." };
    juce::Slider volumeSlider;
    juce::Label soundFontName;
    juce::TextButton loadSoundFontButton { "Load..." };
    juce::ComboBox presetBox;

    ToolButton doneButton { "Done", ToolButton::Variant::primary };

private:
    struct Row
    {
        juce::String label;
        int y = 0;
    };

    std::vector<std::pair<juce::String, int>> sectionHeaders;
    std::vector<Row> rowLabels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsPanel)
};

} // namespace pianolume
