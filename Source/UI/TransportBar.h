#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "ToolButton.h"

namespace pianolume
{

/** 画面上部の 2 段目。再生操作、再生位置（ループ区間つき）、A-B ループ。 */
class TransportBar : public juce::Component
{
public:
    TransportBar();
    ~TransportBar() override;

    void setPlaying (bool isPlaying);
    void setPosition (double positionSeconds, double durationSeconds, bool isWaiting);

    /** 始点・終点が未設定なら負の値を渡す。 */
    void setLoop (double startSeconds, double endSeconds, bool isEnabled);

    /** 再生位置のバーをドラッグ中か（ドラッグ中は外から位置を更新しない）。 */
    bool isScrubbing() const;

    std::function<void()> onReturnToStart;
    std::function<void()> onPlayPause;
    std::function<void (double seconds)> onSeek;
    std::function<void()> onSetLoopStart;
    std::function<void()> onSetLoopEnd;
    std::function<void()> onToggleLoop;

    static constexpr int preferredHeight = 56;

    void paint (juce::Graphics& g) override;
    void resized() override;

    static juce::String formatTime (double seconds);

private:
    class Timeline;

    ToolButton returnButton { "Return to start", ToolButton::Variant::ghost, Icons::skipBack() };
    ToolButton playButton { "Play", ToolButton::Variant::primaryCircle, Icons::play() };
    juce::Label currentTime, totalTime;
    std::unique_ptr<Timeline> timeline;
    ToolButton loopStartButton { "Loop start", ToolButton::Variant::loopMarker };
    ToolButton loopEndButton { "Loop end", ToolButton::Variant::loopMarker };
    ToolButton loopButton { "Loop", ToolButton::Variant::loop, Icons::loop() };

    bool playing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportBar)
};

} // namespace pianolume
