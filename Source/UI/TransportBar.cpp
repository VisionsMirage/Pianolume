#include "TransportBar.h"
#include "Theme.h"

namespace pianolume
{

//==============================================================================
/** 再生位置のバー。ループ区間を帯と目印で示し、クリック・ドラッグで移動する。 */
class TransportBar::Timeline : public juce::Component
{
public:
    Timeline()
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTitle ("Playback position");
    }

    void setPosition (double newPosition, double newDuration)
    {
        if (! isDragging)
            position = newPosition;

        duration = newDuration;
        repaint();
    }

    void setLoop (double start, double end, bool enabled)
    {
        loopStart = start;
        loopEnd = end;
        loopEnabled = enabled;
        repaint();
    }

    bool isBeingDragged() const noexcept { return isDragging; }

    std::function<void (double)> onSeek;

    void paint (juce::Graphics& g) override
    {
        using namespace Theme::Colours;

        const auto bounds = getLocalBounds().toFloat();
        const auto centreY = bounds.getCentreY();
        const auto track = bounds.withSizeKeepingCentre (bounds.getWidth(), 6.0f);

        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.fillRoundedRectangle (track, 3.0f);

        if (duration > 0.0)
        {
            if (loopStart >= 0.0 && loopEnd > loopStart)
            {
                const auto x1 = xForTime (loopStart);
                const auto x2 = xForTime (loopEnd);
                g.setColour (loop.withAlpha (loopEnabled ? 0.28f : 0.10f));
                g.fillRoundedRectangle ({ x1, centreY - 7.0f, x2 - x1, 14.0f }, 4.0f);
            }

            const auto playheadX = xForTime (position);
            g.setColour (text);
            g.fillRoundedRectangle (track.withRight (playheadX), 3.0f);

            g.setColour (loop);

            for (const auto marker : { loopStart, loopEnd })
                if (marker >= 0.0)
                    g.fillRoundedRectangle ({ xForTime (marker) - 1.0f, centreY - 12.0f, 2.0f, 24.0f }, 1.0f);

            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.fillEllipse (juce::Rectangle<float> (22.0f, 22.0f).withCentre ({ playheadX, centreY }));
            g.setColour (juce::Colours::white);
            g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ playheadX, centreY }));
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        isDragging = true;
        seekTo (event.position.x);
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        seekTo (event.position.x);
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        isDragging = false;
    }

private:
    static constexpr float inset = 7.0f;

    float xForTime (double seconds) const
    {
        const auto proportion = duration > 0.0 ? juce::jlimit (0.0, 1.0, seconds / duration) : 0.0;
        return inset + (float) proportion * ((float) getWidth() - inset * 2.0f);
    }

    void seekTo (float x)
    {
        if (duration <= 0.0)
            return;

        const auto proportion = juce::jlimit (0.0f, 1.0f, (x - inset) / ((float) getWidth() - inset * 2.0f));
        position = proportion * duration;
        repaint();

        if (onSeek != nullptr)
            onSeek (position);
    }

    double position = 0.0;
    double duration = 0.0;
    double loopStart = -1.0;
    double loopEnd = -1.0;
    bool loopEnabled = false;
    bool isDragging = false;
};

//==============================================================================
TransportBar::TransportBar()
    : timeline (std::make_unique<Timeline>())
{
    addAndMakeVisible (returnButton);
    returnButton.setCornerRadius (18.0f);
    returnButton.onClick = [this] { if (onReturnToStart != nullptr) onReturnToStart(); };

    addAndMakeVisible (playButton);
    playButton.onClick = [this] { if (onPlayPause != nullptr) onPlayPause(); };

    for (auto* label : { &currentTime, &totalTime })
    {
        addAndMakeVisible (label);
        label->setFont (Theme::monoFont (13.0f));
        label->setInterceptsMouseClicks (false, false);
    }

    currentTime.setJustificationType (juce::Justification::centredRight);
    totalTime.setColour (juce::Label::textColourId, Theme::Colours::textMuted);

    addAndMakeVisible (*timeline);
    timeline->onSeek = [this] (double seconds) { if (onSeek != nullptr) onSeek (seconds); };

    addAndMakeVisible (loopStartButton);
    addAndMakeVisible (loopEndButton);
    addAndMakeVisible (loopButton);
    loopButton.setIconSize (16.0f);
    loopButton.setButtonText ("Loop");
    loopButton.setCornerRadius (8.0f);
    loopStartButton.setCornerRadius (8.0f);
    loopEndButton.setCornerRadius (8.0f);

    loopStartButton.onClick = [this] { if (onSetLoopStart != nullptr) onSetLoopStart(); };
    loopEndButton.onClick = [this] { if (onSetLoopEnd != nullptr) onSetLoopEnd(); };
    loopButton.onClick = [this] { if (onToggleLoop != nullptr) onToggleLoop(); };

    setPosition (0.0, 0.0, false);
    setLoop (-1.0, -1.0, false);
}

TransportBar::~TransportBar() = default;

void TransportBar::setPlaying (bool isPlaying)
{
    if (playing == isPlaying)
        return;

    playing = isPlaying;
    playButton.setIcon (isPlaying ? Icons::pause() : Icons::play());
    playButton.setTitle (isPlaying ? "Pause" : "Play");
}

void TransportBar::setPosition (double positionSeconds, double durationSeconds, bool isWaiting)
{
    timeline->setPosition (positionSeconds, durationSeconds);
    currentTime.setText (formatTime (positionSeconds), juce::dontSendNotification);
    totalTime.setText (formatTime (durationSeconds), juce::dontSendNotification);

    // 待機中は時間表示の色を変えて、押すのを待っていることを示す
    currentTime.setColour (juce::Label::textColourId, isWaiting ? Theme::Colours::loopText : Theme::Colours::text);
}

void TransportBar::setLoop (double startSeconds, double endSeconds, bool isEnabled)
{
    timeline->setLoop (startSeconds, endSeconds, isEnabled);
    loopStartButton.setButtonText (startSeconds >= 0.0 ? "A " + formatTime (startSeconds) : "A");
    loopEndButton.setButtonText (endSeconds >= 0.0 ? "B " + formatTime (endSeconds) : "B");
    loopButton.setToggleState (isEnabled, juce::dontSendNotification);
    resized();
}

bool TransportBar::isScrubbing() const
{
    return timeline->isBeingDragged();
}

void TransportBar::paint (juce::Graphics& g)
{
    g.fillAll (Theme::Colours::panel);
    g.setColour (Theme::Colours::divider);
    g.fillRect (getLocalBounds().removeFromBottom (1));
}

void TransportBar::resized()
{
    auto area = getLocalBounds().reduced (20, 0);

    const auto centred = [] (juce::Rectangle<int> column, int height)
    {
        return column.withSizeKeepingCentre (column.getWidth(), height);
    };

    returnButton.setBounds (centred (area.removeFromLeft (36), 36));
    area.removeFromLeft (10);
    playButton.setBounds (centred (area.removeFromLeft (44), 44));
    area.removeFromLeft (14);
    currentTime.setBounds (area.removeFromLeft (48));
    area.removeFromLeft (10);

    loopButton.setBounds (centred (area.removeFromRight (loopButton.getIdealWidth (32)), 32));
    area.removeFromRight (6);
    loopEndButton.setBounds (centred (area.removeFromRight (loopEndButton.getIdealWidth (32) - 8), 32));
    area.removeFromRight (6);
    loopStartButton.setBounds (centred (area.removeFromRight (loopStartButton.getIdealWidth (32) - 8), 32));
    area.removeFromRight (14);

    totalTime.setBounds (area.removeFromRight (48));
    area.removeFromRight (10);
    timeline->setBounds (centred (area, 28));
}

juce::String TransportBar::formatTime (double seconds)
{
    const auto total = juce::jmax (0, (int) seconds);
    return juce::String (total / 60) + ":" + juce::String (total % 60).paddedLeft ('0', 2);
}

} // namespace pianolume
