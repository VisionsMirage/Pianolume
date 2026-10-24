#include "CalibrationModel.h"

namespace pianolume
{

namespace
{
    using Handle = CalibrationModel::Handle;

    juce::Point<float>& getCorner (ProjectionCalibration& calibration, Handle handle)
    {
        const auto index = (size_t) handle;
        return index < 4 ? calibration.fallCorners[index] : calibration.keyCorners[index - 4];
    }

    /** 連動しているときに一緒に動く相手のハンドル。 */
    Handle getLinkedHandle (Handle handle)
    {
        switch (handle)
        {
            case Handle::fallBottomLeft:  return Handle::keyTopLeft;
            case Handle::fallBottomRight: return Handle::keyTopRight;
            case Handle::keyTopLeft:      return Handle::fallBottomLeft;
            case Handle::keyTopRight:     return Handle::fallBottomRight;
            default:                      return Handle::none;
        }
    }

    juce::Point<float> clampToScreen (juce::Point<float> point)
    {
        // 少しだけ画面外にも置けるようにして、端まで投影を広げられるようにする
        return { juce::jlimit (-0.25f, 1.25f, point.x), juce::jlimit (-0.25f, 1.25f, point.y) };
    }
}

ProjectionCalibration CalibrationModel::getCalibration() const
{
    const juce::SpinLock::ScopedLockType scopedLock (lock);
    return calibration;
}

template <typename Function>
void CalibrationModel::modify (Function&& function)
{
    {
        const juce::SpinLock::ScopedLockType scopedLock (lock);
        function (calibration);
    }

    sendChangeMessage();
}

void CalibrationModel::setCalibration (const ProjectionCalibration& newCalibration)
{
    modify ([&] (ProjectionCalibration& current) { current = newCalibration; });
}

std::vector<CalibrationModel::Handle> CalibrationModel::getEditableHandles() const
{
    std::vector<Handle> handles { Handle::fallTopLeft, Handle::fallTopRight, Handle::fallBottomRight, Handle::fallBottomLeft };

    if (! getCalibration().linkPlanes)
    {
        handles.push_back (Handle::keyTopLeft);
        handles.push_back (Handle::keyTopRight);
    }

    handles.push_back (Handle::keyBottomRight);
    handles.push_back (Handle::keyBottomLeft);
    return handles;
}

juce::Point<float> CalibrationModel::getHandlePosition (Handle handle) const
{
    if (handle == Handle::none)
        return {};

    auto current = getCalibration();
    return getCorner (current, handle);
}

void CalibrationModel::moveHandle (Handle handle, juce::Point<float> position)
{
    if (handle == Handle::none)
        return;

    modify ([&] (ProjectionCalibration& current)
    {
        const auto clamped = clampToScreen (position);
        getCorner (current, handle) = clamped;

        if (current.linkPlanes)
            if (const auto linked = getLinkedHandle (handle); linked != Handle::none)
                getCorner (current, linked) = clamped;
    });
}

void CalibrationModel::setSelectedHandle (Handle handle)
{
    selectedHandle = handle;
    sendChangeMessage();
}

void CalibrationModel::selectNextHandle()
{
    const auto handles = getEditableHandles();
    const auto found = std::find (handles.begin(), handles.end(), selectedHandle.load());
    const auto next = (found == handles.end() || std::next (found) == handles.end()) ? handles.front() : *std::next (found);
    setSelectedHandle (next);
}

void CalibrationModel::nudgeSelectedHandle (juce::Point<float> delta)
{
    const auto handle = selectedHandle.load();

    if (handle != Handle::none)
        moveHandle (handle, getHandlePosition (handle) + delta);
}

void CalibrationModel::setLinkPlanes (bool shouldLink)
{
    modify ([&] (ProjectionCalibration& current)
    {
        current.linkPlanes = shouldLink;

        // 連動を有効にしたら、鍵盤エリアの奥の辺を落下エリアの手前の辺にそろえる
        if (shouldLink)
        {
            current.keyCorners[0] = current.fallCorners[3];
            current.keyCorners[1] = current.fallCorners[2];
        }
    });

    const auto handle = selectedHandle.load();

    if (shouldLink && (handle == Handle::keyTopLeft || handle == Handle::keyTopRight))
        setSelectedHandle (getLinkedHandle (handle));
}

void CalibrationModel::transformAllCorners (const std::function<juce::Point<float> (juce::Point<float>)>& transform)
{
    modify ([&] (ProjectionCalibration& current)
    {
        for (auto* corners : { &current.fallCorners, &current.keyCorners })
            for (auto& corner : *corners)
                corner = transform (corner);
    });
}

// 反転・回転は、プロジェクタの映像の向きが変わったときに四隅の位置を物理的な位置に合わせ直すためのもの
void CalibrationModel::flipHorizontally()
{
    transformAllCorners ([] (juce::Point<float> p) { return juce::Point<float> (1.0f - p.x, p.y); });
}

void CalibrationModel::flipVertically()
{
    transformAllCorners ([] (juce::Point<float> p) { return juce::Point<float> (p.x, 1.0f - p.y); });
}

void CalibrationModel::rotate180()
{
    transformAllCorners ([] (juce::Point<float> p) { return juce::Point<float> (1.0f - p.x, 1.0f - p.y); });
}

void CalibrationModel::resetCorners()
{
    modify ([] (ProjectionCalibration& current)
    {
        const auto defaults = ProjectionCalibration::createDefault();
        current.fallCorners = defaults.fallCorners;
        current.keyCorners = defaults.keyCorners;
        current.linkPlanes = true;
    });
}

void CalibrationModel::squarePlane (bool fallPlane)
{
    modify ([&] (ProjectionCalibration& current)
    {
        auto& corners = fallPlane ? current.fallCorners : current.keyCorners;

        // 左端と上端は左上、右端は右上、下端は左下の角から取る
        const auto left = corners[0].x;
        const auto top = corners[0].y;
        const auto right = corners[1].x;
        const auto bottom = corners[3].y;

        corners = { juce::Point<float> { left, top }, { right, top }, { right, bottom }, { left, bottom } };

        // 連動しているときは、境目の辺をもう一方の面にも反映する
        if (current.linkPlanes)
        {
            if (fallPlane)
            {
                current.keyCorners[0] = current.fallCorners[3];
                current.keyCorners[1] = current.fallCorners[2];
            }
            else
            {
                current.fallCorners[3] = current.keyCorners[0];
                current.fallCorners[2] = current.keyCorners[1];
            }
        }
    });
}

void CalibrationModel::setBlackKeys (float offsetXLeft, float offsetXRight, float offsetY, float length)
{
    modify ([&] (ProjectionCalibration& current)
    {
        current.blackKeyOffsetXLeft = offsetXLeft;
        current.blackKeyOffsetXRight = offsetXRight;
        current.blackKeyOffsetY = offsetY;
        current.blackKeyLength = length;
    });
}

void CalibrationModel::setOutput (float brightness, float gamma)
{
    modify ([&] (ProjectionCalibration& current)
    {
        current.brightness = brightness;
        current.gamma = gamma;
    });
}

void CalibrationModel::setCalibrating (bool shouldCalibrate)
{
    calibrating = shouldCalibrate;
    sendChangeMessage();
}

juce::String CalibrationModel::getHandleLabel (Handle handle)
{
    switch (handle)
    {
        case Handle::fallTopLeft:     return "1";
        case Handle::fallTopRight:    return "2";
        case Handle::fallBottomRight: return "3";
        case Handle::fallBottomLeft:  return "4";
        case Handle::keyTopLeft:      return "4'";
        case Handle::keyTopRight:     return "3'";
        case Handle::keyBottomRight:  return "5";
        case Handle::keyBottomLeft:   return "6";
        case Handle::none:            break;
    }

    return {};
}

} // namespace pianolume
