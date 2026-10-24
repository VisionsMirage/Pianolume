#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "ToolButton.h"

namespace pianolume
{

/** 画面上部の 1 段目。ロゴ、曲を開く、再生中の曲、再生モード、速度、パネル切り替えのボタンを並べる。 */
class HeaderBar : public juce::Component
{
public:
    struct Mode
    {
        int id;
        juce::String label;
    };

    HeaderBar (std::vector<Mode> modes, std::vector<int> speedPercents);
    ~HeaderBar() override;

    void setNowPlaying (const juce::String& title, const juce::String& details);
    void setMode (int modeId);
    void setSpeedPercent (int percent);
    void setPartsVisible (bool isVisible)       { partsButton.setToggleState (isVisible, juce::dontSendNotification); }
    void setLogVisible (bool isVisible)         { logButton.setToggleState (isVisible, juce::dontSendNotification); }
    void setProjectionActive (bool isActive)    { projectionButton.setToggleState (isActive, juce::dontSendNotification); }

    std::function<void()> onOpen;
    std::function<void (int modeId)> onModeChanged;
    std::function<void (int percent)> onSpeedChanged;
    std::function<void()> onPartsClicked;
    std::function<void()> onLogClicked;
    std::function<void()> onProjectionClicked;
    std::function<void()> onSettingsClicked;

    static constexpr int preferredHeight = 60;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    class ModeSelector;
    class SpeedStepper;

    juce::Rectangle<int> logoArea, dividerArea, nowPlayingArea;
    juce::String nowPlayingTitle, nowPlayingDetails;

    ToolButton openButton { "Open", ToolButton::Variant::secondary };
    std::unique_ptr<ModeSelector> modeSelector;
    std::unique_ptr<SpeedStepper> speedStepper;
    ToolButton partsButton { "Parts" }, logButton { "Log" }, projectionButton { "Projection" }, settingsButton { "Settings" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderBar)
};

} // namespace pianolume
