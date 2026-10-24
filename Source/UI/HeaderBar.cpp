#include "HeaderBar.h"
#include "Theme.h"
#include "View/PartColours.h"

namespace pianolume
{

namespace
{
    constexpr int horizontalPadding = 20;
    constexpr int gap = 18;
    constexpr int controlHeight = 36;
    constexpr int iconButtonSize = 40;
}

//==============================================================================
/** Listen / Play Along / Wait を切り替えるセグメント。 */
class HeaderBar::ModeSelector : public juce::Component
{
public:
    explicit ModeSelector (std::vector<Mode> modesToUse)
        : modes (std::move (modesToUse))
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTitle ("Play mode");
    }

    void setSelectedId (int id)
    {
        selectedId = id;
        repaint();
    }

    int getIdealWidth() const
    {
        auto width = padding * 2.0f;

        for (const auto& mode : modes)
            width += getSegmentWidth (mode);

        return juce::roundToInt (width + gapBetween * (float) (modes.size() - 1));
    }

    std::function<void (int)> onChange;

    void paint (juce::Graphics& g) override
    {
        using namespace Theme::Colours;

        const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (control);
        g.fillRoundedRectangle (bounds, 11.0f);
        g.setColour (border);
        g.drawRoundedRectangle (bounds, 11.0f, 1.0f);

        for (const auto& [mode, area] : getSegments())
        {
            const auto isSelected = mode.id == selectedId;

            if (isSelected)
            {
                g.setColour (primary);
                g.fillRoundedRectangle (area, 8.0f);
            }

            g.setColour (isSelected ? onPrimary : juce::Colour (0xffb4bbcb));
            g.setFont (Theme::uiFont (13.0f, isSelected ? 600 : 500));
            g.drawText (mode.label, area, juce::Justification::centred, false);
        }
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        for (const auto& [mode, area] : getSegments())
        {
            if (area.contains (event.position) && mode.id != selectedId)
            {
                setSelectedId (mode.id);

                if (onChange != nullptr)
                    onChange (mode.id);
            }
        }
    }

private:
    static constexpr float padding = 3.0f;
    static constexpr float gapBetween = 2.0f;

    float getSegmentWidth (const Mode& mode) const
    {
        return juce::GlyphArrangement::getStringWidth (Theme::uiFont (13.0f, 600), mode.label) + 28.0f;
    }

    std::vector<std::pair<Mode, juce::Rectangle<float>>> getSegments() const
    {
        std::vector<std::pair<Mode, juce::Rectangle<float>>> segments;
        auto x = padding;

        for (const auto& mode : modes)
        {
            const auto width = getSegmentWidth (mode);
            segments.push_back ({ mode, { x, padding, width, (float) getHeight() - padding * 2.0f } });
            x += width + gapBetween;
        }

        return segments;
    }

    std::vector<Mode> modes;
    int selectedId = 0;
};

//==============================================================================
/** 再生速度の − / ＋。 */
class HeaderBar::SpeedStepper : public juce::Component
{
public:
    explicit SpeedStepper (std::vector<int> percentsToUse)
        : percents (std::move (percentsToUse))
    {
        for (auto* button : { &minusButton, &plusButton })
        {
            addAndMakeVisible (button);
            button->setIconSize (16.0f);
            button->setCornerRadius (7.0f);
        }

        minusButton.setTitle ("Slower");
        plusButton.setTitle ("Faster");
        minusButton.onClick = [this] { step (-1); };
        plusButton.onClick = [this] { step (1); };
    }

    void setPercent (int percent)
    {
        for (size_t i = 0; i < percents.size(); ++i)
            if (percents[i] == percent)
                index = i;

        repaint();
    }

    std::function<void (int)> onChange;

    void paint (juce::Graphics& g) override
    {
        using namespace Theme::Colours;

        const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (control);
        g.fillRoundedRectangle (bounds, 10.0f);
        g.setColour (border);
        g.drawRoundedRectangle (bounds, 10.0f, 1.0f);

        g.setColour (text);
        g.setFont (Theme::monoFont (13.0f));
        g.drawText (juce::String (percents[index]) + "%", getLocalBounds().reduced (32, 0), juce::Justification::centred, false);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (4);
        minusButton.setBounds (area.removeFromLeft (28));
        plusButton.setBounds (area.removeFromRight (28));
    }

private:
    void step (int direction)
    {
        const auto newIndex = juce::jlimit (0, (int) percents.size() - 1, (int) index + direction);

        if ((size_t) newIndex == index)
            return;

        index = (size_t) newIndex;
        repaint();

        if (onChange != nullptr)
            onChange (percents[index]);
    }

    std::vector<int> percents;
    size_t index = 0;
    ToolButton minusButton { "-", ToolButton::Variant::ghost, Icons::minus() };
    ToolButton plusButton { "+", ToolButton::Variant::ghost, Icons::plus() };
};

//==============================================================================
HeaderBar::HeaderBar (std::vector<Mode> modes, std::vector<int> speedPercents)
    : modeSelector (std::make_unique<ModeSelector> (std::move (modes))),
      speedStepper (std::make_unique<SpeedStepper> (std::move (speedPercents)))
{
    addAndMakeVisible (openButton);
    openButton.setIcon (Icons::folder());
    openButton.setIconSize (16.0f);
    openButton.setButtonText ("Open");
    openButton.onClick = [this] { if (onOpen != nullptr) onOpen(); };

    addAndMakeVisible (*modeSelector);
    modeSelector->onChange = [this] (int id) { if (onModeChanged != nullptr) onModeChanged (id); };

    addAndMakeVisible (*speedStepper);
    speedStepper->onChange = [this] (int percent) { if (onSpeedChanged != nullptr) onSpeedChanged (percent); };

    const std::pair<ToolButton*, Icon> iconButtons[] = {
        { &partsButton, Icons::layers() },
        { &logButton, Icons::terminal() },
        { &projectionButton, Icons::projector() },
        { &settingsButton, Icons::settings() },
    };

    for (const auto& [button, icon] : iconButtons)
    {
        addAndMakeVisible (button);
        button->setIcon (icon);
        button->setTooltip (button->getName());
    }

    partsButton.onClick = [this] { if (onPartsClicked != nullptr) onPartsClicked(); };
    logButton.onClick = [this] { if (onLogClicked != nullptr) onLogClicked(); };
    projectionButton.onClick = [this] { if (onProjectionClicked != nullptr) onProjectionClicked(); };
    settingsButton.onClick = [this] { if (onSettingsClicked != nullptr) onSettingsClicked(); };
}

HeaderBar::~HeaderBar() = default;

void HeaderBar::setNowPlaying (const juce::String& title, const juce::String& details)
{
    nowPlayingTitle = title;
    nowPlayingDetails = details;
    repaint (nowPlayingArea);
}

void HeaderBar::setMode (int modeId)
{
    modeSelector->setSelectedId (modeId);
}

void HeaderBar::setSpeedPercent (int percent)
{
    speedStepper->setPercent (percent);
}

void HeaderBar::paint (juce::Graphics& g)
{
    using namespace Theme::Colours;

    g.fillAll (panel);
    g.setColour (divider);
    g.fillRect (getLocalBounds().removeFromBottom (1));

    // ロゴ: 鍵盤を表すマークと右手・左手の色
    {
        auto area = logoArea;
        const auto mark = area.removeFromLeft (26).toFloat().withSizeKeepingCentre (26.0f, 26.0f);

        g.setColour (text);
        g.drawRoundedRectangle (mark.reduced (1.0f), 7.0f, 1.5f);

        for (const auto x : { 8.0f, 13.0f, 18.0f })
            g.fillRoundedRectangle (mark.getX() + x - 0.75f, mark.getY() + 7.0f, 1.5f, 12.0f, 0.75f);

        g.setColour (PartColours::forRole (PartRole::rightHand));
        g.fillRoundedRectangle (mark.getX() + 9.5f, mark.getY() + 6.0f, 2.5f, 7.0f, 1.0f);
        g.setColour (PartColours::forRole (PartRole::leftHand));
        g.fillRoundedRectangle (mark.getX() + 14.5f, mark.getY() + 6.0f, 2.5f, 7.0f, 1.0f);

        g.setColour (text);
        g.setFont (Theme::uiFont (17.0f, 700));
        g.drawText ("Pianolume", area.withTrimmedLeft (10), juce::Justification::centredLeft, false);
    }

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.fillRect (dividerArea);

    // 再生中の曲
    {
        auto area = nowPlayingArea;
        const auto cover = area.removeFromLeft (38).withSizeKeepingCentre (38, 38).toFloat();

        g.setColour (juce::Colour (0xff1b2130));
        g.fillRoundedRectangle (cover, 10.0f);
        Icons::music().draw (g, cover.withSizeKeepingCentre (18.0f, 18.0f), textSecondary);

        area.removeFromLeft (12);
        auto lines = area.withSizeKeepingCentre (area.getWidth(), 46);

        g.setColour (textMuted);
        g.setFont (Theme::uiFont (10.5f, 600));
        g.drawText ("NOW PLAYING", lines.removeFromTop (13), juce::Justification::centredLeft, false);

        g.setColour (nowPlayingTitle.isEmpty() ? textMuted : text);
        g.setFont (Theme::uiFont (15.0f, 600));
        g.drawText (nowPlayingTitle.isEmpty() ? "No song loaded" : nowPlayingTitle,
                    lines.removeFromTop (19), juce::Justification::centredLeft, true);

        g.setColour (textMuted);
        g.setFont (Theme::uiFont (12.0f));
        g.drawText (nowPlayingTitle.isEmpty() ? "Open a MIDI file or drop it here" : nowPlayingDetails,
                    lines, juce::Justification::centredLeft, true);
    }
}

void HeaderBar::resized()
{
    auto area = getLocalBounds().reduced (horizontalPadding, 0);

    const auto centred = [] (juce::Rectangle<int> column, int height)
    {
        return column.withSizeKeepingCentre (column.getWidth(), height);
    };

    logoArea = area.removeFromLeft (142);
    area.removeFromLeft (gap);

    openButton.setBounds (centred (area.removeFromLeft (openButton.getIdealWidth (controlHeight)), controlHeight));
    area.removeFromLeft (gap);

    dividerArea = centred (area.removeFromLeft (1), 28);
    area.removeFromLeft (gap);

    for (auto* button : { &settingsButton, &projectionButton, &logButton, &partsButton })
    {
        button->setBounds (centred (area.removeFromRight (iconButtonSize), iconButtonSize));
        area.removeFromRight (6);
    }

    area.removeFromRight (gap - 6);
    speedStepper->setBounds (centred (area.removeFromRight (108), controlHeight));
    area.removeFromRight (gap);
    modeSelector->setBounds (centred (area.removeFromRight (modeSelector->getIdealWidth()), controlHeight + 2));
    area.removeFromRight (gap);

    nowPlayingArea = area;
}

} // namespace pianolume
