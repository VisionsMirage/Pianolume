#include "PartsPanel.h"
#include "Theme.h"
#include "View/PartColours.h"

namespace pianolume
{

namespace
{
    constexpr int headerHeight = 62;
    constexpr int rowHeight = 96;
    constexpr int bottomPadding = 8;
    constexpr float cornerRadius = 16.0f;

    constexpr std::pair<PartRole, const char*> roleNames[] = {
        { PartRole::rightHand,     "Right" },
        { PartRole::leftHand,      "Left" },
        { PartRole::accompaniment, "Accomp." },
        { PartRole::mute,          "Mute" },
    };

    juce::Colour getRoleColour (PartRole role)
    {
        return role == PartRole::mute ? juce::Colour (0xff4b5262) : PartColours::forRole (role);
    }

    juce::String describe (const SongPart& part)
    {
        return "Ch " + juce::String (part.channel)
             + (part.instrument.isNotEmpty() ? juce::String::fromUTF8 (" \xc2\xb7 ") + part.instrument : juce::String())
             + juce::String::fromUTF8 (" \xc2\xb7 ") + juce::String (part.noteCount) + " notes, "
             + juce::MidiMessage::getMidiNoteName (part.lowestNote, true, true, 4) + juce::String::fromUTF8 ("\xe2\x80\x93")
             + juce::MidiMessage::getMidiNoteName (part.highestNote, true, true, 4);
    }
}

//==============================================================================
class PartsPanel::Row : public juce::Component
{
public:
    Row (int index, const SongPart& part, PartRole initialRole)
        : partIndex (index),
          name (part.name),
          details (describe (part)),
          role (initialRole)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    std::function<void (int, PartRole)> onRoleChanged;

    void paint (juce::Graphics& g) override
    {
        using namespace Theme::Colours;

        g.setColour (divider);
        g.fillRect (getLocalBounds().removeFromTop (1));

        auto area = getLocalBounds().reduced (16, 12);
        auto top = area.removeFromTop (36);

        g.setColour (getRoleColour (role));
        g.fillEllipse (top.removeFromLeft (10).withSizeKeepingCentre (10, 10).toFloat());
        top.removeFromLeft (10);

        g.setColour (text);
        g.setFont (Theme::uiFont (14.0f, 600));
        g.drawText (name, top.removeFromTop (19), juce::Justification::centredLeft, true);

        g.setColour (textMuted);
        g.setFont (Theme::uiFont (12.0f));
        g.drawText (details, top, juce::Justification::centredLeft, true);

        for (const auto& [value, label, bounds] : getRoleButtons())
        {
            const auto isSelected = value == role;
            const auto colour = value == PartRole::mute || value == PartRole::accompaniment ? juce::Colour (0xff9ca3af)
                                                                                             : getRoleColour (value);

            g.setColour (isSelected ? colour.withAlpha (0.16f) : juce::Colours::transparentBlack);
            g.fillRoundedRectangle (bounds, 8.0f);
            g.setColour (isSelected ? colour.withAlpha (0.7f) : border);
            g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

            g.setColour (isSelected ? primary : textMuted);
            g.setFont (Theme::uiFont (12.0f, 500));
            g.drawText (label, bounds, juce::Justification::centred, false);
        }
    }

    void mouseUp (const juce::MouseEvent& event) override
    {
        for (const auto& [value, label, bounds] : getRoleButtons())
        {
            juce::ignoreUnused (label);

            if (bounds.contains (event.position) && value != role)
            {
                role = value;
                repaint();

                if (onRoleChanged != nullptr)
                    onRoleChanged (partIndex, role);
            }
        }
    }

private:
    struct RoleButton
    {
        PartRole value;
        const char* label;
        juce::Rectangle<float> bounds;
    };

    std::vector<RoleButton> getRoleButtons() const
    {
        auto area = getLocalBounds().reduced (16, 12).withTrimmedTop (46).withHeight (30).toFloat();
        const auto gap = 4.0f;
        const auto width = (area.getWidth() - gap * 3.0f) / 4.0f;

        std::vector<RoleButton> buttons;

        for (const auto& [value, label] : roleNames)
        {
            buttons.push_back ({ value, label, area.removeFromLeft (width) });
            area.removeFromLeft (gap);
        }

        return buttons;
    }

    const int partIndex;
    const juce::String name, details;
    PartRole role;
};

//==============================================================================
PartsPanel::PartsPanel()
{
    addAndMakeVisible (closeButton);
    closeButton.setIconSize (14.0f);
    closeButton.setCornerRadius (8.0f);
    closeButton.onClick = [this] { if (onClose != nullptr) onClose(); };

    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&rowContainer, false);
    viewport.setScrollBarsShown (true, false);
}

PartsPanel::~PartsPanel() = default;

void PartsPanel::setSong (const Song* song, const std::function<PartRole (int)>& getRole)
{
    rows.clear();

    if (song != nullptr)
    {
        for (int i = 0; i < (int) song->parts.size(); ++i)
        {
            const auto& part = song->parts[(size_t) i];

            // コントローラだけでノートの無いパートは表示しない
            if (part.noteCount == 0)
                continue;

            auto* row = rows.add (std::make_unique<Row> (i, part, getRole (i)));
            row->onRoleChanged = [this] (int index, PartRole role)
            {
                if (onRoleChanged != nullptr)
                    onRoleChanged (index, role);
            };

            rowContainer.addAndMakeVisible (row);
        }
    }

    resized();
    repaint();
}

int PartsPanel::getPreferredHeight() const
{
    return headerHeight + juce::jmax (1, rows.size()) * rowHeight + bottomPadding;
}

void PartsPanel::paint (juce::Graphics& g)
{
    using namespace Theme::Colours;

    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);

    g.setColour (overlay);
    g.fillRoundedRectangle (bounds, cornerRadius);
    g.setColour (juce::Colours::white.withAlpha (0.09f));
    g.drawRoundedRectangle (bounds, cornerRadius, 1.0f);

    auto header = getLocalBounds().removeFromTop (headerHeight).reduced (16, 12);

    g.setColour (text);
    g.setFont (Theme::uiFont (15.0f, 600));
    g.drawText ("Parts", header.removeFromTop (20), juce::Justification::centredLeft, false);

    g.setColour (textMuted);
    g.setFont (Theme::uiFont (12.0f));
    g.drawText ("Choose who plays each part", header, juce::Justification::centredLeft, false);

    if (rows.isEmpty())
    {
        g.setColour (textMuted);
        g.setFont (Theme::uiFont (13.0f));
        g.drawText ("Open a MIDI file to see its parts",
                    getLocalBounds().withTrimmedTop (headerHeight).reduced (16, 0).removeFromTop (rowHeight),
                    juce::Justification::centredLeft, false);
    }
}

void PartsPanel::resized()
{
    auto area = getLocalBounds();
    auto header = area.removeFromTop (headerHeight);
    closeButton.setBounds (header.removeFromRight (48).withSizeKeepingCentre (32, 32));

    viewport.setBounds (area.withTrimmedBottom (bottomPadding));

    const auto width = viewport.getMaximumVisibleWidth();
    rowContainer.setSize (width, rows.size() * rowHeight);

    for (int i = 0; i < rows.size(); ++i)
        rows[i]->setBounds (0, i * rowHeight, width, rowHeight);
}

} // namespace pianolume
