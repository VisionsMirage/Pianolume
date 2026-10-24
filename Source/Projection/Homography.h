#pragma once

#include <juce_graphics/juce_graphics.h>
#include <array>

namespace pianolume
{

/** 射影変換（3x3 行列、行優先）。単位正方形を任意の四角形に写す変換を作れる。 */
struct Homography
{
    std::array<double, 9> m { 1, 0, 0, 0, 1, 0, 0, 0, 1 };

    /** (0,0) (1,0) (1,1) (0,1) をそれぞれ corners[0..3] に写す変換。 */
    static Homography squareToQuad (const std::array<juce::Point<float>, 4>& corners);

    Homography inverted() const;
    juce::Point<float> map (juce::Point<float> point) const;

    /** OpenGL の uniform に渡す列優先の配列。 */
    std::array<float, 9> toColumnMajor() const;
};

} // namespace pianolume
