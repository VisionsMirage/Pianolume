#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "ToolButton.h"

namespace pianolume
{

/** 画面下部のログ。表示・非表示は MainComponent が切り替える。 */
class LogPanel : public juce::Component
{
public:
    LogPanel();

    /** 時刻を付けて 1 行追加する。 */
    void append (const juce::String& text);

    std::function<void()> onHide;

    static constexpr int preferredHeight = 132;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    ToolButton hideButton { "Hide", ToolButton::Variant::ghost };
    juce::TextEditor editor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LogPanel)
};

} // namespace pianolume
