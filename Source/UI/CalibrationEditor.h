#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Keyboard/KeyboardLayout.h"
#include "Projection/CalibrationModel.h"
#include "ToolButton.h"

namespace pianolume
{

/** 投影の合わせ込み画面。プロジェクタの画面を縮小表示して四隅を動かし、黒鍵の補正や明るさを調整する。 */
class CalibrationEditor : public juce::Component,
                          private juce::ChangeListener
{
public:
    CalibrationEditor (CalibrationModel& model, juce::File profilesFolder);
    ~CalibrationEditor() override;

    /** プロジェクタ画面の大きさ（縦横比と、矢印キーで動かす 1px の大きさに使う）。 */
    void setProjectorSize (juce::Point<int> size);

    /** 縮小表示に鍵の位置を描くために使う。 */
    void setLayout (const KeyboardLayout& layout);

    std::function<void()> onDone;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void visibilityChanged() override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    class Canvas;

    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void refreshFromModel();
    void applyBlackKeys();
    void applyOutput();

    void refreshProfiles();
    void loadProfile (const juce::String& name);
    void askProfileName();
    void saveProfile (const juce::String& name);
    void deleteSelectedProfile();
    juce::File getProfileFile (const juce::String& name) const;

    CalibrationModel& model;
    juce::File profilesFolder;

    std::unique_ptr<Canvas> canvas;

    juce::ToggleButton linkToggle { "Join fall area and keys" };
    juce::TextButton flipHorizontalButton { "Flip H" }, flipVerticalButton { "Flip V" };
    juce::TextButton rotateButton { "Rotate 180" }, resetButton { "Reset" };
    juce::TextButton squareFallButton { "Square 1-2-3-4" }, squareKeysButton { "Square 4-3-5-6" };

    juce::Slider blackOffsetLeftSlider, blackOffsetRightSlider, blackOffsetYSlider, blackLengthSlider;
    juce::Slider brightnessSlider, gammaSlider;

    juce::ComboBox profileBox;
    juce::TextButton saveProfileButton { "Save As..." }, deleteProfileButton { "Delete" };
    std::unique_ptr<juce::AlertWindow> nameDialog;

    ToolButton doneButton { "Done", ToolButton::Variant::primary };

    struct Label
    {
        juce::String text;
        juce::Rectangle<int> bounds;
        bool isSection;
    };

    std::vector<Label> labels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CalibrationEditor)
};

} // namespace pianolume
