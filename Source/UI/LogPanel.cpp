#include "LogPanel.h"
#include "Theme.h"

namespace pianolume
{

namespace
{
    constexpr int headerHeight = 32;
    constexpr int maxLogLength = 20000;
}

LogPanel::LogPanel()
{
    addAndMakeVisible (hideButton);
    hideButton.setButtonText ("Hide");
    hideButton.setCornerRadius (6.0f);
    hideButton.onClick = [this] { if (onHide != nullptr) onHide(); };

    addAndMakeVisible (editor);
    editor.setMultiLine (true);
    editor.setReadOnly (true);
    editor.setCaretVisible (false);
    editor.setScrollbarsShown (true);
    editor.setWantsKeyboardFocus (false);
    editor.setFont (Theme::monoFont (12.0f));
}

void LogPanel::append (const juce::String& text)
{
    if (editor.getTotalNumChars() > maxLogLength)
        editor.setText (editor.getText().substring (maxLogLength / 2), false);

    editor.moveCaretToEnd();
    editor.insertTextAtCaret (juce::Time::getCurrentTime().formatted ("%H:%M:%S") + "  " + text + "\n");
}

void LogPanel::paint (juce::Graphics& g)
{
    using namespace Theme::Colours;

    g.fillAll (panel);
    g.setColour (divider);
    g.fillRect (getLocalBounds().removeFromTop (1));

    g.setColour (textMuted);
    g.setFont (Theme::uiFont (11.0f, 600));
    g.drawText ("LOG", getLocalBounds().removeFromTop (headerHeight).reduced (16, 0), juce::Justification::centredLeft, false);
}

void LogPanel::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop (headerHeight).reduced (10, 4);
    hideButton.setBounds (header.removeFromRight (hideButton.getIdealWidth (24) - 12));

    editor.setBounds (area.reduced (10, 0).withTrimmedBottom (8));
}

} // namespace pianolume
