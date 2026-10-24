#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "ProjectionView.h"

namespace pianolume
{

/** プロジェクタへの投影ウィンドウ。

    通常はディスプレイ全面に枠なしで出す。プレビュー用に、普通のウィンドウとして開くこともできる。
    合わせ込み中は、四隅をマウスでドラッグしたり矢印キーで動かしたりできる。
*/
class ProjectionWindow : public juce::Component
{
public:
    ProjectionWindow (const KeyState& keyState, const SongPlayer& player, CalibrationModel& calibrationModel,
                      const KeyboardLayout& layout, juce::Rectangle<int> area, bool isWindowed);
    ~ProjectionWindow() override;

    void setLayout (const KeyboardLayout& layout);
    void setLookAheadSeconds (double seconds);
    void setShowGrid (bool shouldShow);
    void setNoteNames (bool onNotes, NotesRenderer::KeyNames onKeys);

    /** 閉じる操作（Esc キー、ウィンドウの閉じるボタン）をしたときに呼ばれる。 */
    std::function<void()> onCloseRequested;

    /** Esc 以外のキー（再生操作など）を操作画面へ渡す。 */
    std::function<bool (const juce::KeyPress&)> onKeyPressed;

    /** 大きさが変わったとき（プレビューのウィンドウを広げたときなど）に呼ばれる。 */
    std::function<void()> onResized;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;
    void userTriedToCloseWindow() override;

    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;

private:
    juce::Point<float> toNormalised (juce::Point<float> position) const;

    CalibrationModel& calibrationModel;
    ProjectionView view;
    CalibrationModel::Handle draggingHandle = CalibrationModel::Handle::none;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProjectionWindow)
};

} // namespace pianolume
