#pragma once

#include <juce_graphics/juce_graphics.h>
#include <array>

namespace pianolume
{

/** 投影の合わせ込み結果。座標はプロジェクタ画面全体を 0〜1 とした値。

    各面の四隅は「奥の左、奥の右、手前の右、手前の左」の順。
    落下エリア（鍵盤の奥の面）の手前の辺と、鍵盤エリアの奥の辺が接する。
*/
struct ProjectionCalibration
{
    using Corners = std::array<juce::Point<float>, 4>;

    Corners fallCorners;
    Corners keyCorners;

    /** 落下エリアの手前の辺と鍵盤エリアの奥の辺を一緒に動かすか。 */
    bool linkPlanes = true;

    /** 黒鍵の補正。X は白鍵幅、Y と長さは鍵盤エリアの奥行きに対する割合。
        黒鍵は白鍵より高いので、ななめから投影すると場所によってずれ方が変わる。X は左端と右端で別に持つ。 */
    float blackKeyOffsetXLeft = 0.0f;
    float blackKeyOffsetXRight = 0.0f;
    float blackKeyOffsetY = 0.0f;
    float blackKeyLength = 0.62f;

    /** 投影全体の明るさとガンマ。 */
    float brightness = 1.0f;
    float gamma = 1.0f;

    static ProjectionCalibration createDefault();

    juce::var toVar() const;
    static ProjectionCalibration fromVar (const juce::var& value);
};

} // namespace pianolume
