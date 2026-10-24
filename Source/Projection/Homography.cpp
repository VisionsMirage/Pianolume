#include "Homography.h"

namespace pianolume
{

Homography Homography::squareToQuad (const std::array<juce::Point<float>, 4>& corners)
{
    // Heckbert, "Fundamentals of Texture Mapping and Image Warping" の正方形→四角形の式
    const double x0 = corners[0].x, y0 = corners[0].y;
    const double x1 = corners[1].x, y1 = corners[1].y;
    const double x2 = corners[2].x, y2 = corners[2].y;
    const double x3 = corners[3].x, y3 = corners[3].y;

    const auto dx1 = x1 - x2, dx2 = x3 - x2, dx3 = x0 - x1 + x2 - x3;
    const auto dy1 = y1 - y2, dy2 = y3 - y2, dy3 = y0 - y1 + y2 - y3;

    double g = 0.0, h = 0.0;
    const auto denominator = dx1 * dy2 - dx2 * dy1;

    if (std::abs (denominator) > 1.0e-12)
    {
        g = (dx3 * dy2 - dx2 * dy3) / denominator;
        h = (dx1 * dy3 - dx3 * dy1) / denominator;
    }

    Homography result;
    result.m = { x1 - x0 + g * x1, x3 - x0 + h * x3, x0,
                 y1 - y0 + g * y1, y3 - y0 + h * y3, y0,
                 g,                h,                1.0 };
    return result;
}

Homography Homography::inverted() const
{
    const auto& a = m;

    const auto c00 = a[4] * a[8] - a[5] * a[7];
    const auto c01 = a[5] * a[6] - a[3] * a[8];
    const auto c02 = a[3] * a[7] - a[4] * a[6];
    const auto determinant = a[0] * c00 + a[1] * c01 + a[2] * c02;

    if (std::abs (determinant) < 1.0e-12)
        return {};

    const auto inverse = 1.0 / determinant;

    Homography result;
    result.m = { c00 * inverse, (a[2] * a[7] - a[1] * a[8]) * inverse, (a[1] * a[5] - a[2] * a[4]) * inverse,
                 c01 * inverse, (a[0] * a[8] - a[2] * a[6]) * inverse, (a[2] * a[3] - a[0] * a[5]) * inverse,
                 c02 * inverse, (a[1] * a[6] - a[0] * a[7]) * inverse, (a[0] * a[4] - a[1] * a[3]) * inverse };
    return result;
}

juce::Point<float> Homography::map (juce::Point<float> point) const
{
    const auto w = m[6] * point.x + m[7] * point.y + m[8];

    if (std::abs (w) < 1.0e-12)
        return {};

    return { (float) ((m[0] * point.x + m[1] * point.y + m[2]) / w),
             (float) ((m[3] * point.x + m[4] * point.y + m[5]) / w) };
}

std::array<float, 9> Homography::toColumnMajor() const
{
    return { (float) m[0], (float) m[3], (float) m[6],
             (float) m[1], (float) m[4], (float) m[7],
             (float) m[2], (float) m[5], (float) m[8] };
}

} // namespace pianolume
