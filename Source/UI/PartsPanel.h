#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Song/Song.h"
#include "ToolButton.h"

namespace pianolume
{

/** 曲のパート一覧。落下エリアの上に重ねて表示し、パートごとに右手 / 左手 / 伴奏 / ミュートを選ぶ。 */
class PartsPanel : public juce::Component
{
public:
    PartsPanel();
    ~PartsPanel() override;

    /** song が null なら一覧を空にする。getRole で現在の役割を問い合わせる。 */
    void setSong (const Song* song, const std::function<PartRole (int)>& getRole);

    /** 全パートを表示するのに必要な高さ。 */
    int getPreferredHeight() const;

    std::function<void (int partIndex, PartRole role)> onRoleChanged;
    std::function<void()> onClose;

    static constexpr int preferredWidth = 360;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    class Row;

    ToolButton closeButton { "Close parts", ToolButton::Variant::ghost, Icons::close() };
    juce::Viewport viewport;
    juce::Component rowContainer;
    juce::OwnedArray<Row> rows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PartsPanel)
};

} // namespace pianolume
