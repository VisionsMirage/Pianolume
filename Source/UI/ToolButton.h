#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Icons.h"

namespace pianolume
{

/** デザイン案のボタン。アイコンと文字（どちらか一方でも可）を描く。トグル状態は外から setToggleState で示す。 */
class ToolButton : public juce::Button
{
public:
    enum class Variant
    {
        secondary,      // 角丸の暗いボタン。トグル中は明るい枠
        ghost,          // 背景なし
        primary,        // 白地の角丸
        primaryCircle,  // 白地の円（再生ボタン）
        loop,           // トグル中はアンバー
        loopMarker      // ループ始点・終点の表示（アンバー、等幅文字）
    };

    explicit ToolButton (const juce::String& name, Variant variant = Variant::secondary, Icon icon = {});

    void setIcon (Icon newIcon);
    void setIconSize (float size)                   { iconSize = size; repaint(); }
    void setCornerRadius (float radius)             { cornerRadius = radius; repaint(); }

    /** 文字を含めたときの推奨幅。 */
    int getIdealWidth (int height) const;

    void paintButton (juce::Graphics& g, bool isHighlighted, bool isDown) override;

private:
    juce::Font getTextFont() const;

    Variant variant;
    Icon icon;
    float iconSize = 18.0f;
    float cornerRadius = 10.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToolButton)
};

} // namespace pianolume
