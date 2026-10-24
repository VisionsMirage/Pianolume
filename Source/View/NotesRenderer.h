#pragma once

#include <juce_graphics/juce_graphics.h>
#include "Keyboard/KeyboardLayout.h"
#include "Keyboard/KeyState.h"
#include "Song/SongPlayer.h"

namespace pianolume
{

/** 落下エリアと鍵盤の描画。操作画面では上下に並べ、投影では別々の面に描いてから変形する。

    描画スレッドから呼ぶ。KeyState と SongPlayer はスレッドをまたいで読める作りになっている。
*/
class NotesRenderer
{
public:
    enum class Style
    {
        screen,     // 実物に近い鍵盤を描く（モニタ用）
        projection  // 光る部分だけを描き、それ以外は黒（プロジェクタ用）
    };

    /** 鍵に音名を書く範囲。 */
    enum class KeyNames
    {
        none,
        whiteKeys,
        blackKeys,
        cOnly
    };

    struct Settings
    {
        Style style = Style::screen;
        KeyboardLayout layout;
        double lookAheadSeconds = 2.5;
        bool showGrid = true;
        bool showNoteNames = false;     // 落下ノートの先頭に音名を書く
        KeyNames keyNames = KeyNames::none; // 鍵の手前側に音名を書く範囲
        int playableLowest = 0;
        int playableHighest = 127;

        // 黒鍵の位置と長さ。X は白鍵幅、Y と長さは鍵盤エリアの奥行きに対する割合。
        // X は左端と右端で別に持ち、あいだは位置に応じて補間する（白鍵との高さの差による視差は場所で変わるため）
        float blackKeyOffsetXLeft = 0.0f;
        float blackKeyOffsetXRight = 0.0f;
        float blackKeyOffsetY = 0.0f;
        float blackKeyLength = KeyboardLayout::blackKeyLength;

        bool isPlayable (int note) const noexcept { return note >= playableLowest && note <= playableHighest; }
        bool showsKeyName (int note) const noexcept;
    };

    struct KeyGuide
    {
        float level = 0.0f;
        PartRole role = PartRole::accompaniment;
    };

    /** 1 フレーム分の状態。落下エリアを描くときにガイド（光らせる鍵）が決まり、鍵盤の描画で使う。 */
    struct Frame
    {
        std::shared_ptr<const Song> song;
        double now = 0.0;
        bool isPlaying = false;
        bool isWaiting = false;
        std::array<KeyGuide, 128> guides {};
    };

    NotesRenderer (const KeyState& keyState, const SongPlayer& player);

    /** 描画のはじめに 1 回呼ぶ。描画スレッドごとに別の NotesRenderer を使うこと。 */
    Frame beginFrame();

    /** 落下エリア（オクターブ線、小節線、範囲外のレーン、ノート、判定ライン）。 */
    void paintFallArea (juce::Graphics& g, juce::Rectangle<float> area, const Settings& settings, Frame& frame) const;

    /** 鍵盤。 */
    void paintKeys (juce::Graphics& g, juce::Rectangle<float> area, const Settings& settings, const Frame& frame) const;

    /** 鍵の左右の位置。area の幅に鍵盤全体を収める。 */
    static juce::Range<float> getKeyX (const KeyboardLayout& layout, juce::Rectangle<float> area, int note);

    /** 黒鍵の補正を反映した鍵の矩形。 */
    static juce::Rectangle<float> getKeyBounds (const Settings& settings, juce::Rectangle<float> area, int note);

private:
    void paintGrid (juce::Graphics& g, const Song& song, juce::Rectangle<float> area,
                    const Settings& settings, double now) const;
    void paintUnplayableLanes (juce::Graphics& g, juce::Rectangle<float> area, const Settings& settings) const;
    void paintNotes (juce::Graphics& g, juce::Rectangle<float> area, const Settings& settings, Frame& frame) const;
    void paintHitLine (juce::Graphics& g, juce::Rectangle<float> area, const Settings& settings, const Frame& frame) const;
    void paintWaitingChip (juce::Graphics& g, juce::Rectangle<float> area, const Settings& settings, const Frame& frame) const;

    double getSmoothedPosition (bool isPlaying, bool isWaiting);

    const KeyState& keyState;
    const SongPlayer& player;

    // 表示用の再生位置。オーディオの処理単位ごとの揺れをならして、ノートを一定の速さで動かす
    double displayedPosition = 0.0;
    double lastFrameTimeMs = 0.0;
    bool hasDisplayedPosition = false;
};

} // namespace pianolume
