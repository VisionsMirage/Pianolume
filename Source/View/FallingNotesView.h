#pragma once

#include <juce_opengl/juce_opengl.h>
#include "NotesRenderer.h"

namespace pianolume
{

/** 操作画面の落下バーと鍵盤。

    OpenGL の描画スレッドで画面更新ごとに描くので、バーが滑らかに動き、押鍵の表示も遅れない。
    子コンポーネント（パート一覧など）を上に重ねて表示できる。
*/
class FallingNotesView : public juce::Component,
                         private juce::OpenGLRenderer
{
public:
    FallingNotesView (const KeyState& keyState, const SongPlayer& player);
    ~FallingNotesView() override;

    /** 画面に描く鍵盤の範囲。 */
    void setLayout (const KeyboardLayout& newLayout);

    /** 入力鍵盤で弾ける音域。範囲外の鍵とレーンはグレーで示す。 */
    void setPlayableRange (int lowestNote, int highestNote);

    /** 落下エリアの上端から判定ラインまでにかかる時間（秒）。 */
    void setLookAheadSeconds (double seconds);

    /** 落下ノートの先頭と、鍵の手前側に音名を書くか。 */
    void setNoteNames (bool onNotes, NotesRenderer::KeyNames onKeys);

    /** 全体の高さに対する鍵盤エリアの割合。 */
    void setKeyAreaProportion (float proportion)    { keyAreaProportion = juce::jlimit (0.05f, 0.9f, proportion); }

    void paint (juce::Graphics&) override {}
    void resized() override;

private:
    void newOpenGLContextCreated() override {}
    void renderOpenGL() override;
    void openGLContextClosing() override {}

    NotesRenderer renderer;
    juce::OpenGLContext openGLContext;

    juce::SpinLock settingsLock;
    NotesRenderer::Settings settings;

    std::atomic<float> keyAreaProportion { 0.2f };
    std::atomic<int> viewWidth { 0 };
    std::atomic<int> viewHeight { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FallingNotesView)
};

} // namespace pianolume
