#include "ProjectionCalibration.h"

namespace pianolume
{

namespace
{
    namespace Keys
    {
        const juce::Identifier fall { "fall" };
        const juce::Identifier keys { "keys" };
        const juce::Identifier linkPlanes { "linkPlanes" };
        const juce::Identifier blackKeyOffsetX { "blackKeyOffsetX" }; // 左右を分ける前の形式
        const juce::Identifier blackKeyOffsetXLeft { "blackKeyOffsetXLeft" };
        const juce::Identifier blackKeyOffsetXRight { "blackKeyOffsetXRight" };
        const juce::Identifier blackKeyOffsetY { "blackKeyOffsetY" };
        const juce::Identifier blackKeyLength { "blackKeyLength" };
        const juce::Identifier brightness { "brightness" };
        const juce::Identifier gamma { "gamma" };
    }

    juce::var cornersToVar (const ProjectionCalibration::Corners& corners)
    {
        juce::Array<juce::var> values;

        for (const auto& corner : corners)
        {
            values.add (corner.x);
            values.add (corner.y);
        }

        return values;
    }

    void cornersFromVar (const juce::var& value, ProjectionCalibration::Corners& corners)
    {
        const auto* values = value.getArray();

        if (values == nullptr || values->size() != 8)
            return;

        for (size_t i = 0; i < corners.size(); ++i)
            corners[i] = { (float) (double) values->getReference ((int) i * 2),
                           (float) (double) values->getReference ((int) i * 2 + 1) };
    }

    float getFloat (const juce::var& object, const juce::Identifier& key, float fallback)
    {
        const auto value = object.getProperty (key, fallback);
        return (float) (double) value;
    }
}

ProjectionCalibration ProjectionCalibration::createDefault()
{
    ProjectionCalibration calibration;
    calibration.fallCorners = { { { 0.05f, 0.05f }, { 0.95f, 0.05f }, { 0.95f, 0.62f }, { 0.05f, 0.62f } } };
    calibration.keyCorners = { { { 0.05f, 0.62f }, { 0.95f, 0.62f }, { 0.95f, 0.94f }, { 0.05f, 0.94f } } };
    return calibration;
}

juce::var ProjectionCalibration::toVar() const
{
    auto* object = new juce::DynamicObject();
    object->setProperty (Keys::fall, cornersToVar (fallCorners));
    object->setProperty (Keys::keys, cornersToVar (keyCorners));
    object->setProperty (Keys::linkPlanes, linkPlanes);
    object->setProperty (Keys::blackKeyOffsetXLeft, blackKeyOffsetXLeft);
    object->setProperty (Keys::blackKeyOffsetXRight, blackKeyOffsetXRight);
    object->setProperty (Keys::blackKeyOffsetY, blackKeyOffsetY);
    object->setProperty (Keys::blackKeyLength, blackKeyLength);
    object->setProperty (Keys::brightness, brightness);
    object->setProperty (Keys::gamma, gamma);
    return object;
}

ProjectionCalibration ProjectionCalibration::fromVar (const juce::var& value)
{
    auto calibration = createDefault();

    if (! value.isObject())
        return calibration;

    cornersFromVar (value[Keys::fall], calibration.fallCorners);
    cornersFromVar (value[Keys::keys], calibration.keyCorners);
    calibration.linkPlanes = (bool) value.getProperty (Keys::linkPlanes, true);
    const auto legacyOffsetX = getFloat (value, Keys::blackKeyOffsetX, 0.0f);
    calibration.blackKeyOffsetXLeft = getFloat (value, Keys::blackKeyOffsetXLeft, legacyOffsetX);
    calibration.blackKeyOffsetXRight = getFloat (value, Keys::blackKeyOffsetXRight, legacyOffsetX);
    calibration.blackKeyOffsetY = getFloat (value, Keys::blackKeyOffsetY, calibration.blackKeyOffsetY);
    calibration.blackKeyLength = getFloat (value, Keys::blackKeyLength, calibration.blackKeyLength);
    calibration.brightness = getFloat (value, Keys::brightness, calibration.brightness);
    calibration.gamma = getFloat (value, Keys::gamma, calibration.gamma);
    return calibration;
}

} // namespace pianolume
